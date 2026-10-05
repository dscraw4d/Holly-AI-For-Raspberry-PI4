#include "search_cache.h"
#include "news.h"
#include "search.h"
#include <stdint.h>
#include <string.h>
#include "mind.h"
#include "ethernet.h"
#include "holly.h"
#include "smp.h"
#include "display.h"
#include "dashboard.h"
#include "video.h"
#include "documents.h"
#include "clock.h"
#include "animation.h"
#include "sha256.h"
#include "pi_network.h"
#include "sdcard.h"
#include "vault.h"
#include "selftrain.h"
#include "http_server.h"
static void service(void);
#define GPIO 0xFE200000UL
#define UART 0xFE201000UL
#define REG(base, offset) (*(volatile uint32_t *)((base)+(offset)))
static void uart_init(void) {
    REG(UART,0x30)=0; /* disable */
    uint32_t select=REG(GPIO,0x04);
    select &= ~((7u<<12)|(7u<<15));
    select |= (4u<<12)|(4u<<15); /* GPIO14/15 ALT0 = PL011 */
    REG(GPIO,0x04)=select;
    REG(GPIO,0xE4) &= ~((3u<<28)|(3u<<30)); /* Pi 4 pulls off */
    REG(UART,0x44)=0x7FF;
    REG(UART,0x24)=26; REG(UART,0x28)=3; /* 48 MHz / 115200 baud */
    REG(UART,0x2C)=(3u<<5)|(1u<<4); /* 8 bits, FIFO */
    REG(UART,0x30)=(1u<<0)|(1u<<8)|(1u<<9);
}
static void putc(char c) {
    if(c=='\n') putc('\r');
    service();
    while(REG(UART,0x18) & (1u<<5)) {service();}
    REG(UART,0x00)=(uint32_t)c;
}
static char getc(void) {
    while(REG(UART,0x18) & (1u<<4)) {service();}
    return (char)(REG(UART,0x00)&255);
}
static void print(const char *s) { while(*s) putc(*s++); }
static int starts(const char *s,const char *prefix) {
    while(*prefix) if(*s++!=*prefix++) return 0;
    return 1;
}
static void hex32(uint32_t value) {
    const char digits[] = "0123456789ABCDEF";
    print("0x");
    for(int shift=28;shift>=0;shift-=4) putc(digits[(value>>shift)&15]);
}
static void dma_report(struct ether_device *device,int transmit) {
    struct ether_dma_snapshot s;
    if(ether_dma_snapshot(device,transmit,&s)) return;
    print(transmit ? "TX DMA cfg/ctrl/status: " : "RX DMA cfg/ctrl/status: ");
    hex32(s.configuration);putc(' ');hex32(s.control);putc(' ');hex32(s.status);putc('\n');
    print("  producer/consumer: ");hex32(s.producer);putc(' ');hex32(s.consumer);
    print(" ring size: ");hex32(s.buffer_size);putc('\n');
    print("  descriptor start/end: ");hex32((uint32_t)s.descriptor_start);
    putc(' ');hex32((uint32_t)s.descriptor_end);putc('\n');
}
static struct holly_video video;
static struct holly_display *display=&video.displays[0];
static struct holly_clock clock_device;
static int boot_splash_active;
static uint64_t boot_splash_start;
static enum holly_expression requested_expression=HOLLY_IDLE;
static uint64_t last_render_ticks;
static struct holly_mouth mouth;
static unsigned last_mouth_frame;
static unsigned mouth_animation_enabled=1;
static unsigned web_speech_active,web_reply_rendering;
static uint64_t web_speech_until;
static void web_speech(unsigned action,unsigned frame,void *ctx){
 (void)ctx;uint64_t ticks=0;
 if(action==5){holly_dashboard_avatar(frame);last_mouth_frame=~0u;return;}
 if(action==3){web_reply_rendering=1;return;}
 if(action==4){web_reply_rendering=0;return;}
 if(action==2){web_speech_active=0;(void)holly_mouth_frame(&mouth,0);mouth.manual=0;return;}
 if(holly_clock_read(&clock_device,&ticks))return;
 web_speech_active=1;web_speech_until=ticks+10000000u;
 if(mouth_animation_enabled)(void)holly_mouth_frame(&mouth,action==0?0:frame);
}
static void portrait_reply(const char *text,void *ctx){
 (void)ctx;if(mouth_animation_enabled&&!web_reply_rendering&&!web_speech_active)holly_mouth_reply(&mouth,text);
}
static unsigned video_enabled;
static int network_initialized;
static struct holly_sdcard sdcard;
static struct holly_vault vault;
static struct holly_documents documents;
static void bind_web_cache(void){struct holly_block_ops io;uint32_t first,span;if(!holly_documents_web_area(&documents,&io,&first,&span))(void)holly_search_cache_bind_disk(&io,first,span);}
static int document_chat_reply(struct holly_session *s,const char *q,unsigned flags,holly_emit_fn emit,void *ctx){return holly_documents_chat(&documents,s,q,flags,emit,ctx);}
static int document_search(const char *q,holly_emit_fn emit,void *ctx){(void)holly_documents_search(&documents,q,emit,ctx);return 0;}
static int document_script(const char *q,holly_emit_fn emit,void *ctx){(void)holly_documents_script_ask(&documents,q,emit,ctx);return 0;}
static int document_command(const char *q,holly_emit_fn emit,void *ctx){
 if(!strcmp(q,"doc format")||!strcmp(q,"doc init")){
  uint32_t first,span;
  if(holly_vault_reserve_documents_large(&vault,&first,&span)){emit("ERR cannot safely reserve document area. History must be healthy and unwrapped; run storagediag.\n",ctx);return 0;}
  int format=!strcmp(q,"doc format");
  if(format?holly_documents_format(&documents,&vault.io,first,span):holly_documents_mount_span(&documents,&vault.io,first,span)){emit("ERR document bank unavailable; run doc format once to reset documents for the new layout.\n",ctx);return 0;}
  bind_web_cache();
  emit(format?"DOC bank formatted. Old documents removed; re-upload originals. Memories and history retained.\n":"DOC bank mounted.\n",ctx);return 0;
 }
 return holly_documents_command(&documents,q,emit,ctx);
}
static int storage_initialized;
static const char *storage_stage="not attempted";
static int storage_result;
static int training_mount_result=-99;
static int memory_initialized;
static void service(void){
    uint64_t ticks;
    if(holly_clock_read(&clock_device,&ticks))return;
    if(network_initialized)holly_pi_network_poll(ticks);
    holly_training_tick(ticks);
    holly_assistant_tick(ticks);holly_news_tick(ticks);holly_search_tick(ticks);
    static uint64_t reading_tick;
    if(ticks-reading_tick>=100000u){reading_tick=ticks;holly_documents_read_step(&documents);}
    holly_dashboard_status(holly_pi_network_status,holly_pi_network_dhcp,
                           holly_pi_network_ip,(unsigned)storage_initialized,documents.ready);
    holly_web_status(holly_pi_network_status>0&&holly_pi_network_dhcp!=0,(unsigned)storage_initialized);
    if(web_speech_active&&ticks>=web_speech_until){web_speech_active=0;(void)holly_mouth_frame(&mouth,0);mouth.manual=0;}
    if(boot_splash_active||!video_enabled)return;
    if(video.mode||display->status!=HOLLY_DISPLAY_READY)return;
    /* No mailbox operations on animation ticks. Poll LAN before bounded
     * framebuffer writes; never repaint idle portraits or diagnostic patterns. */
    (void)holly_mouth_step(&mouth,ticks/1000u);
    if(last_mouth_frame==mouth.frame||
       (ticks>=last_render_ticks&&ticks-last_render_ticks<100000u))return;
    last_mouth_frame=mouth.frame;
    for(unsigned port=0;port<2;port++)if(video.displays[port].status==HOLLY_DISPLAY_READY)
        (void)holly_display_draw(&video.displays[port],HOLLY_SPEAKING,mouth.frame);
    if(holly_clock_read(&clock_device,&last_render_ticks))last_render_ticks=ticks;
}
extern char __stack_top[];
/* Direct access is supported only with MMU and D-cache disabled. */
static int direct_memory_ready(void){
    uint64_t el,sctlr;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    if((el>>2)==1u) __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    else if((el>>2)==2u) __asm__ volatile("mrs %0, sctlr_el2" : "=r"(sctlr));
    else return 0;
    return (sctlr&((1u<<0)|(1u<<2)))==0;
}
#ifndef HOLLY_VIDEO_DISABLED
static int framebuffer_exchange(uint32_t *request,size_t bytes,void *unused){
    (void)unused;uint32_t bus;
    if(!direct_memory_ready()||holly_pi4_ram_to_bus((uintptr_t)request,bytes,&bus))return -1;
    struct holly_mailbox mailbox=holly_pi4_mailbox();
    return holly_mailbox_property(&mailbox,bus);
}
static int framebuffer_map(const struct holly_framebuffer *fb,uint32_t **p,size_t *bytes,void *unused){
    (void)unused;uintptr_t physical;
    if(!direct_memory_ready()||fb->byte_size>32u*1024u*1024u||
       holly_pi4_fb_bus_to_arm(fb->bus_address,fb->byte_size,&physical)||
       physical<0x1000000u)return -1;
    *p=(uint32_t *)physical;*bytes=fb->byte_size;return 0;
}
#endif
static int video_start(void){
#ifdef HOLLY_VIDEO_DISABLED
    return -1;
#else
    if(!direct_memory_ready())return -1;
    const struct holly_display_ops ops={framebuffer_exchange,framebuffer_map,0};
    int result=holly_video_start(&video,&ops);
    display=&video.displays[video.active];video_enabled=result==0;
    return result;
#endif
}
static void display_draw(enum holly_expression expression,unsigned level){
    (void)level;requested_expression=expression;service();
}
static void ip_report(const uint8_t ip[4]){
    for(unsigned i=0;i<4;i++){if(i)putc('.');
        unsigned v=ip[i];if(v>=100)putc((char)('0'+v/100));if(v>=10)putc((char)('0'+(v/10)%10));putc((char)('0'+v%10));}
}
static void network_report(void){
    print("SSH network: ");
    if(holly_pi_network_status<0)print("hardware unavailable\n");
    else if(!network_initialized)print("disabled\n");
    else if(!holly_pi_network_dhcp)print("waiting for DHCP\n");
    else {print(holly_pi_network_dhcp==2?"DHCP ":"fallback ");ip_report(holly_pi_network_ip);print(" port 22\n");}
}
static void display_report(void){
    print("HDMI: ");print(holly_display_status_text(display->status));putc('\n');
    if(display->status==HOLLY_DISPLAY_READY){
        print("width/height/pitch: ");hex32(display->framebuffer.width);putc(' ');
        hex32(display->framebuffer.height);putc(' ');hex32(display->framebuffer.pitch_bytes);putc('\n');
        print("bus address/bytes/frames: ");hex32(display->framebuffer.bus_address);putc(' ');
        hex32(display->framebuffer.byte_size);putc(' ');hex32(display->frames);putc('\n');
    }
}
static int vault_read(uint32_t lba,uint8_t *sector,void *context){
    return holly_sdcard_read_sector((struct holly_sdcard *)context,lba,sector);
}
static int vault_write(uint32_t lba,const uint8_t *sector,void *context){
    return holly_sdcard_write_sector((struct holly_sdcard *)context,lba,sector);
}
static int persist_lessons(const struct lesson *saved,unsigned count,void *context){
    return holly_vault_save((struct holly_vault *)context,saved,count);
}
static int persist_chat_turn(const char *input,const char *output,void *context){
    return holly_vault_history_append((struct holly_vault *)context,input,output);
}
static int persistent_memory_add(const char *text,const char *source,unsigned confidence,
                                 uint32_t *id,void *context){
    return holly_vault_memory_add((struct holly_vault *)context,text,source,confidence,id);
}
static unsigned persistent_memory_count(void *context){
    return holly_vault_memory_count((struct holly_vault *)context);
}
static int persistent_memory_get(uint32_t id,struct holly_memory_item *item,void *context){
    return holly_vault_memory_get((struct holly_vault *)context,id,item);
}
static int persistent_memory_get_at(unsigned index,struct holly_memory_item *item,void *context){
    return holly_vault_memory_get_at((struct holly_vault *)context,index,item);
}
static int persistent_memory_update(uint32_t id,const char *text,const char *source,
                                    unsigned confidence,void *context){
    return holly_vault_memory_update((struct holly_vault *)context,id,text,source,confidence);
}
static int persistent_memory_forget(uint32_t id,void *context){
    return holly_vault_memory_forget((struct holly_vault *)context,id);
}
static const struct holly_memory_ops persistent_memory_ops={
    persistent_memory_add,persistent_memory_count,persistent_memory_get,
    persistent_memory_get_at,persistent_memory_update,persistent_memory_forget
};
static int restore_chat_turn(unsigned back,char *input,unsigned input_capacity,
                             char *output,unsigned output_capacity,void *context){
    return holly_vault_history_get((struct holly_vault *)context,back,input,input_capacity,
                                   output,output_capacity);
}
static unsigned saved_chat_turns(void *context){
    return holly_vault_history_count((struct holly_vault *)context);
}
static void storage_report(void){
    print("Storage: ");
    if(!storage_initialized){print("Holly Vault unavailable; lessons are RAM-only.\n");return;}
    print("Holly Vault online; lessons persist on the SD data partition.\n");
}
static void storage_start(void){
    uint8_t mbr[HOLLY_VAULT_SECTOR];uint32_t first,sectors;unsigned count=0;
    sdcard=holly_pi4_sdcard();
    storage_stage="SD initialization";
    storage_result=holly_sdcard_start(&sdcard);
    if(storage_result){storage_report();return;}
    storage_stage="read MBR";
    storage_result=holly_sdcard_read_sector(&sdcard,0,mbr);
    if(storage_result){storage_report();return;}
    storage_stage="find Vault partition";
    storage_result=holly_vault_find_partition(mbr,&first,&sectors);
    if(storage_result){storage_report();return;}
    storage_stage="validate expansion";
    int expanded=holly_vault_expand_image_partition(mbr,sdcard.sectors,&first,&sectors);
    if(expanded<0){storage_result=expanded;storage_report();return;}
    if(expanded>0){
        uint8_t verify[HOLLY_VAULT_SECTOR];
        storage_stage="write expanded MBR";
        storage_result=holly_sdcard_write_sector(&sdcard,0,mbr);
        if(storage_result){storage_report();return;}
        storage_stage="read back expanded MBR";
        storage_result=holly_sdcard_read_sector(&sdcard,0,verify);
        if(storage_result){storage_report();return;}
        storage_stage="verify expanded MBR";
        for(unsigned i=0;i<HOLLY_VAULT_SECTOR;i++)if(verify[i]!=mbr[i]){
            storage_result=-1;storage_report();return;
        }
    }
    struct holly_block_ops io={vault_read,vault_write,&sdcard};
    storage_stage="mount Vault";
    storage_result=holly_vault_mount(&vault,&io,first,sectors);
    if(storage_result){storage_report();return;}
    storage_stage="load lessons";
    storage_result=holly_vault_load(&vault,lessons,&count);
    if(storage_result){storage_report();return;}
    training_mount_result=holly_training_mount(&io);
    lesson_count=count;
    holly_set_storage_card_sectors(sdcard.sectors);
    if(!holly_vault_history_start(&vault))
        holly_set_history(persist_chat_turn,restore_chat_turn,saved_chat_turns,&vault);
    if(!holly_vault_memory_start(&vault,lessons,count)){
        memory_initialized=1;
        holly_set_memory(&persistent_memory_ops,&vault);
    }
    holly_set_document_chat(document_chat_reply);
    holly_set_document_commands(document_command,document_search,document_script);
    holly_reference_set_documents(holly_documents_visit_query,&documents);
    uint32_t cap=holly_vault_document_history_capacity(vault.sectors);
    if(vault.history_ready&&vault.history_capacity==cap){
        uint32_t first_doc=vault.first_lba+HOLLY_VAULT_HISTORY_START_SECTOR+cap*2u;
        (void)holly_documents_mount_span(&documents,&vault.io,first_doc,vault.sectors-HOLLY_VAULT_HISTORY_START_SECTOR-cap*2u);
    }
    bind_web_cache();
    mind_set_persistence(persist_lessons,&vault);storage_initialized=1;storage_stage="online";storage_result=0;
    print("Storage: Holly Vault online; restored ");
    if(count>=100)print("many");else {putc((char)('0'+count/10));putc((char)('0'+count%10));}
    print(" lesson(s), ");
    if(memory_initialized){
        if(holly_vault_memory_count(&vault)>=1000)print("many");
        else {unsigned memories=holly_vault_memory_count(&vault);putc((char)('0'+memories/100));
            putc((char)('0'+(memories/10)%10));putc((char)('0'+memories%10));}
        print(" reviewable memory item(s).\n");
    }else print("reviewable memory unavailable.\n");
}
/* Read-only diagnostics: never retry initialization or mutate the disk. */
static void diag_hex(holly_emit_fn emit,void *context,const char *label,uint32_t value){
    static const char digits[]="0123456789ABCDEF";
    char text[12]="0x00000000\n";
    for(unsigned i=0;i<8;i++)text[2+i]=digits[(value>>(28-4*i))&15u];
    emit(label,context);emit(text,context);
}
static void diag_int(holly_emit_fn emit,void *context,const char *label,int value){
    char text[16];unsigned n=0;uint32_t v=value<0?0u-(uint32_t)value:(uint32_t)value;
    do{text[n++]=(char)('0'+v%10);v/=10;}while(v);
    if(value<0)text[n++]='-';
    emit(label,context);
    while(n){char digit[2]={text[--n],0};emit(digit,context);}emit("\n",context);
}
static int hardware_diagnostics(const char *command,holly_emit_fn emit,void *context){
    if(starts(command,"networkdiag")){
        diag_int(emit,context,"Network status: ",holly_pi_network_status);
        diag_int(emit,context,"Network recovery attempts: ",(int)holly_pi_network_restarts);
        diag_int(emit,context,"Address mode (0 waiting,1 fallback,2 DHCP): ",holly_pi_network_dhcp);
        emit("IPv4 bytes: ",context);
        for(unsigned i=0;i<4;i++)diag_int(emit,context,"",holly_pi_network_ip[i]);
        return 0;
    }
    if(starts(command,"display ")){
        const char *arg=command+8;int result=-1;
        if(starts(arg,"off")){video_enabled=0;result=0;}
        else if(!video.initialized){result=video_start();}
        else if(starts(arg,"0")||starts(arg,"1")){
            result=holly_video_select(&video,(unsigned)(arg[0]-'0'));
            display=&video.displays[video.active];video_enabled=result==0;last_mouth_frame=~0u;
        }else if(starts(arg,"frame ")&&arg[6]>='1'&&arg[6]<='7'&&!arg[7]){
            result=holly_mouth_frame(&mouth,(unsigned)(arg[6]-'1'));
            if(!result){video.mode=0;video_enabled=1;last_mouth_frame=~0u;}
        }else if(starts(arg,"talk ")){
            holly_mouth_reply(&mouth,arg+5);video.mode=0;video_enabled=1;result=0;
        }else if(starts(arg,"animate off")){
            mouth_animation_enabled=0;result=holly_mouth_frame(&mouth,0);video.mode=0;last_mouth_frame=~0u;
        }else if(starts(arg,"animate on")){
            mouth_animation_enabled=1;result=holly_mouth_frame(&mouth,0);mouth.manual=0;video.mode=0;last_mouth_frame=~0u;
        }else if(starts(arg,"on")){result=holly_video_unblank(&video);video_enabled=result==0;}
        else if(starts(arg,"bars"))result=holly_video_pattern(&video,1);
        else if(starts(arg,"white"))result=holly_video_pattern(&video,2);
        else if(starts(arg,"red"))result=holly_video_pattern(&video,3);
        else if(starts(arg,"green"))result=holly_video_pattern(&video,4);
        else if(starts(arg,"blue"))result=holly_video_pattern(&video,5);
        else if(starts(arg,"face")){(void)holly_mouth_frame(&mouth,0);mouth.manual=0;result=holly_video_pattern(&video,0);video_enabled=result==0;last_mouth_frame=0;}
        diag_int(emit,context,"Display command result (0=completed): ",result);
        emit("Use display 0/1/on/off/bars/white/red/green/blue/face, frame 1..7, talk <text>, animate on/off. HDMI1 is opt-in.\n",context);
        return 0;
    }
    emit("Holly v0.49.29 hardware diagnostics\n",context);
    diag_int(emit,context,"Inference worker cores: ",holly_smp_workers());
    if(starts(command,"storagediag")){
        emit("Storage stage: ",context);emit(storage_stage,context);emit("\n",context);
        diag_int(emit,context,"Storage result: ",storage_result);
        diag_int(emit,context,"SD init stage: ",(int)sdcard.stage);
        diag_int(emit,context,"SD last error: ",sdcard.last_error);
        diag_int(emit,context,"SD last command: ",(int)sdcard.last_command);
        diag_int(emit,context,"SD ready: ",(int)sdcard.ready);
        diag_hex(emit,context,"SD sectors: ",sdcard.sectors);
        diag_hex(emit,context,"Capabilities: ",sdcard.capabilities);
        diag_hex(emit,context,"Host version: ",sdcard.version);
        diag_hex(emit,context,"State snapshot: ",sdcard.state);
        diag_hex(emit,context,"Clock snapshot: ",sdcard.control1);
        diag_hex(emit,context,"Interrupt snapshot: ",sdcard.interrupt);
        diag_hex(emit,context,"Card response: ",sdcard.response);
        diag_int(emit,context,"Model mount result (-99=not attempted): ",training_mount_result);
    }else{
        emit("Video bring-up: fullscreen RGB565 portrait; HDMI1 opt-in with display 1.\n",context);
        diag_int(emit,context,"Video drawing enabled: ",(int)video_enabled);
        diag_int(emit,context,"HTTP speech active: ",(int)web_speech_active);
        diag_int(emit,context,"Mouth frame (1-7): ",(int)mouth.frame+1);
        diag_int(emit,context,"Text mouth animation enabled: ",(int)mouth_animation_enabled);
        diag_int(emit,context,"Mouth events remaining: ",(int)(mouth.count-mouth.index));
        diag_int(emit,context,"Active firmware display index: ",(int)video.active);
        diag_int(emit,context,"Firmware display count: ",(int)video.count);
        for(unsigned port=0;port<2;port++){
            emit(port?"Port 1 framebuffer: ":"Port 0 framebuffer: ",context);
            emit(holly_display_status_text(video.displays[port].status),context);emit("\n",context);
            diag_int(emit,context,port?"Port 1 frames: ":"Port 0 frames: ",(int)video.displays[port].frames);
            diag_int(emit,context,port?"Port 1 JMC splash result: ":"Port 0 JMC splash result: ",video.splash_result[port]);
            diag_hex(emit,context,port?"Port 1 bus: ":"Port 0 bus: ",video.displays[port].framebuffer.bus_address);
        }
        diag_int(emit,context,"Count result: ",video.count_result);
        diag_int(emit,context,"Select result: ",video.select_result);
        emit("HDMI shows face only. Text mouth animation is bounded to ten redraws/second; Browser speech drives HDMI mouth frames; stale control returns to rest.\n",context);
        diag_int(emit,context,"Layer result: ",video.layer_result);
        diag_int(emit,context,"Monitor EDID result: ",video.edid_result);
        diag_int(emit,context,"Monitor EDID valid: ",(int)video.edid_valid);
        diag_int(emit,context,"Viewport reset result: ",video.offset_result);
        diag_int(emit,context,"Unblank result: ",video.unblank_result);
        diag_int(emit,context,"Mailbox locked after timeout: ",(int)video.poisoned);
        diag_int(emit,context,"Pattern (0=face,1=bars,2=white,3=red,4=green,5=blue): ",(int)video.mode);
        diag_hex(emit,context,"Last control tag: ",video.last_tag);
        diag_hex(emit,context,"Last control header: ",video.last_header);
        diag_hex(emit,context,"Last control tag reply: ",video.last_tag_reply);
        if(display->status==HOLLY_DISPLAY_READY){
            volatile const uint32_t *p=display->pixels;
            if(display->framebuffer.depth==16){
                volatile const uint16_t *p16=(volatile const uint16_t *)p;
                diag_hex(emit,context,"Pixel top-left: ",p16[0]);
                diag_hex(emit,context,"Pixel center: ",p16[(display->framebuffer.height/2u)*(display->framebuffer.pitch_bytes/2u)+display->framebuffer.width/2u]);
            }else{
                diag_hex(emit,context,"Pixel top-left: ",p[0]);
                diag_hex(emit,context,"Pixel center: ",p[(display->framebuffer.height/2u)*(display->framebuffer.pitch_bytes/4u)+display->framebuffer.width/2u]);
            }
        }
        emit("HDMI: ",context);emit(holly_display_status_text(display->status),context);emit("\n",context);
        diag_int(emit,context,"CPU direct memory ready: ",direct_memory_ready());
        diag_int(emit,context,"Frames drawn: ",(int)display->frames);
        diag_hex(emit,context,"Mailbox reply: ",display->request[1]);
        const unsigned tags[]={4,9,14,18,22,26,31};
        for(unsigned i=0;i<7;i++)diag_hex(emit,context,"Tag reply: ",display->request[tags[i]]);
        diag_int(emit,context,"Physical width: ",(int)display->request[5]);
        diag_int(emit,context,"Physical height: ",(int)display->request[6]);
        diag_int(emit,context,"Virtual width: ",(int)display->request[10]);
        diag_int(emit,context,"Virtual height: ",(int)display->request[11]);
        diag_int(emit,context,"Depth: ",(int)display->request[15]);
        diag_int(emit,context,"Pixel order: ",(int)display->request[19]);
        diag_int(emit,context,"Alpha mode: ",(int)display->request[23]);
        diag_hex(emit,context,"Framebuffer bus: ",display->request[27]);
        diag_hex(emit,context,"Framebuffer bytes: ",display->request[28]);
        diag_hex(emit,context,"Pitch: ",display->request[32]);
    }
    return 0;
}
static void holly_display_emit(const char *message,void *context){
    (void)context;
    display_draw(HOLLY_SPEAKING,180);
    print(message);
}
static void input(char *out,unsigned limit) {
    static int discard_lf;
    unsigned n=0;
    for(;;) {
        char c=getc();
        if(discard_lf){discard_lf=0;if(c=='\n')continue;}
        if(c=='\r'||c=='\n') { discard_lf=c=='\r';out[n]=0; print("\n"); return; }
        if(c==8||c==127) { if(n){n--;print("\b \b");} continue; }
        if(c>=32 && c<=126 && n+1<limit) {out[n++]=c;putc(c);}
    }
}
void kernel_main(void) {
    uart_init();
    clock_device=holly_pi4_clock();
    print("\nHOLLY AI LEARNING OS 0.49.29 | Pi 4 AArch64\n");
    /* Initialize persistent diagnostic staging before any GPU request. */
    video.count_result=video.select_result=video.offset_result=video.unblank_result=-99;
    video.layer_result=video.edid_result=-99;
    video.splash_result[0]=video.splash_result[1]=-99;

    storage_start();
    holly_set_diagnostics(hardware_diagnostics);
    if(direct_memory_ready()){
        (void)holly_pi_network_start();network_initialized=1;
        if(holly_pi_network_status<0)print("SSH network unavailable: hardware initialization failed.\n");
        else if(holly_pi_network_status==0)print("SSH network disabled: this build has no provisioned credentials.\n");
        else print("SSH network initialized on port 22. Waiting for wired link.\n");
    }
    holly_set_reply_observer(portrait_reply,0);
    holly_web_set_speech_observer(web_speech,0);
    if(video_start())print("HDMI unavailable; SSH remains enabled.\n");
    else print("HDMI0 fullscreen Holly portrait initialized; use display 1 for HDMI1.\n");
    display_report();
    (void)holly_smp_start();
    if(boot_splash_active){
        uint64_t now,previous=boot_splash_start;
        unsigned stalled=0;
        for(;;){
            if(holly_clock_read(&clock_device,&now)||now<boot_splash_start||
               now-boot_splash_start>=1800000u)break;
            if(now!=previous){stalled=0;previous=now;}
            else if(++stalled>=3000000u)break;
            service();
        }
        boot_splash_active=0;
        for(unsigned port=0;port<2;port++)if(video.displays[port].status==HOLLY_DISPLAY_READY)
            (void)holly_display_draw(&video.displays[port],HOLLY_IDLE,0);
    }
    print("Type displaydiag for display status, or storage/history for saved memory.\n");
    print("Alright, dudes. Holly kernel and memory engine online. Type help.\n");
    char line[320];
    struct holly_session console;
    holly_session_init(&console);
    for(;;) {
        display_draw(HOLLY_LISTENING,0);
        print("holly> "); input(line,sizeof(line));
        display_draw(HOLLY_THINKING,0);
        if(starts(line,"hash ")) {
            unsigned length=0;while(line[5+length])length++;
            uint8_t digest[32];
            if(holly_sha256_hash(line+5,length,digest))print("Hash failed.\n");
            else {const char digits[]="0123456789abcdef";
                for(unsigned i=0;i<32;i++){putc(digits[digest[i]>>4]);putc(digits[digest[i]&15]);}
                putc('\n');}
        } else if(starts(line,"cryptodiag")) {
            static const uint8_t expected[32]={
                0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
                0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
            uint8_t digest[32];
            print(holly_sha256_hash("abc",3,digest)==0&&holly_tag_equal(digest,expected,32) ?
                  "SHA-256 abc self-test passed.\n" : "SHA-256 self-test FAILED.\n");
        } else if(starts(line,"timediag")) {
            uint64_t ticks;
            if(holly_clock_read(&clock_device,&ticks))print("Timer snapshot failed.\n");
            else {print("Timer microseconds high/low: ");hex32((uint32_t)(ticks>>32));
                putc(' ');hex32((uint32_t)ticks);putc('\n');}
        } else if(starts(line,"displaydiag")||starts(line,"display ")||starts(line,"storagediag")) {
            (void)hardware_diagnostics(line,holly_display_emit,0);
        } else if(starts(line,"networkdiag")||starts(line,"ipdiag")) {
            network_report();
            if(starts(line,"networkdiag"))(void)hardware_diagnostics(line,holly_display_emit,0);
        } else if(starts(line,"netdiag")) {
            struct ether_device device=ether_pi4_device();
            print("GENET revision: ");hex32(ether_revision(&device));print("\n");
            unsigned address=0;uint32_t id=0;
            if(ether_phy_probe(&device,&address,&id)==0) {
                print("PHY address: ");hex32(address);print(" ID: ");hex32(id);print("\n");
            } else print("PHY not found / controller not ready.\n");
            dma_report(&device,0);dma_report(&device,1);
        } else if(starts(line,"help")) {
            print("Commands: hello, help, networkdiag, netdiag, displaydiag, timediag, cryptodiag, hash, teach, ask, history, repeat, exit.\n");
        } else holly_turn(&console,line,holly_display_emit,0);
        display_draw(HOLLY_IDLE,0);
    }
}
