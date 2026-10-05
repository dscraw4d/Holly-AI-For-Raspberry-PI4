#include "vault.h"

static void zero(void *memory, unsigned bytes) {
    uint8_t *p=memory; for(unsigned i=0;i<bytes;i++)p[i]=0;
}
static void copy(void *to, const void *from, unsigned bytes) {
    uint8_t *d=to; const uint8_t *s=from;
    for(unsigned i=0;i<bytes;i++)d[i]=s[i];
}
static uint16_t get16(const uint8_t *p) { return (uint16_t)p[0]|((uint16_t)p[1]<<8); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p,uint32_t v) {
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
static int same(const uint8_t *a,const char *b,unsigned n) {
    for(unsigned i=0;i<n;i++) {
        if(a[i]!=(uint8_t)b[i])return 0;
    }
    return 1;
}
static uint32_t crc_start(void) { return 0xffffffffu; }
static uint32_t crc_bytes(uint32_t crc,const uint8_t *p,unsigned n) {
    for(unsigned i=0;i<n;i++) {
        crc^=p[i];
        for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^((crc&1u)?0xedb88320u:0);
    }
    return crc;
}
static uint32_t crc_finish(uint32_t crc) { return crc^0xffffffffu; }
static int read_at(struct holly_vault *v,uint32_t lba,uint8_t *sector) {
    if(!v||!v->io.read||lba>=v->sectors)return -1;
    return v->io.read(v->first_lba+lba,sector,v->io.context);
}
static int write_at(struct holly_vault *v,uint32_t lba,const uint8_t *sector) {
    if(!v||!v->io.write||lba>=v->sectors)return -1;
    return v->io.write(v->first_lba+lba,sector,v->io.context);
}
static unsigned bounded_length(const char *s,unsigned cap) {
    unsigned n=0;while(n<cap&&s[n])n++;return n;
}
static int page_header(const uint8_t *h,uint32_t page,uint32_t pages,
                       uint32_t *version,uint32_t *sequence,unsigned *count,
                       uint32_t *crc) {
    uint32_t v=0;
    if(same(h,"HLYVLT01",8)&&get32(h+8)==1u)v=1;
    else if(same(h,"HLYVLT02",8)&&get32(h+8)==2u)v=2;
    if(!v||get32(h+28)!=page||page>=pages)return -1;
    unsigned item_count=get32(h+20);
    if(item_count>MIND_SLOTS)return -1;
    *version=v;*sequence=get32(h+12);*count=item_count;*crc=get32(h+24);return 0;
}
static int parse_record(const uint8_t *sector,unsigned slot,struct lesson *lesson) {
    if(!same(sector,"HLYLESS1",8)||sector[8]!=(uint8_t)slot||sector[9]!=1)return -1;
    uint16_t qlen=get16(sector+10),alen=get16(sector+12);
    if(!qlen||qlen>=QUESTION_SIZE||!alen||alen>=ANSWER_SIZE)return -1;
    for(unsigned i=0;i<QUESTION_SIZE;i++)lesson->question[i]=0;
    for(unsigned i=0;i<ANSWER_SIZE;i++)lesson->answer[i]=0;
    copy(lesson->question,sector+16,qlen);
    copy(lesson->answer,sector+16+QUESTION_SIZE,alen);
    return 0;
}
int holly_vault_find_partition(const uint8_t *mbr,uint32_t *first_lba,uint32_t *sectors) {
    if(!mbr||!first_lba||!sectors||mbr[510]!=0x55||mbr[511]!=0xaa)return -1;
    for(unsigned i=0;i<4;i++) {
        const uint8_t *entry=mbr+446+i*16;
        if(entry[4]!=HOLLY_VAULT_PARTITION_TYPE)continue;
        uint32_t first=get32(entry+8),length=get32(entry+12);
        if(!first||!length)return -1;
        *first_lba=first;*sectors=length;return 0;
    }
    return -1;
}
int holly_vault_expand_image_partition(uint8_t *mbr,uint32_t card_sectors,
                                       uint32_t *first_lba,uint32_t *sectors) {
    if(!mbr||!first_lba||!sectors||mbr[510]!=0x55||mbr[511]!=0xaa||
       !same(mbr+440,"HLY2",4))return -1;
    const uint8_t *boot=mbr+446,*data=mbr+462;
    if(boot[4]!=0x0cu||get32(boot+8)!=2048u||
       data[4]!=HOLLY_VAULT_PARTITION_TYPE||get32(data+8)!=458752u)return -1;
    for(unsigned i=2;i<4;i++) {
        const uint8_t *entry=mbr+446u+i*16u;
        for(unsigned j=0;j<16;j++)if(entry[j])return -1;
    }
    uint32_t first=get32(data+8),old_length=get32(data+12);
    if(!old_length||first>=card_sectors)return -1;
    uint32_t available=card_sectors-first;
    if(old_length>available)return -1;
    *first_lba=first;*sectors=old_length;
    if(available<=old_length)return 0;
    put32(mbr+462+12,available);*sectors=available;return 1;
}
int holly_vault_mount(struct holly_vault *v,const struct holly_block_ops *io,
                      uint32_t first_lba,uint32_t sectors) {
    if(!v||!io||!io->read||!io->write||!sectors||sectors<HOLLY_VAULT_PAGE_SECTORS*2u||
       sectors<HOLLY_VAULT_HISTORY_START_SECTOR+HOLLY_VAULT_HISTORY_RECORD_SECTORS||
       first_lba>UINT32_MAX-sectors)return -1;
    zero(v,sizeof(*v));v->io=*io;v->first_lba=first_lba;v->sectors=sectors;
    v->page_count=sectors/HOLLY_VAULT_PAGE_SECTORS;
    if(v->page_count>HOLLY_VAULT_JOURNAL_PAGES)v->page_count=HOLLY_VAULT_JOURNAL_PAGES;
    v->history_capacity=(sectors-HOLLY_VAULT_HISTORY_START_SECTOR)/HOLLY_VAULT_HISTORY_RECORD_SECTORS;
    v->current_page=UINT32_MAX;v->ready=1;return 0;
}
static int read_snapshot(struct holly_vault *v,uint32_t page,uint32_t expected_version,
                         struct lesson *saved,unsigned *count,uint32_t *sequence,
                         uint32_t *actual_version) {
    uint8_t header[HOLLY_VAULT_SECTOR],sector[HOLLY_VAULT_SECTOR];
    uint32_t version,expected_crc;unsigned saved_count;
    uint32_t page_limit=expected_version==2u?2u:HOLLY_VAULT_JOURNAL_PAGES;
    if(read_at(v,page*HOLLY_VAULT_PAGE_SECTORS,header)||
       page_header(header,page,page_limit,&version,sequence,&saved_count,&expected_crc)||
       (expected_version&&version!=expected_version))return -1;
    struct lesson candidate[MIND_SLOTS];uint32_t crc=crc_start();
    for(unsigned i=0;i<MIND_SLOTS;i++) {
        if(read_at(v,page*HOLLY_VAULT_PAGE_SECTORS+1u+i,sector))return -1;
        crc=crc_bytes(crc,sector,sizeof(sector));
        if(i<saved_count) {if(parse_record(sector,i,&candidate[i]))return -1;}
        else if(!same(sector,"HLYLESS1",8)||sector[9]!=0)return -1;
    }
    if(crc_finish(crc)!=expected_crc)return -1;
    for(unsigned i=0;i<saved_count;i++)saved[i]=candidate[i];
    *count=saved_count;*actual_version=version;return 0;
}
int holly_vault_load(struct holly_vault *v,struct lesson *out,unsigned *count) {
    if(!v||!v->ready||!out||!count)return -1;
    struct lesson candidate[MIND_SLOTS],best[MIND_SLOTS];
    unsigned candidate_count=0,best_count=0,found=0;uint32_t sequence=0,best_sequence=0;
    uint32_t actual_version=0,best_page=UINT32_MAX;
    /* A committed v2 snapshot means the old lesson pages are now reusable.
     * Prefer it exclusively so stale v1 pages can never roll memory backward. */
    for(uint32_t page=0;page<2u;page++) {
        if(read_snapshot(v,page,2,candidate,&candidate_count,&sequence,&actual_version))continue;
        if(!found||sequence>best_sequence) {
            for(unsigned i=0;i<candidate_count;i++)best[i]=candidate[i];
            best_count=candidate_count;best_sequence=sequence;best_page=page;found=1;
        }
    }
    if(found) {
        v->format_version=2;v->page_count=2;
    } else {
        for(uint32_t page=0;page<HOLLY_VAULT_JOURNAL_PAGES;page++) {
            if(read_snapshot(v,page,1,candidate,&candidate_count,&sequence,&actual_version))continue;
            if(!found||sequence>best_sequence) {
                for(unsigned i=0;i<candidate_count;i++)best[i]=candidate[i];
                best_count=candidate_count;best_sequence=sequence;best_page=page;found=1;
            }
        }
        v->format_version=1;v->page_count=HOLLY_VAULT_JOURNAL_PAGES;
    }
    if(found) {
        for(unsigned i=0;i<best_count;i++)out[i]=best[i];
        v->current_page=best_page;v->current_sequence=best_sequence;
    } else {v->current_page=UINT32_MAX;v->current_sequence=0;}
    *count=found?best_count:0;return 0;
}
static int write_snapshot(struct holly_vault *v,uint32_t page,uint32_t sequence,
                          uint32_t version,const struct lesson *saved,unsigned count) {
    uint8_t sector[HOLLY_VAULT_SECTOR],header[HOLLY_VAULT_SECTOR];uint32_t crc=crc_start();
    for(unsigned i=0;i<MIND_SLOTS;i++) {
        zero(sector,sizeof(sector));copy(sector,"HLYLESS1",8);sector[8]=(uint8_t)i;
        if(i<count) {
            unsigned q=bounded_length(saved[i].question,QUESTION_SIZE);
            unsigned a=bounded_length(saved[i].answer,ANSWER_SIZE);
            if(!q||q>=QUESTION_SIZE||!a||a>=ANSWER_SIZE)return -1;
            put16(sector+10,(uint16_t)q);put16(sector+12,(uint16_t)a);sector[9]=1;
            copy(sector+16,saved[i].question,q);copy(sector+16+QUESTION_SIZE,saved[i].answer,a);
        }
        crc=crc_bytes(crc,sector,sizeof(sector));
        if(write_at(v,page*HOLLY_VAULT_PAGE_SECTORS+1u+i,sector))return -1;
    }
    zero(header,sizeof(header));copy(header,version==2u?"HLYVLT02":"HLYVLT01",8);
    put32(header+8,version);put32(header+12,sequence);put32(header+20,count);
    put32(header+24,crc_finish(crc));put32(header+28,page);
    /* Commit the snapshot only after every data sector has been written. */
    return write_at(v,page*HOLLY_VAULT_PAGE_SECTORS,header);
}
int holly_vault_save(struct holly_vault *v,const struct lesson *saved,unsigned count) {
    if(!v||!v->ready||(!saved&&count)||count>MIND_SLOTS)return -1;
    uint32_t version=v->format_version==2u?2u:1u;
    uint32_t pages=version==2u?2u:HOLLY_VAULT_JOURNAL_PAGES;
    uint32_t page=v->current_page==UINT32_MAX?0u:(v->current_page+1u)%pages;
    uint32_t sequence=v->current_sequence+1u;if(!sequence)sequence=1;
    if(write_snapshot(v,page,sequence,version,saved,count))return -1;
    v->format_version=version;v->page_count=pages;
    v->current_page=page;v->current_sequence=sequence;return 0;
}
static uint32_t memory_lba(uint32_t slot,unsigned copy_index) {
    return HOLLY_VAULT_MEMORY_START_SECTOR+
           slot*HOLLY_VAULT_MEMORY_RECORD_COPIES+copy_index;
}
static int memory_decode(const uint8_t *record,uint32_t slot,
                         struct holly_memory_item *item,uint32_t *revision,
                         unsigned *state) {
    if(!same(record,"HLYMEM01",8)||record[9]!=1u||
       get32(record+12)!=slot+1u||
       get32(record+508)!=crc_finish(crc_bytes(crc_start(),record,508)))return -1;
    unsigned record_state=record[8];uint32_t rev=get32(record+16);
    if(!rev||(record_state!=1u&&record_state!=2u))return -1;
    zero(item,sizeof(*item));item->id=slot+1u;item->revision=rev;
    if(record_state==1u) {
        unsigned confidence=get16(record+10),text_length=get16(record+20);
        unsigned source_length=get16(record+22);
        if(confidence>100u||!text_length||text_length>=HOLLY_MEMORY_TEXT_SIZE||
           !source_length||source_length>=HOLLY_MEMORY_SOURCE_SIZE||
           24u+text_length+source_length>508u)return -1;
        item->confidence=confidence;
        copy(item->text,record+24,text_length);
        copy(item->source,record+24+text_length,source_length);
    } else if(get16(record+10)||get16(record+20)||get16(record+22))return -1;
    *revision=rev;*state=record_state;return 0;
}
static void memory_encode(uint8_t *record,uint32_t slot,unsigned state,
                          const struct holly_memory_item *item,uint32_t revision) {
    zero(record,HOLLY_VAULT_SECTOR);copy(record,"HLYMEM01",8);
    record[8]=(uint8_t)state;record[9]=1;put32(record+12,slot+1u);
    put32(record+16,revision);
    if(state==1u) {
        unsigned text_length=bounded_length(item->text,HOLLY_MEMORY_TEXT_SIZE);
        unsigned source_length=bounded_length(item->source,HOLLY_MEMORY_SOURCE_SIZE);
        put16(record+10,(uint16_t)item->confidence);
        put16(record+20,(uint16_t)text_length);put16(record+22,(uint16_t)source_length);
        copy(record+24,item->text,text_length);
        copy(record+24+text_length,item->source,source_length);
    }
    put32(record+508,crc_finish(crc_bytes(crc_start(),record,508)));
}
static int memory_write(struct holly_vault *v,uint32_t slot,unsigned copy_index,
                        unsigned state,const struct holly_memory_item *item,
                        uint32_t revision) {
    uint8_t record[HOLLY_VAULT_SECTOR],verify[HOLLY_VAULT_SECTOR];
    struct holly_memory_item decoded;uint32_t decoded_revision=0;unsigned decoded_state=0;
    memory_encode(record,slot,state,item,revision);
    if(write_at(v,memory_lba(slot,copy_index),record)||
       read_at(v,memory_lba(slot,copy_index),verify)||
       memory_decode(verify,slot,&decoded,&decoded_revision,&decoded_state)||
       decoded_revision!=revision||decoded_state!=state)return -1;
    if(state==1u&&(!same((const uint8_t *)decoded.text,item->text,
                         bounded_length(item->text,HOLLY_MEMORY_TEXT_SIZE))||
                   !same((const uint8_t *)decoded.source,item->source,
                         bounded_length(item->source,HOLLY_MEMORY_SOURCE_SIZE))||
                   decoded.confidence!=item->confidence))return -1;
    return 0;
}
static int memory_erase_copy(struct holly_vault *v,uint32_t slot,unsigned copy_index) {
    uint8_t blank[HOLLY_VAULT_SECTOR],verify[HOLLY_VAULT_SECTOR];
    zero(blank,sizeof(blank));
    if(write_at(v,memory_lba(slot,copy_index),blank)||
       read_at(v,memory_lba(slot,copy_index),verify))return -1;
    for(unsigned i=0;i<sizeof(verify);i++)if(verify[i])return -1;
    return 0;
}
int holly_vault_memory_start(struct holly_vault *v,const struct lesson *saved,
                             unsigned count) {
    if(!v||!v->ready||(!saved&&count)||count>MIND_SLOTS)return -1;
    if(v->format_version!=2u) {
        /* Keep the last v1 snapshot intact until the v2 commit sector lands.
         * Once committed, only pages 0 and 1 are needed for lessons; pages
         * 2 through 63 become the independent memory journal. */
        uint32_t page=v->current_page==0u?1u:0u;
        uint32_t sequence=v->current_sequence+1u;if(!sequence)sequence=1;
        if(write_snapshot(v,page,sequence,2,saved,count))return -1;
        struct lesson verified[MIND_SLOTS];unsigned verified_count=0;
        uint32_t verified_sequence=0,version=0;
        if(read_snapshot(v,page,2,verified,&verified_count,&verified_sequence,&version)||
           verified_count!=count||verified_sequence!=sequence)return -1;
        v->format_version=2;v->page_count=2;
        v->current_page=page;v->current_sequence=sequence;
    }
    v->memory_count=0;
    v->memory_ready=0;
    zero(v->memory_revision,sizeof(v->memory_revision));
    zero(v->memory_copy,sizeof(v->memory_copy));
    zero(v->memory_active,sizeof(v->memory_active));
    zero(v->memories,sizeof(v->memories));
    for(uint32_t slot=0;slot<HOLLY_VAULT_MEMORY_SLOT_COUNT;slot++) {
        uint8_t records[2][HOLLY_VAULT_SECTOR];
        struct holly_memory_item items[2];uint32_t revisions[2]={0,0};
        unsigned states[2]={0,0};int valid[2]={0,0},read_error[2]={0,0};
        for(unsigned copy_index=0;copy_index<2;copy_index++) {
            if(read_at(v,memory_lba(slot,copy_index),records[copy_index])) {
                read_error[copy_index]=1;continue;
            }
            if(!memory_decode(records[copy_index],slot,&items[copy_index],
                              &revisions[copy_index],&states[copy_index]))valid[copy_index]=1;
        }
        int chosen=-1;
        if(valid[0]&&valid[1])
            chosen=(int32_t)(revisions[1]-revisions[0])>0?1:0;
        else if(valid[0])chosen=0;
        else if(valid[1])chosen=1;
        else if(read_error[0]||read_error[1])return -1;
        if(chosen<0)continue;
        v->memory_revision[slot]=revisions[chosen];
        v->memory_copy[slot]=(uint8_t)chosen;
        if(states[chosen]==1u) {
            v->memory_active[slot]=1;
            v->memories[slot]=items[chosen];
            v->memory_count++;
        } else if(!read_error[1u-(unsigned)chosen]) {
            /* Retry clearing a forgotten value if power failed after its
             * tombstone was committed but before its older copy was erased. */
            (void)memory_erase_copy(v,slot,1u-(unsigned)chosen);
        }
    }
    v->memory_ready=1;return 0;
}
unsigned holly_vault_memory_count(const struct holly_vault *v) {
    return v&&v->memory_ready?v->memory_count:0;
}
int holly_vault_memory_get(struct holly_vault *v,uint32_t id,
                           struct holly_memory_item *item) {
    if(!v||!v->memory_ready||!item||!id||id>HOLLY_VAULT_MEMORY_SLOT_COUNT||
       !v->memory_active[id-1u])return -1;
    *item=v->memories[id-1u];return 0;
}
int holly_vault_memory_get_at(struct holly_vault *v,unsigned index,
                              struct holly_memory_item *item) {
    if(!v||!v->memory_ready||!item||index>=v->memory_count)return -1;
    for(uint32_t slot=0;slot<HOLLY_VAULT_MEMORY_SLOT_COUNT;slot++) {
        if(!v->memory_active[slot])continue;
        if(!index--){*item=v->memories[slot];return 0;}
    }
    return -1;
}
int holly_vault_memory_add(struct holly_vault *v,const char *text,
                           const char *source,unsigned confidence,uint32_t *id_out) {
    if(!v||!v->memory_ready||!text||!source||!id_out||confidence>100u)return -1;
    unsigned text_length=bounded_length(text,HOLLY_MEMORY_TEXT_SIZE);
    unsigned source_length=bounded_length(source,HOLLY_MEMORY_SOURCE_SIZE);
    if(!text_length||text_length>=HOLLY_MEMORY_TEXT_SIZE||!source_length||
       source_length>=HOLLY_MEMORY_SOURCE_SIZE)return -2;
    uint32_t slot=HOLLY_VAULT_MEMORY_SLOT_COUNT;
    for(uint32_t i=0;i<HOLLY_VAULT_MEMORY_SLOT_COUNT;i++)
        if(!v->memory_active[i]){slot=i;break;}
    if(slot==HOLLY_VAULT_MEMORY_SLOT_COUNT)return -3;
    struct holly_memory_item item;zero(&item,sizeof(item));
    item.id=slot+1u;item.confidence=confidence;
    copy(item.text,text,text_length);copy(item.source,source,source_length);
    uint32_t revision=v->memory_revision[slot]+1u;if(!revision)revision=1;
    unsigned copy_index=v->memory_revision[slot]?1u-v->memory_copy[slot]:0u;
    if(memory_write(v,slot,copy_index,1,&item,revision))return -4;
    item.revision=revision;v->memories[slot]=item;v->memory_revision[slot]=revision;
    v->memory_copy[slot]=(uint8_t)copy_index;v->memory_active[slot]=1;
    v->memory_count++;*id_out=item.id;return 0;
}
int holly_vault_memory_update(struct holly_vault *v,uint32_t id,const char *text,
                              const char *source,unsigned confidence) {
    if(!v||!v->memory_ready||!text||!source||confidence>100u||!id||
       id>HOLLY_VAULT_MEMORY_SLOT_COUNT||!v->memory_active[id-1u])return -1;
    unsigned text_length=bounded_length(text,HOLLY_MEMORY_TEXT_SIZE);
    unsigned source_length=bounded_length(source,HOLLY_MEMORY_SOURCE_SIZE);
    if(!text_length||text_length>=HOLLY_MEMORY_TEXT_SIZE||!source_length||
       source_length>=HOLLY_MEMORY_SOURCE_SIZE)return -2;
    uint32_t slot=id-1u;struct holly_memory_item item=v->memories[slot];
    zero(item.text,sizeof(item.text));zero(item.source,sizeof(item.source));
    copy(item.text,text,text_length);copy(item.source,source,source_length);
    item.confidence=confidence;
    uint32_t revision=v->memory_revision[slot]+1u;if(!revision)revision=1;
    unsigned copy_index=1u-v->memory_copy[slot];
    if(memory_write(v,slot,copy_index,1,&item,revision))return -3;
    item.revision=revision;v->memories[slot]=item;v->memory_revision[slot]=revision;
    v->memory_copy[slot]=(uint8_t)copy_index;return 0;
}
int holly_vault_memory_forget(struct holly_vault *v,uint32_t id) {
    if(!v||!v->memory_ready||!id||id>HOLLY_VAULT_MEMORY_SLOT_COUNT||
       !v->memory_active[id-1u])return -1;
    uint32_t slot=id-1u,revision=v->memory_revision[slot]+1u;
    if(!revision)revision=1;
    unsigned old_copy=v->memory_copy[slot],copy_index=1u-old_copy;
    if(memory_write(v,slot,copy_index,2,0,revision))return -2;
    v->memory_revision[slot]=revision;v->memory_copy[slot]=(uint8_t)copy_index;
    v->memory_active[slot]=0;v->memory_count--;zero(&v->memories[slot],sizeof(v->memories[slot]));
    /* Commit the tombstone first; clear the previous sector so forgotten text
     * is no longer present in either sector exposed by Holly's memory store. */
    return memory_erase_copy(v,slot,old_copy)?-3:0;
}
const char *holly_vault_status(const struct holly_vault *v) {
    return v&&v->ready?"online":"offline";
}

static uint32_t history_crc(uint8_t *record) {
    uint8_t old0=record[16],old1=record[17],old2=record[18],old3=record[19];
    record[16]=record[17]=record[18]=record[19]=0;
    uint32_t crc=crc_finish(crc_bytes(crc_start(),record,
                                      HOLLY_VAULT_HISTORY_RECORD_SECTORS*HOLLY_VAULT_SECTOR));
    record[16]=old0;record[17]=old1;record[18]=old2;record[19]=old3;
    return crc;
}
uint32_t holly_vault_document_history_capacity(uint32_t sectors){
 if(sectors<=HOLLY_VAULT_HISTORY_START_SECTOR)return 0;
 uint32_t maximum=(sectors-HOLLY_VAULT_HISTORY_START_SECTOR)/2u;
 return maximum>2097152u?524288u:maximum/4u;
}
static int history_header_decode(const uint8_t *header,const struct holly_vault *v,
                                uint32_t *sequence,uint32_t *next,uint32_t *count,
                                uint32_t *total,uint32_t *capacity) {
    uint32_t cap=get32(header+28);
    uint32_t maximum=(v->sectors-HOLLY_VAULT_HISTORY_START_SECTOR)/2u;
    if(!same(header,"HLYHST01",8)||get32(header+8)!=1u||
       !cap||(cap!=holly_vault_document_history_capacity(v->sectors)&&cap!=maximum&&(maximum<=HOLLY_DOCUMENT_SECTORS/2u||cap!=maximum-HOLLY_DOCUMENT_SECTORS/2u))||
       get32(header+508)!=crc_finish(crc_bytes(crc_start(),header,508)))return -1;
    uint32_t n=get32(header+16),c=get32(header+20),t=get32(header+24);
    if(n>=cap||c>cap||t<c)return -1;
    *capacity=cap;
    *sequence=get32(header+12);*next=n;*count=c;*total=t;return 0;
}
static int history_header_write(struct holly_vault *v,unsigned slot,uint32_t sequence,
                                uint32_t next,uint32_t count,uint32_t total) {
    uint8_t header[HOLLY_VAULT_SECTOR];zero(header,sizeof(header));copy(header,"HLYHST01",8);
    put32(header+8,1);put32(header+12,sequence);put32(header+16,next);
    put32(header+20,count);put32(header+24,total);put32(header+28,v->history_capacity);
    put32(header+508,crc_finish(crc_bytes(crc_start(),header,508)));
    return write_at(v,HOLLY_VAULT_JOURNAL_PAGES*HOLLY_VAULT_PAGE_SECTORS+slot,header);
}
int holly_vault_history_start(struct holly_vault *v) {
    if(!v||!v->ready||!v->history_capacity)return -1;
    uint8_t headers[2][HOLLY_VAULT_SECTOR];
    uint32_t sequence[2]={0,0},next[2]={0,0},count[2]={0,0},total[2]={0,0};
    int valid[2]={0,0};uint32_t capacities[2]={0,0};unsigned suspect=0;
    for(unsigned i=0;i<2;i++) {
        if(read_at(v,HOLLY_VAULT_JOURNAL_PAGES*HOLLY_VAULT_PAGE_SECTORS+i,headers[i])){suspect=1;continue;}
        unsigned zero=1,erased=1;
        for(unsigned j=0;j<HOLLY_VAULT_SECTOR;j++){if(headers[i][j])zero=0;if(headers[i][j]!=255)erased=0;}
        if(!zero&&!erased)suspect=1;
        if(!history_header_decode(headers[i],v,&sequence[i],&next[i],&count[i],&total[i],&capacities[i]))valid[i]=1;
    }
    int chosen=-1;
    if(valid[0]&&valid[1]){
        /* A partly completed shrink must never expand history onto documents. */
        if(capacities[0]!=capacities[1])chosen=capacities[1]<capacities[0]?1:0;
        else chosen=sequence[1]>sequence[0]?1:0;
    }
    else if(valid[0])chosen=0;else if(valid[1])chosen=1;
    if(chosen<0) {
        if(suspect)return -1; /* Never erase recognised but damaged history metadata. */
        if(history_header_write(v,0,1,0,0,0))return -1;
        v->history_sequence=1;v->history_next=0;v->history_count=0;
        v->history_total=0;v->history_meta_slot=0;
    } else {
        v->history_capacity=capacities[chosen];
        v->history_sequence=sequence[chosen];v->history_next=next[chosen];
        v->history_count=count[chosen];v->history_total=total[chosen];
        v->history_meta_slot=(unsigned)chosen;
    }
    v->history_ready=1;return 0;
}
unsigned holly_vault_history_count(const struct holly_vault *v) {
    return v&&v->history_ready?v->history_count:0;
}
int holly_vault_history_append(struct holly_vault *v,const char *input,const char *output) {
    if(!v||!v->history_ready||!input||!output||v->history_total==UINT32_MAX)return -1;
    unsigned input_length=bounded_length(input,HOLLY_VAULT_HISTORY_INPUT);
    unsigned output_length=bounded_length(output,HOLLY_VAULT_HISTORY_OUTPUT);
    if(!input_length||input_length>=HOLLY_VAULT_HISTORY_INPUT||
       output_length>=HOLLY_VAULT_HISTORY_OUTPUT)return -2;
    uint8_t record[HOLLY_VAULT_HISTORY_RECORD_SECTORS*HOLLY_VAULT_SECTOR];
    zero(record,sizeof(record));copy(record,"HLYCHAT1",8);
    put32(record+8,v->history_total+1u);put16(record+12,(uint16_t)input_length);
    put16(record+14,(uint16_t)output_length);
    copy(record+24,input,input_length);copy(record+24+input_length,output,output_length);
    put32(record+16,history_crc(record));
    uint32_t lba=HOLLY_VAULT_HISTORY_START_SECTOR+v->history_next*HOLLY_VAULT_HISTORY_RECORD_SECTORS;
    for(unsigned i=0;i<HOLLY_VAULT_HISTORY_RECORD_SECTORS;i++)
        if(write_at(v,lba+i,record+i*HOLLY_VAULT_SECTOR))return -3;
    uint32_t next=v->history_next+1u;if(next==v->history_capacity)next=0;
    uint32_t count=v->history_count<v->history_capacity?v->history_count+1u:v->history_count;
    uint32_t total=v->history_total+1u,sequence=v->history_sequence+1u;
    if(!sequence)sequence=1;
    unsigned slot=1u-v->history_meta_slot;
    if(history_header_write(v,slot,sequence,next,count,total))return -4;
    v->history_next=next;v->history_count=count;v->history_total=total;
    v->history_sequence=sequence;v->history_meta_slot=slot;return 0;
}
int holly_vault_history_get(struct holly_vault *v,unsigned back,char *input,
                            unsigned input_capacity,char *output,unsigned output_capacity) {
    if(!v||!v->history_ready||!input||!output||back>=v->history_count)return -1;
    uint32_t slot=(uint32_t)(((uint64_t)v->history_next+v->history_capacity-1u-back)%v->history_capacity);
    uint32_t lba=HOLLY_VAULT_HISTORY_START_SECTOR+slot*HOLLY_VAULT_HISTORY_RECORD_SECTORS;
    uint8_t record[HOLLY_VAULT_HISTORY_RECORD_SECTORS*HOLLY_VAULT_SECTOR];
    for(unsigned i=0;i<HOLLY_VAULT_HISTORY_RECORD_SECTORS;i++)
        if(read_at(v,lba+i,record+i*HOLLY_VAULT_SECTOR))return -2;
    uint32_t expected=v->history_total-back,sequence=get32(record+8),crc=get32(record+16);
    if(!same(record,"HLYCHAT1",8)||sequence!=expected||history_crc(record)!=crc)return -3;
    uint16_t ilen=get16(record+12),olen=get16(record+14);
    if(!ilen||ilen>=HOLLY_VAULT_HISTORY_INPUT||olen>=HOLLY_VAULT_HISTORY_OUTPUT||
       24u+ilen+olen>sizeof(record)||input_capacity<=ilen||output_capacity<=olen)return -4;
    for(unsigned i=0;i<ilen;i++)input[i]=(char)record[24+i];
    input[ilen]=0;
    for(unsigned i=0;i<olen;i++)output[i]=(char)record[24+ilen+i];
    output[olen]=0;
    return 0;
}

int holly_vault_reserve_documents(struct holly_vault *v,uint32_t *first){
 if(!v||!first||!v->history_ready)return -1;
 uint32_t maximum=(v->sectors-HOLLY_VAULT_HISTORY_START_SECTOR)/2u;
 if(maximum<=HOLLY_DOCUMENT_SECTORS/2u+1024u)return -2;
 uint32_t target=maximum-HOLLY_DOCUMENT_SECTORS/2u;
 if(v->history_capacity!=target){
  /* Migration needs an unwrapped journal entirely below the proposed boundary. */
  if(v->history_capacity!=maximum||v->history_total!=v->history_count||
     v->history_next!=v->history_count||v->history_count>=target)return -3;
  v->history_capacity=target;
  for(unsigned i=0;i<2;i++){
   unsigned slot=1u-v->history_meta_slot;uint32_t seq=v->history_sequence+1;
   if(!seq||history_header_write(v,slot,seq,v->history_next,v->history_count,v->history_total)){v->history_ready=0;return -4;}
   uint8_t h[512];uint32_t sq,n,c,t,cap;
   if(read_at(v,HOLLY_VAULT_JOURNAL_PAGES*HOLLY_VAULT_PAGE_SECTORS+slot,h)||
      history_header_decode(h,v,&sq,&n,&c,&t,&cap)||sq!=seq||n!=v->history_next||c!=v->history_count||t!=v->history_total||cap!=target){v->history_ready=0;return -4;}
   v->history_meta_slot=slot;v->history_sequence=seq;
  }
 }
 *first=v->first_lba+HOLLY_VAULT_HISTORY_START_SECTOR+target*2u;return 0;
}

int holly_vault_reserve_documents_large(struct holly_vault*v,uint32_t*first,uint32_t*span){
 if(!v||!first||!span||!v->history_ready)return -1;
 uint32_t target=holly_vault_document_history_capacity(v->sectors);
 if(!target||v->sectors-HOLLY_VAULT_HISTORY_START_SECTOR-target*2u<8192u)return -2;
 if(v->history_capacity!=target){
  if(v->history_total!=v->history_count||v->history_next!=v->history_count||v->history_count>=target)return -3;
  v->history_capacity=target;
 }
 /* Refresh both copies, also after reboot following a partial reservation. */
 for(unsigned i=0;i<2;i++){
   unsigned slot=1u-v->history_meta_slot;uint32_t seq=v->history_sequence+1u;
   if(!seq||history_header_write(v,slot,seq,v->history_next,v->history_count,v->history_total)){v->history_ready=0;return -4;}
   uint8_t h[512];uint32_t sq,n,c,t,cap;
   if(read_at(v,HOLLY_VAULT_JOURNAL_PAGES*HOLLY_VAULT_PAGE_SECTORS+slot,h)||history_header_decode(h,v,&sq,&n,&c,&t,&cap)||sq!=seq||cap!=target||n!=v->history_next||c!=v->history_count||t!=v->history_total){v->history_ready=0;return -4;}
   v->history_meta_slot=slot;v->history_sequence=seq;
 }
 *first=v->first_lba+HOLLY_VAULT_HISTORY_START_SECTOR+target*2u;
 *span=v->sectors-HOLLY_VAULT_HISTORY_START_SECTOR-target*2u;
 return 0;
}
