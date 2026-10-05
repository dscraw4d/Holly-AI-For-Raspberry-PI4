#include "selftrain.h"
#include "language_model.h"
#include "training_seeds.h"
#include <string.h>
#define RECORDS 128u
#define MODEL_BYTES (HOLLY_LM_PARAMETERS*2u)
enum {IDLE,TRAIN,EVAL,IMPORT};
struct example {uint32_t split,id,revision;char text[257],source[97];uint8_t padding[2];};
struct training_state {
 uint32_t version,count,phase,paused,automatic,generation,previous_valid;
 uint32_t remaining,steps,record,position,eval_phase,eval_record,eval_position;
 uint32_t upload_bytes,accepted,rejected;
 uint64_t losses[6];uint32_t correct[6],tokens[6];
 int16_t active[HOLLY_LM_PARAMETERS],previous[HOLLY_LM_PARAMETERS],candidate[HOLLY_LM_PARAMETERS];
 struct example records[RECORDS];
};
static struct training_state state;
static struct model_store disk;
static unsigned initialized,dirty,promoting,previous_before_promotion;
static int16_t previous_backup[HOLLY_LM_PARAMETERS];
static uint64_t clock_now,last_activity,last_tick,next_auto;
_Static_assert(sizeof(struct training_state)<=MODEL_STORE_MAX,"training snapshot fits gap");
static void say(holly_emit_fn e,void *c,const char *s){e(s,c);}
static void number(holly_emit_fn e,void *c,uint64_t n){char b[24];unsigned p=23;b[p]=0;do{b[--p]=(char)('0'+n%10);n/=10;}while(n);say(e,c,b+p);}
static void copy_text(char *a,const char *b){memcpy(a,b,strlen(b)+1);}
static int equal(const char *a,const char *b){return strcmp(a,b)==0;}
static int starts(const char *a,const char *b){while(*b)if(*a++!=*b++)return 0;return 1;}
static int parse(const char **p,uint32_t *n){uint64_t v=0;const char *s=*p;if(*s<'0'||*s>'9')return -1;while(*s>='0'&&*s<='9'){v=v*10+(unsigned)(*s++-'0');if(v>UINT32_MAX)return -1;}*p=s;*n=(uint32_t)v;return 0;}
static int text_ok(const char *p,unsigned cap){unsigned n=0;for(;n<cap&&p[n];n++)if(p[n]!='\n'&&(p[n]<32||p[n]>126))return 0;return n>0&&n<cap;}
static void normalize(const char *in,char *out){
 unsigned n=0;int space=0;
 for(;*in;in++){char c=*in;if(c==' '||c=='\n'){space=n!=0;continue;}if(space)out[n++]=' ';space=0;out[n++]=c>='A'&&c<='Z'?(char)(c+32):c;}
 out[n]=0;
}
static int duplicate_text(const char *a,const char *b){char x[257],y[257];normalize(a,x);normalize(b,y);return equal(x,y);}
static unsigned token(char c){return !c?1:c=='\n'?2:(unsigned char)c-29;}
static void context_for(const char *s,unsigned position,uint8_t *context){memset(context,0,64);unsigned n=position<64?position:64;for(unsigned i=0;i<n;i++)context[64-n+i]=(uint8_t)token(s[position-n+i]);}
static void save(void){dirty=1;if(disk.ready&&!disk.busy)(void)model_store_save(&disk,&state);}
static int valid_state(void){
 if(state.version!=1||state.count<54||state.count>RECORDS||state.phase>IMPORT||state.paused>1||state.automatic>1||state.previous_valid>1||
 state.record>=state.count||state.position>256||state.eval_phase>5||state.eval_record>state.count||state.eval_position>256||state.remaining>100000||state.upload_bytes>MODEL_BYTES)return 0;
 for(unsigned i=0;i<state.count;i++)if(state.records[i].split>2||!text_ok(state.records[i].text,257)||!text_ok(state.records[i].source,97))return 0;
 if(state.phase==TRAIN&&!state.remaining)return 0;
 for(unsigned i=0;i<54;i++)if(state.records[i].id||state.records[i].revision||state.records[i].split!=seed_records[i].split||strcmp(state.records[i].text,seed_records[i].text))return 0;
 if(state.position>strlen(state.records[state.record].text))return 0;
 if(state.eval_record<state.count&&state.eval_position>strlen(state.records[state.eval_record].text))return 0;
 return 1;
}
void holly_training_init(void){
 memset(&state,0,sizeof(state));memset(&disk,0,sizeof(disk));
 state.version=1;holly_lm_default(state.active);memcpy(state.previous,state.active,MODEL_BYTES);memcpy(state.candidate,state.active,MODEL_BYTES);
 state.count=sizeof(seed_records)/sizeof(seed_records[0]);
 for(unsigned i=0;i<state.count;i++){state.records[i].split=seed_records[i].split;copy_text(state.records[i].text,seed_records[i].text);copy_text(state.records[i].source,"Holly authored seed corpus v0.34");}
 initialized=1;dirty=0;promoting=0;clock_now=last_activity=last_tick=0;next_auto=60000000;holly_lm_use(state.active);
}
int holly_training_mount(const struct holly_block_ops *io){
 holly_training_init();int result=model_store_open(&disk,io,&state,sizeof(state));
 if(result<0){holly_training_init();return -1;}
 if(result==1&&!valid_state()){holly_training_init();return -1;}
 if(result==1&&state.phase==IMPORT){state.phase=IDLE;state.upload_bytes=0;}
 holly_lm_use(state.active);if(result==0)save();return result;
}
void holly_training_activity(void){last_activity=clock_now;}
static void start_evaluation(void){state.phase=EVAL;state.eval_phase=state.eval_record=state.eval_position=0;memset(state.losses,0,sizeof(state.losses));memset(state.correct,0,sizeof(state.correct));memset(state.tokens,0,sizeof(state.tokens));}
static void start_round(unsigned count){memcpy(state.candidate,state.active,MODEL_BYTES);state.remaining=count;state.phase=TRAIN;state.paused=0;save();}
static int16_t bounded(int32_t n){return (int16_t)(n>32727?32727:n < -32727?-32727:n);}
static void train_token(void){
 while(state.records[state.record].split!=0){state.record=(state.record+1)%state.count;state.position=0;}
 struct example *r=state.records+state.record;uint8_t context[64];int32_t h[64],scores[98];context_for(r->text,state.position,context);
 unsigned target=token(r->text[state.position]);holly_lm_forward(state.candidate,context,h,scores);
 unsigned rival=target==1?2:1;for(unsigned j=1;j<98;j++)if(j!=target&&scores[j]>scores[rival])rival=j;
 if(scores[rival]+4096>scores[target]){
  for(unsigned j=0;j<64;j++){unsigned a=HOLLY_LM_OUTPUT_OFFSET+j*98+target,b=HOLLY_LM_OUTPUT_OFFSET+j*98+rival;int32_t delta=h[j]*8/4096;
   state.candidate[a]=bounded(state.candidate[a]+delta);state.candidate[b]=bounded(state.candidate[b]-delta);
  }
  state.candidate[HOLLY_LM_BIAS_OFFSET+target]=bounded(state.candidate[HOLLY_LM_BIAS_OFFSET+target]+8);
  state.candidate[HOLLY_LM_BIAS_OFFSET+rival]=bounded(state.candidate[HOLLY_LM_BIAS_OFFSET+rival]-8);
 }
 if(target==1){state.position=0;state.record=(state.record+1)%state.count;}else state.position++;
 state.steps++;if(!--state.remaining){start_evaluation();save();}else if(state.steps%256==0)save();
}
static int selected(unsigned phase,unsigned record){return phase<4?state.records[record].split==(phase<2?1u:2u):record<8;}
static void evaluate_token(void){
 unsigned phase=state.eval_phase;
 while(state.eval_record<state.count&&!selected(phase,state.eval_record)){state.eval_record++;state.eval_position=0;}
 if(state.eval_record==state.count){
  if(++state.eval_phase<6){state.eval_record=state.eval_position=0;save();return;}
  state.eval_phase=5;
  int pass=state.tokens[0]&&state.tokens[2]&&state.tokens[4]&&state.losses[1]<state.losses[0];
  for(unsigned k=0;k<6;k+=2)if(state.tokens[k]!=state.tokens[k+1]||state.losses[k+1]>state.losses[k]||state.correct[k+1]<state.correct[k])pass=0;
  if(pass){
   previous_before_promotion=state.previous_valid;memcpy(previous_backup,state.previous,MODEL_BYTES);
   memcpy(state.previous,state.active,MODEL_BYTES);memcpy(state.active,state.candidate,MODEL_BYTES);
   state.previous_valid=1;state.generation++;state.accepted++;promoting=disk.ready;
   /* Keep serving the prior model until the new checkpoint is durable. */
   if(promoting)holly_lm_use(state.previous);
  }
  else state.rejected++;
  state.phase=IDLE;next_auto=clock_now+60000000;save();return;
 }
 struct example *r=state.records+state.eval_record;uint8_t context[64];int32_t h[64],scores[98];context_for(r->text,state.eval_position,context);
 unsigned target=token(r->text[state.eval_position]);holly_lm_forward(phase%2?state.candidate:state.active,context,h,scores);
 unsigned best=1,rival=target==1?2:1;for(unsigned j=1;j<98;j++){if(scores[j]>scores[best])best=j;if(j!=target&&scores[j]>scores[rival])rival=j;}
 int32_t margin=scores[rival]-scores[target]+4096;
 if(margin>0)state.losses[phase]+=(uint32_t)margin;
 state.correct[phase]+=best==target;state.tokens[phase]++;
 if(target==1){state.eval_position=0;state.eval_record++;}else state.eval_position++;
}
void holly_training_tick(uint64_t now){
 if(!initialized)holly_training_init();
 clock_now=now;
 if(now<last_tick||now-last_tick<(disk.busy?2000u:20000u))return;
 last_tick=now;
 if(disk.busy){model_store_tick(&disk);if(!disk.busy){if(disk.error){state.automatic=0;state.paused=1;
    if(promoting){memcpy(state.active,state.previous,MODEL_BYTES);memcpy(state.previous,previous_backup,MODEL_BYTES);state.previous_valid=previous_before_promotion;state.generation--;state.accepted--;}
   }else dirty=0;
   holly_lm_use(state.active);promoting=0;}return;}
 if(state.paused||now<last_activity||now-last_activity<2000000u)return;
 if(state.phase==TRAIN)train_token();else if(state.phase==EVAL)evaluate_token();
 else if(state.phase==IDLE&&state.automatic&&now>=next_auto)start_round(2048);
}
static void hex(holly_emit_fn emit,void *ctx,const uint8_t *p,unsigned n){static const char digits[]="0123456789abcdef";char b[257];for(unsigned i=0;i<n;i++){b[2*i]=digits[p[i]>>4];b[2*i+1]=digits[p[i]&15];}b[2*n]=0;say(emit,ctx,b);}
static int unhex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
int holly_training_command(char *line,holly_emit_fn emit,void *ctx,const struct holly_memory_ops *memory,void *mc){
 if(!initialized)holly_training_init();
 if(equal(line,"train status")){
  say(emit,ctx,"Training phase ");number(emit,ctx,state.phase);say(emit,ctx," (0 idle, 1 training, 2 evaluating, 3 upload); paused ");number(emit,ctx,state.paused);
  say(emit,ctx,"; automatic ");number(emit,ctx,state.automatic);say(emit,ctx,"; generation ");number(emit,ctx,state.generation);say(emit,ctx,"; steps ");number(emit,ctx,state.steps);
  say(emit,ctx,"; records ");number(emit,ctx,state.count);say(emit,ctx,"; accepted/rejected ");number(emit,ctx,state.accepted);say(emit,ctx,"/");number(emit,ctx,state.rejected);
  say(emit,ctx,disk.busy?"; saving\n":disk.error?"; SAVE FAILED - paused\n":disk.ready?(dirty?"; unsaved\n":"; saved\n"):"; RAM ONLY - no model storage\n");
  for(unsigned i=0;i<6;i++){say(emit,ctx,"Gate ");number(emit,ctx,i);say(emit,ctx," margin ");number(emit,ctx,state.losses[i]);say(emit,ctx," correct/tokens ");number(emit,ctx,state.correct[i]);say(emit,ctx,"/");number(emit,ctx,state.tokens[i]);say(emit,ctx,"\n");}return 0;
 }
 if(starts(line,"train export ")){
  const char *p=line+13;uint32_t i;if(parse(&p,&i)||*p||i>=state.count){say(emit,ctx,"ERR record index\n");return 0;}
  struct example *r=state.records+i;say(emit,ctx,"EXAMPLE ");number(emit,ctx,r->split);say(emit,ctx," ");number(emit,ctx,r->id);say(emit,ctx," ");number(emit,ctx,r->revision);say(emit,ctx," ");hex(emit,ctx,(const uint8_t *)r->source,(unsigned)strlen(r->source));say(emit,ctx," ");
  unsigned n=(unsigned)strlen(r->text);for(unsigned off=0;off<n;off+=128)hex(emit,ctx,(const uint8_t *)r->text+off,n-off>128?128:n-off);say(emit,ctx,"\n");return 0;
 }
 if(starts(line,"model export ")){const char *p=line+13;uint32_t off;
  if(promoting){say(emit,ctx,"BUSY promotion checkpoint\n");return 0;}
  if(parse(&p,&off)||*p||off>=MODEL_BYTES){say(emit,ctx,"ERR offset\n");return 0;}
  unsigned n=MODEL_BYTES-off;if(n>128)n=128;say(emit,ctx,"WEIGHTS ");number(emit,ctx,off);say(emit,ctx," ");hex(emit,ctx,(const uint8_t *)state.active+off,n);say(emit,ctx,"\n");return 0;
 }
 if(disk.busy){say(emit,ctx,"BUSY saving; retry after train status reports saved.\n");return 0;}
 if(equal(line,"train pause")||equal(line,"train resume")||equal(line,"train auto on")||equal(line,"train auto off")){
  if(equal(line,"train auto on")&&!disk.ready){say(emit,ctx,"ERR automatic training requires model storage\n");return 0;}
  if(equal(line,"train pause"))state.paused=1;
  if(equal(line,"train resume"))state.paused=0;
  if(equal(line,"train auto on")){state.automatic=1;state.paused=0;next_auto=clock_now+60000000;}
  if(equal(line,"train auto off"))state.automatic=0;
  save();say(emit,ctx,"OK training controls queued; check train status\n");return 0;
 }
 if(equal(line,"train cancel")){state.phase=IDLE;state.automatic=0;state.paused=0;save();say(emit,ctx,"OK candidate cancelled\n");return 0;}
 if(equal(line,"train checkpoint")){save();say(emit,ctx,"OK checkpoint queued\n");return 0;}
 if(state.phase==IMPORT&&starts(line,"model import ")){
  const char *p=line+13;
  if(starts(p,"finish ")){p+=7;uint32_t checksum;if(parse(&p,&checksum)||*p||state.upload_bytes!=MODEL_BYTES||checksum!=model_crc(state.candidate,MODEL_BYTES)){say(emit,ctx,"ERR incomplete upload or CRC mismatch\n");return 0;}start_evaluation();state.paused=0;save();say(emit,ctx,"OK candidate queued for evaluation\n");return 0;}
  uint32_t off;uint8_t data[128];unsigned n=0;
  if(parse(&p,&off)||*p++!=' '||off!=state.upload_bytes){say(emit,ctx,"ERR upload offset\n");return 0;}
  while(*p&&n<128){int a=unhex(*p++);if(!*p){say(emit,ctx,"ERR hex\n");return 0;}int b=unhex(*p++);if(a<0||b<0){say(emit,ctx,"ERR hex\n");return 0;}data[n++]=(uint8_t)(a*16+b);}
  if(*p||!n||off+n>MODEL_BYTES){say(emit,ctx,"ERR upload size\n");return 0;}
  memcpy((uint8_t *)state.candidate+off,data,n);state.upload_bytes+=n;say(emit,ctx,"OK chunk\n");return 0;
 }
 if(state.phase!=IDLE){say(emit,ctx,"BUSY candidate; pause/resume/cancel or wait\n");return 0;}
 if(equal(line,"model import begin")){state.phase=IMPORT;state.upload_bytes=0;state.automatic=0;say(emit,ctx,"OK upload started\n");return 0;}
 if(equal(line,"model rollback")){
  if(!state.previous_valid){say(emit,ctx,"ERR no previous model\n");return 0;}
  memcpy(state.candidate,state.active,MODEL_BYTES);memcpy(state.active,state.previous,MODEL_BYTES);memcpy(state.previous,state.candidate,MODEL_BYTES);state.automatic=0;state.generation++;save();say(emit,ctx,"OK rollback queued; check train status\n");return 0;
 }
 if(starts(line,"train start ")){const char *p=line+12;uint32_t count;if(parse(&p,&count)||*p||!count||count>100000){say(emit,ctx,"ERR use train start 1..100000\n");return 0;}start_round(count);say(emit,ctx,"OK output-layer training queued\n");return 0;}
 if(starts(line,"train approve ")){
  const char *p=line+14;uint32_t id;struct holly_memory_item item;unsigned split;
  if(parse(&p,&id)||*p++!=' '||!memory||!memory->get||memory->get(id,&item,mc)){say(emit,ctx,"ERR use train approve MEMORY_ID train|valid|test\n");return 0;}
  if(equal(p,"train"))split=0;else if(equal(p,"valid"))split=1;else if(equal(p,"test"))split=2;else {say(emit,ctx,"ERR split\n");return 0;}
  if(!text_ok(item.text,257)||!text_ok(item.source,97)){say(emit,ctx,"ERR record needs printable ASCII\n");return 0;}
  for(unsigned i=0;i<state.count;i++)if(duplicate_text(item.text,state.records[i].text)||(id==state.records[i].id&&id)){say(emit,ctx,"ERR duplicate text or memory ID; remove old approval first\n");return 0;}
  if(state.count==RECORDS){say(emit,ctx,"ERR dataset full\n");return 0;}
  struct example *r=state.records+state.count++;memset(r,0,sizeof(*r));r->id=id;r->revision=item.revision;r->split=split;copy_text(r->text,item.text);copy_text(r->source,item.source);save();say(emit,ctx,"OK reviewed snapshot queued; later memory edits need reapproval\n");return 0;
 }
 if(starts(line,"train remove ")){const char *p=line+13;uint32_t id;if(parse(&p,&id)||*p||!id){say(emit,ctx,"ERR memory ID\n");return 0;}
  for(unsigned i=54;i<state.count;i++)if(state.records[i].id==id){for(unsigned j=i+1;j<state.count;j++)state.records[j-1]=state.records[j];state.count--;memset(state.records+state.count,0,sizeof(struct example));state.record=state.position=0;save();say(emit,ctx,"OK approval removed; learned weights unchanged\n");return 0;}
  say(emit,ctx,"ERR approval not found\n");return 0;
 }
 say(emit,ctx,"train status/export INDEX/approve ID train|valid|test/remove ID/start STEPS/pause/resume/cancel/checkpoint/auto on|off; model export OFFSET/import begin|OFFSET HEX|finish CRC/rollback\n");return 0;
}
