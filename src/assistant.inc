#include <string.h>
/* Original bounded clock, persistent ship notes and alarm records.
 * Included in holly.c to share its existing Vault memory operations. */
static uint64_t assistant_us,clock_anchor_us,alarm_tick_us;
static uint32_t clock_epoch,alarm_ids[16];
static unsigned alarm_scan,alarm_seen;static uint64_t alarm_scan_us;
static void assistant_reset_index(void){memset(alarm_ids,0,sizeof alarm_ids);alarm_scan=alarm_seen=0;alarm_scan_us=0;}
static void alarm_track(unsigned id){for(unsigned i=0;i<16;i++)if(alarm_ids[i]==id)return;for(unsigned i=0;i<16;i++)if(!alarm_ids[i]){alarm_ids[i]=id;return;}}
static int clock_valid,clock_offset,clock_browser;
static void assistant_number(char*out,unsigned value,unsigned cap){
 char digits[12];unsigned n=0;do{digits[n++]=(char)('0'+value%10);value/=10;}while(value);
 while(n){char one[2]={digits[--n],0};append_text(out,one,cap);}
}
static uint32_t assistant_now(void){
 if(!clock_valid)return 0;
 uint64_t delta=assistant_us>=clock_anchor_us?(assistant_us-clock_anchor_us)/1000000u:0;
 return (uint32_t)(clock_epoch+delta);
}
uint32_t holly_clock_utc(void){return assistant_now();}
int holly_clock_seed(uint32_t epoch,int offset,unsigned browser){
 if(epoch<946684800u||epoch>4102444800u||offset < -840||offset>840)return -1;
 clock_epoch=epoch;clock_offset=offset;clock_anchor_us=assistant_us;clock_valid=1;clock_browser=(int)browser;return 0;
}
static int assistant_store_ready(void){return memory_ops&&memory_ops->count&&memory_ops->get_at&&memory_ops->add&&memory_ops->update;}
static int alarm_record(const struct holly_memory_item*item,char state,uint32_t *epoch,const char **label){
 if(!same(item->source,"holly alarm")||item->text[0]!=state||item->text[1]!=' ')return 0;
 unsigned number;const char *end;
 if(parse_number(item->text+2,&number,&end)||*end!=' ')return 0;
 *epoch=number;*label=end+1;return 1;
}
void holly_assistant_tick(uint64_t us){
 assistant_us=us;if(!assistant_store_ready()||!memory_ops->get)return;
 unsigned count=memory_ops->count(memory_context);
 if(count!=alarm_seen){alarm_seen=count;alarm_scan=0;}
 /* Vault records are in RAM. Bound startup indexing to 16 records per turn. */
 if(us-alarm_scan_us>=20000u){alarm_scan_us=us;
  for(unsigned n=0;n<16&&alarm_scan<count;n++,alarm_scan++){
   struct holly_memory_item item;uint32_t due;const char*label;
   if(!memory_ops->get_at(alarm_scan,&item,memory_context)&&(alarm_record(&item,'A',&due,&label)||alarm_record(&item,'F',&due,&label)))alarm_track(item.id);
  }
 }
 if(!clock_valid||us-alarm_tick_us<1000000u)return;
 alarm_tick_us=us;uint32_t now=assistant_now();
 for(unsigned i=0;i<16;i++){if(!alarm_ids[i])continue;struct holly_memory_item item;uint32_t due;const char*label;
  if(memory_ops->get(alarm_ids[i],&item,memory_context)||(!alarm_record(&item,'A',&due,&label)&&!alarm_record(&item,'F',&due,&label))){alarm_ids[i]=0;continue;}
  if(item.text[0]=='A'&&due<=now){item.text[0]='F';(void)memory_ops->update(item.id,item.text,item.source,100,memory_context);}
 }
}
int holly_alarm_poll(char *out,unsigned cap){
 out[0]=0;if(!assistant_store_ready()||!memory_ops->get)return 0;
 for(unsigned i=0;i<16;i++){if(!alarm_ids[i])continue;struct holly_memory_item item;uint32_t due;const char*label;
  if(memory_ops->get(alarm_ids[i],&item,memory_context)||!alarm_record(&item,'F',&due,&label))continue;
  assistant_number(out,item.id,cap);append_text(out,"|Emergency! There is an emergency going on... it\'s still going on. Alarm: ",cap);append_text(out,label,cap);return 1;
 }return 0;
}
int holly_alarm_ack(unsigned id){
 if(!assistant_store_ready()||!memory_ops->get)return -1;
 struct holly_memory_item item;uint32_t due;const char *label;
 if(memory_ops->get(id,&item,memory_context)||!alarm_record(&item,'F',&due,&label))return -1;
 item.text[0]='D';int result=memory_ops->update(item.id,item.text,item.source,100,memory_context);if(!result)for(unsigned i=0;i<16;i++)if(alarm_ids[i]==id)alarm_ids[i]=0;return result;
}
static void assistant_answer(struct holly_session*s,const char*text,holly_emit_fn emit,void*ctx){
 copy(s->last_answer,text,sizeof s->last_answer);s->expression=HOLLY_SPEAKING;
 say(emit,ctx,"Holly: ");say(emit,ctx,text);say(emit,ctx,"\n");
}
static void assistant_date(char*out,unsigned cap,int only_date){
 int64_t local=(int64_t)assistant_now()+(int64_t)clock_offset*60;
 unsigned days=(unsigned)(local/86400),seconds=(unsigned)(local%86400),year=1970,month=0;
 unsigned remain=days;
 for(;;){unsigned leap=year%4==0&&(year%100!=0||year%400==0),n=365+leap;if(remain<n)break;remain-=n;year++;}
 const unsigned sizes[]={31,28,31,30,31,30,31,31,30,31,30,31};
 while(month<11){unsigned n=sizes[month]+(month==1&&year%4==0&&(year%100!=0||year%400==0));if(remain<n)break;remain-=n;month++;}
 const char *months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
 const char *weekdays[]={"Thursday","Friday","Saturday","Sunday","Monday","Tuesday","Wednesday"};
 if(!only_date){unsigned h=seconds/3600,m=seconds/60%60;char b[6]={(char)('0'+h/10),(char)('0'+h%10),':',(char)('0'+m/10),(char)('0'+m%10),0};append_text(out,b,cap);append_text(out," on ",cap);}
 append_text(out,weekdays[days%7],cap);append_text(out,", ",cap);assistant_number(out,remain+1,cap);append_text(out," ",cap);append_text(out,months[month],cap);append_text(out," ",cap);assistant_number(out,year,cap);
}
static unsigned assistant_small_number(const char*text,const char**end){
 unsigned n;if(!parse_number(text,&n,end))return n;
 const char *words[]={"zero","one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen","twenty","thirty","sixty"};
 for(unsigned i=0;i<sizeof words/sizeof words[0];i++){unsigned k=text_length(words[i],20);if(starts_no_case(text,words[i])&&(!text[k]||text[k]==' ')){*end=text+k;return i<21?i:i==21?30:60;}}
 *end=text;return 0;
}
static int assistant_reply(struct holly_session*s,const char*input,unsigned trusted,holly_emit_fn emit,void*ctx){
 const char *raw=input;
 if(starts_no_case(raw,"holly ")||starts_no_case(raw,"hilly "))raw+=6;
 else if(starts_no_case(raw,"holly, ")||starts_no_case(raw,"hilly, "))raw+=7;
 char q[320];unsigned at=0;while(raw[at]&&at+1<sizeof q){q[at]=lower(raw[at]);at++;}q[at]=0;
 while(at&&(q[at-1]=='?'||q[at-1]=='!'||q[at-1]=='.'))q[--at]=0;
 if(s->note_pending||(starts(q,"take a note")&&(!q[11]||q[11]==' '||q[11]==':'||q[11]==','))||(starts(q,"take note")&&(!q[9]||q[9]==' '||q[9]==':'||q[9]==','))){
  if(same(q,"cancel")||same(q,"cancel note")){s->note_pending=0;assistant_answer(s,"Note cancelled.",emit,ctx);return 1;}
  const char *text=raw;
  if(!s->note_pending){text=raw+(starts(q,"take a note")?11:9);while(*text==' '||*text==':'||*text==',')text++;
   if(!*text){s->note_pending=1;assistant_answer(s,"Go ahead. What shall I note down?",emit,ctx);return 1;}}
  if(!assistant_store_ready()){assistant_answer(s,"My note storage isn't available. Check storage over SSH.",emit,ctx);return 1;}
  if(text_length(text,HOLLY_MEMORY_TEXT_SIZE)>=HOLLY_MEMORY_TEXT_SIZE){assistant_answer(s,"That note is too long. Keep it within 256 characters.",emit,ctx);return 1;}
  uint32_t id;if(memory_ops->add(text,"holly note",100,&id,memory_context)){assistant_answer(s,"I couldn't save that note. Please check storage.",emit,ctx);return 1;}
  s->note_pending=0;char reply[100]="Saved note ";assistant_number(reply,id,sizeof reply);append_text(reply,". I'll keep that on the card.",sizeof reply);assistant_answer(s,reply,emit,ctx);return 1;
 }
 if(same(q,"recall my notes")||same(q,"recall notes")||same(q,"read my notes")||same(q,"notes")||same(q,"show my notes")){
  if(!assistant_store_ready()){assistant_answer(s,"My note storage isn't available.",emit,ctx);return 1;}
  char reply[HOLLY_REPLY_SIZE]="Your latest ship notes: ";unsigned shown=0,total=0,count=memory_ops->count(memory_context);
  while(count){struct holly_memory_item item;if(memory_ops->get_at(--count,&item,memory_context)||!same(item.source,"holly note"))continue;total++;
   if(shown>=5)continue;
   assistant_number(reply,item.id,sizeof reply);append_text(reply,": ",sizeof reply);append_text(reply,item.text,sizeof reply);append_text(reply,". ",sizeof reply);shown++;}
  if(!total)copy(reply,"You haven't saved any ship notes yet.",sizeof reply);
  else if(total>shown)append_text(reply,"Use recall note ID for an older note.",sizeof reply);
  assistant_answer(s,reply,emit,ctx);return 1;
 }
 if(starts(q,"recall note ")){
  unsigned id;const char *end;struct holly_memory_item item;
  if(parse_number(q+12,&id,&end)||*end||!assistant_store_ready()||!memory_ops->get||memory_ops->get(id,&item,memory_context)||!same(item.source,"holly note"))assistant_answer(s,"I haven't found a ship note with that number.",emit,ctx);
  else assistant_answer(s,item.text,emit,ctx);
  return 1;
 }
 if(starts(q,"clock set ")){
  if(!trusted){assistant_answer(s,"Manual clock settings require SSH. The browser synchronises automatically.",emit,ctx);return 1;}
  unsigned epoch,zone=0;int sign=1;const char *end,*after;
  if(parse_number(q+10,&epoch,&end)){assistant_answer(s,"Use clock set UNIX_SECONDS OFFSET_MINUTES.",emit,ctx);return 1;}
  if(*end){if(*end++!=' '){assistant_answer(s,"Use clock set UNIX_SECONDS OFFSET_MINUTES.",emit,ctx);return 1;}if(*end=='-'){sign=-1;end++;}
   if(parse_number(end,&zone,&after)||*after||zone>840){assistant_answer(s,"UTC offset must be between -840 and 840 minutes.",emit,ctx);return 1;}}
  if(holly_clock_seed(epoch,(int)zone*sign,0))assistant_answer(s,"That clock setting is outside the supported range.",emit,ctx);
  else assistant_answer(s,"Clock set. I'll use that local UTC offset.",emit,ctx);
  return 1;
 }
 int date=same(q,"what is today's date")||same(q,"what is today's date?")||same(q,"what is todays date")||same(q,"what is todays date?")||same(q,"what day is it")||same(q,"what day is it?")||same(q,"what is the date")||same(q,"what is the date?")||same(q,"what\'s today\'s date")||same(q,"today\'s date")||same(q,"todays date");
 int time=same(q,"what time is it")||same(q,"what time is it?")||same(q,"what is the time")||same(q,"what is the time?")||same(q,"tell me the time")||same(q,"clock status")||same(q,"what\'s the time")||same(q,"what time")||same(q,"time");
 if(date||time){
  if(!clock_valid){assistant_answer(s,"I haven't synchronised the time since boot. Open my web page, or set the clock over SSH.",emit,ctx);return 1;}
  char reply[220]= "It's ";assistant_date(reply,sizeof reply,date);
  if(same(q,"clock status"))append_text(reply,clock_browser?". Set from the browser computer's clock.":". Set manually over SSH.",sizeof reply);
  assistant_answer(s,reply,emit,ctx);return 1;
 }
 if(same(q,"alarm status")||same(q,"recall my alarms")||same(q,"alarms")){
  char reply[HOLLY_REPLY_SIZE]="";if(!assistant_store_ready()){assistant_answer(s,"Alarm storage isn't available.",emit,ctx);return 1;}
  unsigned count=memory_ops->count(memory_context),shown=0;
  for(unsigned i=0;i<count&&shown<8;i++){struct holly_memory_item item;uint32_t due;const char *label;
   if(memory_ops->get_at(i,&item,memory_context)||(!alarm_record(&item,'A',&due,&label)&&!alarm_record(&item,'F',&due,&label)))continue;
   append_text(reply,"Alarm ",sizeof reply);assistant_number(reply,item.id,sizeof reply);append_text(reply,item.text[0]=='F'?" is due: ":" is set: ",sizeof reply);append_text(reply,label,sizeof reply);append_text(reply,". ",sizeof reply);shown++;}
  if(!shown)copy(reply,"No active or pending alarms.",sizeof reply);
  assistant_answer(s,reply,emit,ctx);return 1;
 }
 if(starts(q,"set an alarm ")||starts(q,"set alarm ")){
  if(!clock_valid||!assistant_store_ready()){assistant_answer(s,"I need a synchronised clock and working storage before I can save an alarm.",emit,ctx);return 1;}
  const char *arg=q+(starts(q,"set an alarm ")?13:10),*end;unsigned delta=0;uint32_t now=assistant_now(),due=0;
  if(starts(arg,"in ")){
   arg+=3;unsigned n=assistant_small_number(arg,&end);while(*end==' ')end++;
   if(starts(end,"second"))delta=n;else if(starts(end,"minute"))delta=n*60;else if(starts(end,"hour"))delta=n*3600;
   if(!n||n>1440||!delta||delta>86400){assistant_answer(s,"Try set an alarm in 5 minutes, up to 24 hours.",emit,ctx);return 1;}due=now+delta;
  }else if(starts(arg,"for ")||starts(arg,"at ")){
   arg+=starts(arg,"for ")?4:3;unsigned h=assistant_small_number(arg,&end),m=0;
   if(*end==':'){const char *after;if(parse_number(end+1,&m,&after)){assistant_answer(s,"Use set an alarm for 07:30.",emit,ctx);return 1;}end=after;}
   while(*end==' ')end++;
   if(starts(end,"am")||starts(end,"pm")){if(!h||h>12){assistant_answer(s,"Use an hour from 1 to 12 with AM or PM.",emit,ctx);return 1;}h=h%12+(starts(end,"pm")?12:0);end+=2;}
   if((*end&&*end!=' '&&*end!='.')||h>23||m>59){assistant_answer(s,"Use set an alarm for 07:30 or set an alarm for 7 AM.",emit,ctx);return 1;}
   int64_t local=(int64_t)now+clock_offset*60;due=(uint32_t)(local/86400*86400+(h*60+m)*60-clock_offset*60);if(due<=now)due+=86400;
  }else {assistant_answer(s,"Use set an alarm in 5 minutes or set an alarm for 07:30.",emit,ctx);return 1;}
  char text[HOLLY_MEMORY_TEXT_SIZE]="A ";assistant_number(text,due,sizeof text);append_text(text," ",sizeof text);append_text(text,raw,sizeof text);
  unsigned count=memory_ops->count(memory_context),active=0;uint32_t reusable=0;
  for(unsigned i=0;i<count;i++){struct holly_memory_item item;if(!memory_ops->get_at(i,&item,memory_context)&&same(item.source,"holly alarm")){if(item.text[0]=='A'||item.text[0]=='F')active++;else if(item.text[0]=='D')reusable=item.id;}}
  if(active>=16){assistant_answer(s,"I've already got 16 active or pending alarms.",emit,ctx);return 1;}
  uint32_t id=reusable;int result=reusable?memory_ops->update(id,text,"holly alarm",100,memory_context):memory_ops->add(text,"holly alarm",100,&id,memory_context);
  if(result)assistant_answer(s,"I couldn't save that alarm.",emit,ctx);
  else {alarm_track(id);char reply[180]="Alarm ";assistant_number(reply,id,sizeof reply);append_text(reply," saved. Keep the browser open for my spoken announcement.",sizeof reply);assistant_answer(s,reply,emit,ctx);}return 1;
 }
 return 0;
}
static void normalize_kryten(char *text,unsigned cap){
 if(starts_no_case(text,"teach ")||starts_no_case(text,"remember "))return;
 const char *aliases[]={"crichton","crighton","kryton","kriton","cryten","cryton","criten","krytan","krytten","krayton","cry ten","kry ten","cry ton","kry ton","cr eye ten","kriten","crayten","crayton","krytonne","krypton"};
 char out[320];unsigned n=0;
 for(unsigned i=0;text[i]&&n+1<sizeof out;){
  unsigned match=0;
  if(!i||!word_char(text[i-1]))for(unsigned a=0;a<sizeof aliases/sizeof aliases[0];a++){
   unsigned k=0;while(aliases[a][k]&&text[i+k]&&lower(text[i+k])==aliases[a][k])k++;
   if(!aliases[a][k]&&!word_char(text[i+k])){match=k;break;}
  }
  if(match){if(n+6>=sizeof out)break;memcpy(out+n,"Kryten",6);n+=6;i+=match;}
  else out[n++]=text[i++];
 }out[n]=0;copy(text,out,cap);
}
int holly_browser_clock(const char*text){
 unsigned epoch,zone;int sign=1;const char *end,*after;
 if(parse_number(text,&epoch,&end)||*end++!=' ')return -1;
 if(*end=='-'){sign=-1;end++;}
 if(parse_number(end,&zone,&after)||*after||zone>840)return -1;
 return holly_clock_seed(epoch,(int)zone*sign,1);
}
int holly_alarm_ack_text(const char*text){
 unsigned id;const char*end;if(parse_number(text,&id,&end)||*end)return -1;
 return holly_alarm_ack(id);
}
