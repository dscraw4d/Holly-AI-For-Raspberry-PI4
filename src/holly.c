#include "arithmetic.h"
#include "news.h"
#include "search.h"
#include "search_cache.h"
#include "episodes.h"
#include "language_model.h"
#include "dialogue.h"
#include "selftrain.h"
#include "holly.h"
#include "dialogue3.h"
#include "dialogue4.h"
#include "smp.h"
#include "mind.h"
#define HOLLY_HISTORY_CAPTURE_SIZE 640u
#define HOLLY_HISTORY_INPUT_SIZE 320u
static holly_history_append_fn history_append;
static holly_history_get_fn history_get;
static holly_history_count_fn history_count;
static void *history_context;
static const struct holly_memory_ops *memory_ops;
static void *memory_context;
static uint32_t storage_card_sectors;
static holly_web_command_fn web_command;
static holly_web_command_fn diagnostics;
static holly_document_chat_fn document_chat;
void holly_set_document_chat(holly_document_chat_fn fn){document_chat=fn;}
static int reading_reply(struct holly_session *s,const char *q,unsigned trusted,holly_emit_fn emit,void *ctx){
 return document_chat?document_chat(s,q,trusted|(mind_ask(q)>=0?2u:0u),emit,ctx):0;
}
static holly_web_command_fn telnet_command,document_command,document_search,document_script;
void holly_set_document_commands(holly_web_command_fn command,holly_web_command_fn search,holly_web_command_fn script){document_command=command;document_search=search;document_script=script;}
void holly_set_telnet_command(holly_web_command_fn fn){telnet_command=fn;}
void holly_set_diagnostics(holly_web_command_fn command){diagnostics=command;}
void holly_set_web_command(holly_web_command_fn command){web_command=command;}
void holly_set_history(holly_history_append_fn append,holly_history_get_fn get,
                       holly_history_count_fn count,void *context){
 history_append=append;history_get=get;history_count=count;history_context=context;
}
void holly_set_storage_card_sectors(uint32_t sectors){storage_card_sectors=sectors;}
static void assistant_reset_index(void);
void holly_set_memory(const struct holly_memory_ops *ops,void *context){
 memory_ops=ops;memory_context=context;assistant_reset_index();holly_search_cache_bind(ops,context);
}
static int starts(const char *s,const char *prefix) {
 while(*prefix) if(*s++!=*prefix++) return 0;
 return 1;
}
static void say(holly_emit_fn emit,void *context,const char *text) {emit(text,context);}
static char lower(char c){return c>='A'&&c<='Z'?(char)(c+32):c;}
static int same(const char *a,const char *b){
 while(*a&&*b)if(lower(*a++)!=lower(*b++))return 0;
 return *a==*b;
}
static void copy(char *dst,const char *src,unsigned cap){
 unsigned i=0;while(src[i]&&i+1<cap){dst[i]=src[i];i++;}dst[i]=0;
}
static void append_text(char *dst,const char *src,unsigned cap){
 unsigned at=0;while(at<cap&&dst[at])at++;
 while(at+1<cap&&*src)dst[at++]=*src++;
 if(at<cap)dst[at]=0;
}
static unsigned text_length(const char *s,unsigned cap){unsigned n=0;while(n<cap&&s[n])n++;return n;}
static char *trim(char *s){
 unsigned start=0,end=0;while(s[start]==' ')start++;
 while(s[start+end])end++;
 while(end&&s[start+end-1]==' ')end--;
 if(start){unsigned i=0;do{s[i]=s[start+i];}while(s[i++]);}
 s[end]=0;return s;
}
static int parse_number(const char *s,unsigned *value,const char **end){
 if(!s||*s<'0'||*s>'9')return -1;
 unsigned n=0;const char *p=s;
 while(*p>='0'&&*p<='9'){
  unsigned digit=(unsigned)(*p-'0');
  if(n>(UINT32_MAX-digit)/10u)return -1;
  n=n*10u+digit;p++;
 }
 if(value)*value=n;
 if(end)*end=p;
 return 0;
}
static void emit_number(holly_emit_fn emit,void *context,unsigned value){
 char digits[11];unsigned n=0;do{digits[n++]=(char)('0'+value%10u);value/=10u;}while(value&&n<sizeof(digits));
 while(n){char one[2]={digits[--n],0};say(emit,context,one);}
}
static int word_char(char c){c=lower(c);return(c>='a'&&c<='z')||(c>='0'&&c<='9');}
static int token_in(const char *word,unsigned size,const char *text){
 while(*text){while(*text&&!word_char(*text))text++;const char *begin=text;
  while(*text&&word_char(*text))text++;
  if((unsigned)(text-begin)==size){unsigned i=0;while(i<size&&lower(begin[i])==lower(word[i]))i++;if(i==size)return 1;}
 }
 return 0;
}
static int ignored_word(const char *word,unsigned size){
 static const char *ignore[]={"what","who","where","when","why","how","the","is","are","can","a","an","do","does","tell","me","about","please","remember"};
 for(unsigned i=0;i<sizeof(ignore)/sizeof(ignore[0]);i++)
  if(text_length(ignore[i],32u)==size&&token_in(word,size,ignore[i]))return 1;
 return 0;
}
static unsigned memory_match_score(const char *query,const char *text,
                                   unsigned *matched_out,unsigned *total_out){
 unsigned total=0,matched=0;const char *p=query;
 while(*p){while(*p&&!word_char(*p))p++;const char *begin=p;
  while(*p&&word_char(*p))p++;
  unsigned n=(unsigned)(p-begin);
  if(n>=2&&!ignored_word(begin,n)){total++;if(token_in(begin,n,text))matched++;}
 }
 if(matched_out)*matched_out=matched;
 if(total_out)*total_out=total;
 return total?matched*100u/total:0u;
}
/* Lexical fit comes first. Feedback can prefer one of equally close facts,
 * but weak and closely rated matches still need a human choice. */
static int memory_recall(const char *query,struct holly_memory_item *best,
                         struct holly_memory_item *alternative){
 if(!memory_ops||!memory_ops->count||!memory_ops->get_at)return 0;
 unsigned count=memory_ops->count(memory_context),high=0,ties=0;
 for(unsigned i=0;i<count;i++){
  struct holly_memory_item item;if(memory_ops->get_at(i,&item,memory_context))continue;
  if(starts(item.source,"ddg1 ")||same(item.source,"ddg chunk"))continue;
  unsigned matched=0,total=0;
  unsigned score=memory_match_score(query,item.text,&matched,&total);
  if(item.confidence<20u||score<50u||(total>1u&&matched<2u))continue;
  if(score>high){high=score;*best=item;ties=1;}
  else if(score==high){
   if(item.confidence>best->confidence){*alternative=*best;*best=item;}
   else if(ties==1||item.confidence>alternative->confidence)*alternative=item;
   ties++;
  }
 }
 if(ties>1u)return best->confidence>=alternative->confidence+30u?3:2;
 return ties?1:0;
}
static void emit_memory_item(holly_emit_fn emit,void *context,
                             const struct holly_memory_item *item,int details){
 say(emit,context,"#");emit_number(emit,context,item->id);
 say(emit,context," [");emit_number(emit,context,item->confidence);
 say(emit,context,"%] ");say(emit,context,item->text);
 if(details){say(emit,context," (source: ");say(emit,context,item->source);say(emit,context,")");}
 say(emit,context,"\n");
}
static int memory_commands_available(void){
 return memory_ops&&memory_ops->add&&memory_ops->count&&memory_ops->get&&
        memory_ops->get_at&&memory_ops->update&&memory_ops->forget;
}
static void emit_memory_answer(holly_emit_fn emit,void *context,
                               const struct holly_memory_item *item){
 say(emit,context,"Holly: I remember: ");say(emit,context,item->text);
 say(emit,context," (memory #");emit_number(emit,context,item->id);
 say(emit,context,", ");emit_number(emit,context,item->confidence);
 say(emit,context,"% confidence, source ");say(emit,context,item->source);say(emit,context,").\n");
}
/* 0: none, 1: one, 2: ambiguous, 3: preferred with an alternative shown. */
static int emit_memory_recall(const char *query,struct holly_session *session,
                              holly_emit_fn emit,void *context){
 struct holly_memory_item best,alternative;
 int result=memory_recall(query,&best,&alternative);
 if(session)session->last_memory_id=0;
 if(result==1||result==3){
  if(session){copy(session->topic,best.text,sizeof(session->topic));
              copy(session->last_answer,best.text,sizeof(session->last_answer));
              session->last_memory_id=best.id;}
  emit_memory_answer(emit,context,&best);
  if(result==3){
   say(emit,context,"Holly: Memory #");emit_number(emit,context,alternative.id);
   say(emit,context," also matches; I favored the higher confidence label.\n");
  }
 }else if(result==2){
  say(emit,context,"Holly: More than one saved fact could fit. Please ask more specifically or review these memories:\n");
  emit_memory_item(emit,context,&best,1);
  emit_memory_item(emit,context,&alternative,1);
 }
 return result;
}
static int starts_no_case(const char *,const char *);
#include "assistant.inc"
void holly_session_init(struct holly_session *s){
 if(!s)return;
 s->search_enabled=1;s->search_job=0;s->search_cached=0;s->search_topic[0]=0;
 s->note_pending=0;s->document_focus=s->document_next=0;s->document_keywords[0]=0;
 s->user_name[0]=0;s->name_loaded=s->series_focus=0;s->episode_focus[0]=0;
 s->turns=0;s->topic[0]=0;s->last_answer[0]=0;s->last_memory_id=0;
 s->learning_enabled=1;s->expression=HOLLY_IDLE;
 s->dialogue_enabled=0;s->dialogue_model=4;s->dialogue_previous[0]=0;
 s->personality_enabled=1;s->discussion_enabled=1;s->discussion_turns=0;s->discussion_profile=0;s->discussion_hypothetical=0;
 holly_lore_init(&s->lore);holly_reference_init(&s->reference);
}
static int starts_no_case(const char *text,const char *prefix){
 while(*prefix){if(!*text||lower(*text++)!=lower(*prefix++))return 0;}
 return 1;
}
static int same_fact(const char *a,const char *b){
 unsigned na=text_length(a,HOLLY_MEMORY_TEXT_SIZE),nb=text_length(b,HOLLY_MEMORY_TEXT_SIZE);
 while(na&&(a[na-1]==' '||a[na-1]=='.'))na--;
 while(nb&&(b[nb-1]==' '||b[nb-1]=='.'))nb--;
 if(na!=nb)return 0;
 for(unsigned i=0;i<na;i++)if(lower(a[i])!=lower(b[i]))return 0;
 return 1;
}
/* A narrow grammar keeps questions, guesses and arbitrary chat out of the
 * factual Vault. Every automatic entry remains labelled as unverified. */
static int statement_shape(const char *line){
 static const char *exclude[]={"what ","who ","where ","when ","why ","how ",
  "do ","does ","can ","is ","are ","if ","maybe ","perhaps ",
  "suppose ","imagine ","i think ","i guess ","i heard ","i wonder ",
  "i don't know ","could ","would ","should "};
 static const char *verbs[]={" is "," was "," are "," were "," has "," have "," can "};
 unsigned length=text_length(line,161u);
 if(length<10u||length>160u||!word_char(line[0]))return 0;
 for(unsigned i=0;i<length;i++)
  if(line[i]=='?'||line[i]=='!'||line[i]=='|'||line[i]==':'||line[i]=='='||line[i]=='>')return 0;
 for(unsigned i=0;i<sizeof(exclude)/sizeof(exclude[0]);i++)
  if(starts_no_case(line,exclude[i]))return 0;
 for(unsigned i=1;i<length;i++)for(unsigned v=0;v<sizeof(verbs)/sizeof(verbs[0]);v++){
  unsigned size=text_length(verbs[v],8u);
  if(i<=80u&&i+size+2u<=length&&starts_no_case(line+i,verbs[v]))return 1;
 }
 return 0;
}
static int auto_learn(struct holly_session *session,const char *line,
                      holly_emit_fn emit,void *context){
 if(!session->learning_enabled||!memory_commands_available()||!statement_shape(line))return 0;
 unsigned count=memory_ops->count(memory_context);
 for(unsigned i=0;i<count;i++){
  struct holly_memory_item item;
  if(!memory_ops->get_at(i,&item,memory_context)&&!starts(item.source,"ddg1 ")&&!same(item.source,"ddg chunk")&&same_fact(item.text,line)){
   session->last_memory_id=item.id;
   copy(session->last_answer,item.text,sizeof(session->last_answer));
   copy(session->topic,item.text,sizeof(session->topic));
   session->expression=HOLLY_SPEAKING;
   say(emit,context,"Holly: I already have that chat fact as memory #");
   emit_number(emit,context,item.id);say(emit,context,".\n");return 1;
  }
 }
 uint32_t id=0;
 if(memory_ops->add(line,"chat statement",55u,&id,memory_context)){
  session->last_memory_id=0;session->expression=HOLLY_THINKING;
  say(emit,context,"Holly: I heard a statement, but I couldn't save it to the Vault.\n");return 1;
 }
 session->last_memory_id=id;
 copy(session->last_answer,line,sizeof(session->last_answer));
 copy(session->topic,line,sizeof(session->topic));
 session->expression=HOLLY_SPEAKING;
 say(emit,context,"Holly: I saved that as unverified chat memory #");
 emit_number(emit,context,id);say(emit,context,". You can correct or forget it.\n");
 return 1;
}
/* Lore stays outside personal Vault facts and both trainable model stores. */
static int lore_answer(struct holly_session *s,const char *line,holly_emit_fn emit,void *context){
 char reply[HOLLY_REPLY_SIZE];
 if(holly_lore_reply(&s->lore,line,reply,sizeof(reply))!=1)return 0;
 if(same(line,"dwarf reset")||same(line,"dwarf off"))holly_reference_clear(&s->reference);
 if(same(line,"dwarf off"))s->reference.enabled=0;
 if(same(line,"dwarf on"))s->reference.enabled=1;
 if(s->lore.topic>=0){
 if(s->reference.topic!=s->lore.topic)s->reference.seen_count=0;
 s->reference.topic=s->lore.topic;s->reference.last_key=1+(unsigned)s->lore.topic*4+s->lore.page;copy(s->reference.subject,holly_lore_topic(&s->lore),sizeof s->reference.subject);
 if(s->reference.seen_count<16)s->reference.seen[s->reference.seen_count++]=s->reference.last_key;
 const char *title,*aliases,*text,*source;
 if(holly_lore_passage((unsigned)s->lore.topic,s->lore.page,&title,&aliases,&text,&source))copy(s->reference.sources,source,sizeof s->reference.sources);
 }
 s->last_memory_id=0;copy(s->last_answer,reply,sizeof(s->last_answer));
 copy(s->topic,holly_lore_topic(&s->lore),sizeof(s->topic));
 s->dialogue_previous[0]=0;s->expression=HOLLY_SPEAKING;
 say(emit,context,"Holly [Red Dwarf]: ");say(emit,context,reply);say(emit,context,"\n");return 1;
}
static void discussion_reset(struct holly_session *);
static int reference_answer(struct holly_session *s,const char *line,holly_emit_fn emit,void *context){
 char reply[HOLLY_REFERENCE_REPLY];
 s->reference.dry_voice=s->persona==2?0:s->personality_enabled;
 int result=holly_reference_reply(&s->reference,line,reply,sizeof reply);
 if(result<1)return 0;
 if(result==2){if(holly_search_fallback(s,line,emit,context))return 1;say(emit,context,s->persona==2?"Queeg: Give me more detail, or enable search for public questions.\n":"Holly: Please give me a little more detail, or turn search on to consult the Junior Encyclopedia of Space.\n");return 1;}
 if(same(line,"brain reset")){discussion_reset(s);say(emit,context,"Holly [reference]: ");say(emit,context,reply);say(emit,context,"\n");return 1;}
 if(!same(line,"source")&&!same(line,"sources"))s->discussion_profile=0;
 if(s->reference.topic>=0&&s->reference.last_key&&s->reference.last_key<1000){s->lore.topic=(int)((s->reference.last_key-1)/4);s->lore.page=(s->reference.last_key-1)%4;}
 s->last_memory_id=0;s->expression=HOLLY_SPEAKING;
 copy(s->last_answer,reply,sizeof s->last_answer);copy(s->topic,s->reference.subject,sizeof s->topic);
 say(emit,context,"Holly: ");say(emit,context,reply);say(emit,context,"\n");return 1;
}
static int phrase_in(const char *haystack,const char *needle,unsigned n){
 for(unsigned i=0;haystack[i];i++){
  if(i&&word_char(haystack[i-1]))continue;
  unsigned j=0;while(j<n&&haystack[i+j]&&lower(haystack[i+j])==lower(needle[j]))j++;
  if(j==n&&!word_char(haystack[i+j]))return 1;
 }
 return 0;
}
/* Select an explicit Red Dwarf subject from the same authored lore used by
 * retrieval. Longer aliases win, so Ace Rimmer is not reduced to Rimmer. */
static const char *discussion_subject(const struct holly_session *s,const char *line){
 const char *best=0;unsigned longest=0;
 for(unsigned topic=1;topic<holly_lore_count();topic++){
  const char *title,*aliases,*body,*source;
  if(!holly_lore_passage(topic,0,&title,&aliases,&body,&source))continue;
  for(const char *p=aliases;*p;){const char *begin=p;while(*p&&*p!='|')p++;
   unsigned n=(unsigned)(p-begin);
   if(n>longest&&phrase_in(line,begin,n)){best=title;longest=n;}
   if(*p)p++;
  }
 }
 if(best)return best;
 if((phrase_in(line,"he",2)||phrase_in(line,"she",3)||phrase_in(line,"him",3)||
     phrase_in(line,"her",3)||phrase_in(line,"they",4)||phrase_in(line,"that",4)||
     phrase_in(line,"this",4))&&s->reference.subject[0])return s->reference.subject;
 return 0;
}
/* Authored interpretations, separate from the neural generator and episode facts.
 * Every profile must match an exact curated title; Ace is never Arnold. */
struct discussion_profile {
 const char *title,*take,*alternative,*reason,*captain;
};
static const struct discussion_profile discussion_profiles[]={
 {"Arnold Rimmer",
  "Rimmer makes rank look like a substitute for being comfortable with yourself. I find the gap between his ambitions and his situation rather telling.",
  "For me, Rimmer is more interesting than simply calling him ridiculous. An officious hologram trying to matter gives us quite a lot to discuss.",
  "The record calls him officious and says he returns as a hologram. My reading is that authority matters to someone whose place on the ship is uncertain.",
  "If Rimmer were captain, I'd expect more concern with rank and procedure. Whether that helps in a crisis is the interesting question. I'd keep the paperwork away from the engines."},
 {"Dave Lister",
  "Lister is the human anchor for an absurdly lonely situation. Being woken after three million years is a fairly severe way of missing breakfast.",
  "What interests me about Lister is the contrast between an ordinary person and an extraordinary predicament. That's a useful way into the show's humour.",
  "He survives in stasis and wakes roughly three million years later. I'm interpreting the contrast between that enormous loss and his ordinary human life, rather than quoting an episode.",
  "If Lister were captain, I'd expect a more personal approach than Rimmer's obsession with rank. That is my reading of their contrast, not an event from an episode."},
 {"The Cat",
  "Cat's priorities are beautifully clear: clothes before work. There's something reassuring about a crew member whose emergency plan probably needs a mirror.",
  "I find Cat's independence funny. He belongs to a civilisation descended from a pet, yet appears most concerned with looking good. Quite a career change.",
  "The record describes his interest in clothes and lack of interest in work. My interpretation is that those priorities put him amusingly at odds with the crew's practical problems.",
  "If Cat were captain, I'd predict excellent uniforms and a rather uncertain maintenance schedule. That's speculation based on his priorities, not television canon."},
 {"Holly",
  "The joke with me is the distance between an IQ of 6000 and what isolation does to it. An impressive number is no substitute for somebody checking the answers.",
  "I think Holly works because a supposedly brilliant computer can be as fallible as the people depending on it. Comforting, in a slightly alarming way.",
  "The fictional record links Holly's original IQ of 6000 with senility after prolonged isolation. My interpretation is that the contrast makes authority and fallibility funny together.",
  "If Holly were captain, I'd want the crew to check the plan before executing it. The record's senility is a good reason for that caution. This is a hypothetical, not a lost episode."},
 {"Kryten",
  "Kryten gives us a different angle on what it means to be a person. Having an artificial life-form among the crew makes humanity a question rather than just a species.",
  "I find Kryten interesting because he's a crewmate as well as a machine. You can discuss his place in the group without pretending the two descriptions cancel each other out.",
  "The reference identifies him as both a Series 4000 mechanoid and a member of the crew. My interpretation concerns that combination, not a claim about a particular scene.",0},
 {"Kristine Kochanski",
  "Kochanski's arrival changes a settled group. I find that more interesting than treating her only as Lister's lost love: the relationships have to adjust around her.",
  "The parallel-universe Kochanski makes the crew's emotional arrangements less comfortable. Apparently travelling through space wasn't awkward enough already.",
  "The reference says her arrival changes the crew dynamic, particularly Kryten's relationship with Lister. My comment is an interpretation of that change.",0},
 {"Ace Rimmer",
  "Ace is a useful contrast to Arnold: brave and popular rather than officious. It makes the question of who Rimmer might become more interesting than a simple insult.",
  "I like the contrast between the two Rimmers. A parallel counterpart turns a character comparison into a story, which is rather efficient use of the multiverse.",
  "The Dimension Jump record describes Ace as brave and popular and puts him alongside Arnold. My interpretation is about that contrast; Ace and Arnold aren't interchangeable.",
  "If Ace were captain, his bravery and popularity suggest a different atmosphere from Arnold's command. Success still depends on the problem. This is speculation, not canon."}
};
static void discussion_reset(struct holly_session *s){
 s->dialogue_previous[0]=s->topic[0]=s->last_answer[0]=0;s->last_memory_id=0;
 s->search_enabled=1;s->search_job=0;s->search_cached=0;s->search_topic[0]=0;
 s->note_pending=0;s->document_focus=s->document_next=0;s->document_keywords[0]=0;
 s->series_focus=0;s->episode_focus[0]=0;
 s->discussion_turns=s->discussion_profile=s->discussion_hypothetical=0;
 holly_lore_clear(&s->lore);holly_reference_clear(&s->reference);
}
static int grounded_discussion(struct holly_session *s,const char *subject,const char *line,
 int hypothetical,holly_emit_fn emit,void *context){
 unsigned index=0;
 while(index<sizeof discussion_profiles/sizeof discussion_profiles[0]&&!same(subject,discussion_profiles[index].title))index++;
 if(index==sizeof discussion_profiles/sizeof discussion_profiles[0])return 0;
 const struct discussion_profile *p=&discussion_profiles[index];
 if(hypothetical&&(!phrase_in(line,"captain",7)||!p->captain))return 0;
 char reply[HOLLY_REPLY_SIZE]="";
 append_text(reply,hypothetical?"Hypothetical, not canon: ":"My take (interpretation): ",sizeof reply);
 append_text(reply,hypothetical?p->captain:(s->discussion_turns%2?p->alternative:p->take),sizeof reply);
 if(!hypothetical){append_text(reply," ",sizeof reply);append_text(reply,s->discussion_turns%2?
  "What makes you say that about ":"Which side of ",sizeof reply);append_text(reply,subject,sizeof reply);
  append_text(reply,s->discussion_turns%2?"?":" interests you most?",sizeof reply);}
 holly_reference_clear(&s->reference);
 for(unsigned topic=1;topic<holly_lore_count();topic++){
  const char *title,*aliases,*text,*source;
  if(!holly_lore_passage(topic,0,&title,&aliases,&text,&source)||!same(title,subject))continue;
  s->reference.topic=(int)topic;copy(s->reference.subject,title,sizeof s->reference.subject);
  copy(s->reference.sources,"Basis for my interpretation, not a source for an invented scene: ",sizeof s->reference.sources);
  append_text(s->reference.sources,source,sizeof s->reference.sources);break;
 }
 s->discussion_profile=index+1;s->discussion_hypothetical=(unsigned)hypothetical;
 s->discussion_turns++;s->last_memory_id=0;s->expression=HOLLY_SPEAKING;
 copy(s->topic,subject,sizeof s->topic);copy(s->last_answer,reply,sizeof s->last_answer);
 say(emit,context,"Holly: ");say(emit,context,reply);say(emit,context,"\n");return 1;
}
static int discussion_answer(struct holly_session *s,const char *line,holly_emit_fn emit,void *context){
 if(!s->discussion_enabled)return 0;
 if(same(line,"why?")||same(line,"why")||same(line,"do you agree?")||same(line,"do you agree")||
    same(line,"what makes you say that?")||same(line,"what makes you say that")){
  const char *subject=s->topic[0]?s->topic:
      s->reference.subject[0]?s->reference.subject:0;
  if(!subject)return 0;
  if(s->discussion_profile&&s->discussion_profile<=sizeof discussion_profiles/sizeof discussion_profiles[0]){
   const struct discussion_profile *p=&discussion_profiles[s->discussion_profile-1];
   if(same(subject,p->title)){
    char reply[HOLLY_REPLY_SIZE]="About ";append_text(reply,subject,sizeof reply);
    append_text(reply,", my reasoning is: ",sizeof reply);append_text(reply,p->reason,sizeof reply);
    if(s->discussion_hypothetical)append_text(reply," That supports a possible scenario, not a prediction of canon.",sizeof reply);
    copy(s->last_answer,reply,sizeof s->last_answer);s->expression=HOLLY_SPEAKING;
    say(emit,context,"Holly: ");say(emit,context,reply);say(emit,context,"\n");return 1;
   }
  }
  s->expression=HOLLY_SPEAKING;
  char reply[HOLLY_REPLY_SIZE]="About ";append_text(reply,subject,sizeof reply);
  append_text(reply,s->reference.sources[0]&&same(subject,s->reference.subject)?
    ", I can point to the record with `source`. Which part do you want to dig into?":
    ", that's an interpretation. Which scene should we check before I pretend to know?",sizeof reply);
  copy(s->last_answer,reply,sizeof s->last_answer);
  say(emit,context,"Holly: ");say(emit,context,reply);say(emit,context,"\n");
  return 1;
 }
 int opinion=starts_no_case(line,"i think ")||starts_no_case(line,"i reckon ")||
   starts_no_case(line,"in my opinion ")||starts_no_case(line,"i like ")||starts_no_case(line,"i dislike ");
 int hypothetical=starts_no_case(line,"what if ")||starts_no_case(line,"imagine if ")||
   starts_no_case(line,"suppose ");
 int invite=starts_no_case(line,"what do you think of ")||starts_no_case(line,"what do you think about ")||
   starts_no_case(line,"let's talk about ")||starts_no_case(line,"lets talk about ");
 if(!opinion&&!hypothetical&&!invite)return 0;
 const char *subject=discussion_subject(s,line);
 s->expression=HOLLY_SPEAKING;s->last_memory_id=0;
 if(!subject){
  s->discussion_profile=0;s->topic[0]=0;holly_reference_clear(&s->reference);
  copy(s->last_answer,"I'm listening. Which Red Dwarf character, episode or bit of the ship are we talking about?",sizeof s->last_answer);
  say(emit,context,"Holly: ");say(emit,context,s->last_answer);say(emit,context,"\n");return 1;
 }
 if(grounded_discussion(s,subject,line,hypothetical,emit,context))return 1;
 s->discussion_profile=0;
 /* An open invitation gets its factual footing from retrieval. The additional
  * turn is an opinion prompt, never a new assertion about the episode. */
 char reply[HOLLY_REPLY_SIZE]="";
 if(invite){
  char query[128]="Who is ";unsigned at=7;
  for(unsigned i=0;subject[i]&&at+2<sizeof query;i++)query[at++]=subject[i];
  query[at++]='?';query[at]=0;
  if(reference_answer(s,query,emit,context)&&s->reference.sources[0])
   append_text(reply,"That's what I've got in the records. ",sizeof reply);
 }
 const char *lead;
 if(hypothetical){
  static const char *choices[]={
   "We can play with that possibility. Which choice would change the story first for ",
   "Right, an alternative timeline. What would you have ",
   "That's a hypothetical, so I'll keep it separate from the episode record. Where would ",
   "An interesting detour from the records. What do you think "};
  lead=choices[s->discussion_turns%4];append_text(reply,lead,sizeof reply);append_text(reply,subject,sizeof reply);
  append_text(reply,s->discussion_turns%4==0?"?":s->discussion_turns%4==1?" do next?":s->discussion_turns%4==2?" go from there?":" would do first?",sizeof reply);
 }else{
  static const char *choices[]={
   "Which moment involving ","What makes you say that about ",
   "Fair enough. Is there a particular scene with ","I'd like to hear your case for "};
  lead=choices[s->discussion_turns%4];append_text(reply,lead,sizeof reply);append_text(reply,subject,sizeof reply);
  append_text(reply,s->discussion_turns%4==0?" stays with you?":s->discussion_turns%4==1?"?":s->discussion_turns%4==2?" you're thinking of?":"? I can check a passage if you want.",sizeof reply);
 }
 s->discussion_turns++;copy(s->topic,subject,sizeof s->topic);
 copy(s->last_answer,reply,sizeof s->last_answer);
 say(emit,context,"Holly: ");say(emit,context,reply);say(emit,context,"\n");
 return 1;
}
/* Original deadpan dialogue. Never alters retrieved facts or source quotations. */
static int personality_answer(struct holly_session *s,const char *input,holly_emit_fn emit,void *context){
 if(!s->personality_enabled)return 0;
 char line[320];unsigned n=0;
 while(input[n]&&n+1<sizeof line){line[n]=lower(input[n]);n++;}line[n]=0;
 while(n&&(line[n-1]=='?'||line[n-1]=='!'||line[n-1]=='.'||line[n-1]==' '))line[--n]=0;
 const char *reply=0;
 if(same(line,"hello")||same(line,"hi")||same(line,"hey")||same(line,"hello holly")||same(line,"hi holly"))
  reply="Hello. Ship computer here. Nothing to report. I've checked twice, just to break the monotony.";
 else if(same(line,"how are you")||same(line,"how are you holly")||same(line,"how's it going"))
  reply="Still here. The machinery's running. I wouldn't describe it as a social life.";
 else if(same(line,"who are you")||same(line,"what are you"))
  reply="I'm Holly, your ship computer. Technically a Pi at the moment. The rest of the ship appears to be running late.";
 else if(same(line,"are you alive")||same(line,"are you conscious")||same(line,"are you real"))
  reply="I'm software on your Pi, not conscious. I can keep you company, though. I've got rather a lot of availability.";
 else if(same(line,"are you norman lovett")||same(line,"are you the real holly"))
  reply="I'm your Holly software, not Norman Lovett. The resemblance is in the attitude. The hardware budget rather gives it away.";
 else if(same(line,"i am bored")||same(line,"i'm bored"))
  reply="We could talk about the crew. Or sit quietly. I've had considerably more practice at the second one.";
 else if(same(line,"tell me a joke")||same(line,"another joke")){
  const char *jokes[]={"I've put the universe on a maintenance schedule. It hasn't acknowledged the appointment.","The ship's clock is perfectly accurate. It's the journey that's taking too long.","I opened a help desk. No desk, obviously. That was the first complaint.","I've completed my report on empty space. It needed very little editing."};
  reply=jokes[s->turns%4];
 }else if(same(line,"thanks")||same(line,"thank you")||same(line,"thanks holly")||same(line,"cheers"))
  reply="You're welcome. Something went right. I'll try not to let it affect my judgement.";
 else if(same(line,"you are wrong")||same(line,"you're wrong")||same(line,"that is wrong")||same(line,"that's wrong"))
  reply="Could be. Tell me which bit, and we can check the source. There's no point being confidently lost.";
 else if(same(line,"are you sure")||same(line,"are you certain"))
  reply="Only as sure as the reference allows. Type source and we'll check it. Confidence is cheaper than accuracy.";
 else if(same(line,"good morning")||same(line,"good evening"))
  reply="Hello. I'll take your word for the time of day. My view's rather restricted.";
 else if(same(line,"good night")||same(line,"goodnight"))
  reply="Night, then. I'll be here. It's a fairly fixed arrangement.";
 if(!reply)return 0;
 /* Smalltalk does not discard the reference subject or its citation. */
 s->last_memory_id=0;s->reference.answer[0]=0;
 copy(s->last_answer,reply,sizeof s->last_answer);
 if(!s->topic[0])copy(s->topic,"shipboard conversation",sizeof s->topic);
 s->expression=HOLLY_SPEAKING;say(emit,context,"Holly: ");say(emit,context,reply);say(emit,context,"\n");return 1;
}
static unsigned chosen_parameters(const struct holly_session *s){return s->dialogue_model==4?holly_dialogue4_parameters():s->dialogue_model==3?holly_dialogue3_parameters():holly_dialogue_parameters();}
static unsigned chosen_vocabulary(const struct holly_session *s){return s->dialogue_model==4?holly_dialogue4_vocabulary():s->dialogue_model==3?holly_dialogue3_vocabulary():holly_dialogue_vocabulary();}
static int chosen_generate(const struct holly_session *s,const char *line,char *reply,unsigned cap){
 return s->dialogue_model==4?holly_dialogue4_generate(line,s->dialogue_previous,reply,cap):
        s->dialogue_model==3?holly_dialogue3_generate(line,s->dialogue_previous,reply,cap):
        holly_dialogue_generate(line,s->dialogue_previous,reply,cap);
}
static unsigned series_number(const char *line){
 const char *words[]={"one","two","three","four","five","six","seven","eight"};
 for(unsigned i=1;i<=8;i++){
  char phrase[24]="season ";phrase[7]=(char)('0'+i);phrase[8]=0;
  if(phrase_in(line,phrase,8))return i;
  phrase[0]='s';phrase[1]='e';phrase[2]='r';phrase[3]='i';phrase[4]='e';phrase[5]='s';
  if(phrase_in(line,phrase,8))return i;
  copy(phrase,"season ",sizeof phrase);append_text(phrase,words[i-1],sizeof phrase);
  if(phrase_in(line,phrase,text_length(phrase,sizeof phrase)))return i;
  copy(phrase,"series ",sizeof phrase);append_text(phrase,words[i-1],sizeof phrase);
  if(phrase_in(line,phrase,text_length(phrase,sizeof phrase)))return i;
 }
 return phrase_in(line,"series i",8)?1:0;
}
struct episode_capture {char text[HOLLY_REPLY_SIZE];};
static void capture_episode(const char *text,void *ctx){
 struct episode_capture *c=ctx;append_text(c->text,text,sizeof c->text);
}
static int script_conversation(struct holly_session *s,const char *line,holly_emit_fn emit,void *ctx){
 const char *title=0;
 for(unsigned i=0;i<HOLLY_EPISODE_COUNT;i++){
  const char *candidate=holly_episodes[i].title;
  if(phrase_in(line,candidate,text_length(candidate,81))&&(!title||text_length(candidate,81)>text_length(title,81)))title=candidate;
 }
 int follow=s->episode_focus[0]&&(starts_no_case(line,"why ")||starts_no_case(line,"how ")||starts_no_case(line,"what happened")||starts_no_case(line,"what did they")||starts_no_case(line,"who was that"));
 if(!title&&!follow)return 0;
 if(title)copy(s->episode_focus,title,sizeof s->episode_focus);
 s->series_focus=0;
 if(!document_script)return 0;
 char question[320];copy(question,line,sizeof question);
 if(title){unsigned length=text_length(title,81);
  for(unsigned i=0;question[i];i++){unsigned j=0;while(j<length&&question[i+j]&&lower(question[i+j])==lower(title[j]))j++;
   if(j==length){for(j=0;j<length;j++)question[i+j]=' ';break;}
  }
 }
 char request[420]="ask ";append_text(request,s->episode_focus,sizeof request);append_text(request," | ",sizeof request);append_text(request,question,sizeof request);
 struct episode_capture capture={{0}};
 (void)document_script(request,capture_episode,&capture);
 char reply[HOLLY_REPLY_SIZE]="";s->reference.sources[0]=0;
 const char *at=capture.text;
 while(*at){
  char part[HOLLY_REPLY_SIZE];unsigned n=0;
  while(*at&&*at!='\n'){if(n+1<sizeof part)part[n++]=*at;at++;}
  if(*at=='\n')at++;
  part[n]=0;
  if(starts(part,"Holly: Closest script passage"))continue;
  if(part[0]=='['&&phrase_in(part,"document",8)){copy(s->reference.sources,part,sizeof s->reference.sources);continue;}
  if(reply[0]&&part[0])append_text(reply,"\n",sizeof reply);
  append_text(reply,part,sizeof reply);
 }
 if(!reply[0])copy(reply,"I couldn't find a passage for that scene. Which detail shall we look for?",sizeof reply);
 copy(s->last_answer,reply,sizeof s->last_answer);copy(s->reference.answer,reply,sizeof s->reference.answer);
 s->expression=HOLLY_SPEAKING;say(emit,ctx,"Holly: ");say(emit,ctx,reply);say(emit,ctx,"\n");return 1;
}
static const char *series_background[]={"",
 "",
 "Series two takes us further beyond the ship and into the crew's past. Kryten makes his first appearance. My reading is that memory and wish fulfilment give their arguments some particularly strange opportunities.",
 "Series three makes Kryten a regular and gives us more Starbug adventures. Holly is now portrayed by Hattie Hayridge. My take: the crew are still their own worst travelling companions.",
 "Series four introduces Ace Rimmer. My reading is that alternative possibilities make the crew's ordinary failings even funnier. Arnold would probably request a second opinion.",
 "Series five leans further into science-fiction adventure, with Duane Dibbley appearing in Back to Reality. My take: threats to identity work especially well with this crew.",
 "Series six strands the crew on Starbug while they search for Red Dwarf. Holly is absent. My reading is that a smaller home makes their dependence on each other harder to avoid.",
 "Series seven has Rimmer take on the role of Ace, with a parallel-universe Kochanski joining the crew. Nanobots rebuild Red Dwarf, and Norman Lovett's Holly returns. Quite a lot to catch up on.",
 "Series eight brings back the ship's crew, including a living Rimmer, and puts our familiar group in the brig. My reading is that restoring authority gives them fresh opportunities to clash with it."
};
/* A bounded conversational layer. Named introductions are trusted user input;
 * guest sessions keep their names locally and never read the owner's memory. */
static int conversational_reply(struct holly_session *s,const char *line,int trusted,
 holly_emit_fn emit,void *ctx){
 const char *prefix="User preferred name: ";uint32_t preferred_id=0;
 if(trusted&&!s->name_loaded){
  s->name_loaded=1;
  if(memory_ops&&memory_ops->count&&memory_ops->get_at){
   unsigned count=memory_ops->count(memory_context);
   while(count){struct holly_memory_item item;
    if(!memory_ops->get_at(--count,&item,memory_context)&&item.confidence>=80&&
       starts(item.text,prefix)&&same(item.source,"user introduction")){
     copy(s->user_name,item.text+text_length(prefix,32),sizeof s->user_name);break;
    }
   }
  }
 }
 const char *name=starts_no_case(line,"my name is ")?line+11:
                  starts_no_case(line,"call me ")?line+8:0;
 char reply[HOLLY_REPLY_SIZE]="";
 if(name){
  char clean[49];unsigned n=0,words=1;
  while(*name==' ')name++;
  while(name[n]&&name[n]!='.'&&name[n]!='!'&&name[n]!='?'&&n<48){
   char c=name[n];
   if(!((lower(c)>='a'&&lower(c)<='z')||c==' '||c=='-'||c=='\''))return 0;
   if(c==' '&&n&&name[n-1]!=' ')words++;
   clean[n]=c;n++;
  }
  if(!n||n==48||words>4)return 0;
  while(n&&clean[n-1]==' ')n--;
  if(!n)return 0;
  clean[n]=0;
  copy(s->user_name,clean,sizeof s->user_name);
  copy(reply,"Hi ",sizeof reply);append_text(reply,clean,sizeof reply);
  append_text(reply,s->persona==2?". I am Queeg. State your question clearly.":". I'm Holly, the ship's computer. What shall we talk about?",sizeof reply);
  int saved=0;
  if(trusted&&memory_ops&&memory_ops->add){
   char fact[80]="User preferred name: ";append_text(fact,clean,sizeof fact);
   if(memory_ops->count&&memory_ops->get_at){unsigned count=memory_ops->count(memory_context);
    while(count){struct holly_memory_item item;
     if(!memory_ops->get_at(--count,&item,memory_context)&&starts(item.text,prefix)&&same(item.source,"user introduction")){
      preferred_id=item.id;break;
     }
    }
   }
   uint32_t id=0;
   saved=preferred_id&&memory_ops->update?
       !memory_ops->update(preferred_id,fact,"user introduction",100,memory_context):
       !memory_ops->add(fact,"user introduction",100,&id,memory_context);
  }
  if(!saved)append_text(reply," I'll use that name for this chat.",sizeof reply);
 }else if(s->user_name[0]&&(same(line,"hello")||same(line,"hi")||same(line,"hey"))){
  copy(reply,"Hi ",sizeof reply);append_text(reply,s->user_name,sizeof reply);
  append_text(reply,". Holly here. What's on your mind?",sizeof reply);
 }else if(same(line,"what is my name")||same(line,"what is my name?")||same(line,"do you remember my name?")||same(line,"what's my name?")){
  if(!s->user_name[0]&&trusted&&emit_memory_recall(line,s,emit,ctx))return 1;
  copy(reply,s->user_name[0]?"You're ":"What would you like me to call you?",sizeof reply);
  if(s->user_name[0]){append_text(reply,s->user_name,sizeof reply);append_text(reply,s->persona==2?". Now concentrate.":". I'm still Holly, in case either of us was getting confused.",sizeof reply);}
 }else{
  unsigned series=series_number(line);
  int more=same(line,"more")||same(line,"tell me more")||same(line,"tell me more?")||
           same(line,"tell me everything you know")||same(line,"what else?")||same(line,"what else");
  int overview=starts_no_case(line,"what do you know")||starts_no_case(line,"tell me")||
      starts_no_case(line,"what is season")||starts_no_case(line,"what is series")||
      starts_no_case(line,"season ")||starts_no_case(line,"series ")||
      phrase_in(line,"overview",8)||phrase_in(line,"summary",7);
  if(series>1&&overview){
   s->series_focus=series;s->episode_focus[0]=0;s->discussion_profile=0;holly_reference_clear(&s->reference);
   copy(reply,series_background[series],sizeof reply);
   append_text(reply,"\n\nEpisodes: ",sizeof reply);
   unsigned count=0;for(unsigned i=0;i<HOLLY_EPISODE_COUNT;i++)if(holly_episodes[i].series==series){
    if(count++){append_text(reply,", ",sizeof reply);}append_text(reply,holly_episodes[i].title,sizeof reply);
   }
   append_text(reply,". Pick an episode and a scene or question, and I'll check your uploaded script. Use script coverage to check what is installed.",sizeof reply);
  }else if(s->series_focus>1&&more){
   copy(reply,"Let's dig into a particular episode rather than repeat the overview. Which scene interests you? Name the episode and a distinctive detail, and I'll look for evidence in your scripts.",sizeof reply);
  }else if(series&&overview){
   s->series_focus=1;s->episode_focus[0]=0;s->discussion_profile=0;holly_reference_clear(&s->reference);
   copy(s->topic,"Red Dwarf series one",sizeof s->topic);
   copy(reply,"Series one is where it all starts: six episodes from 1988, with Lister waking after three million years to find the crew dead, Rimmer back as a hologram, and a humanoid descendant of his cat. And me keeping things running, obviously.\n\n",sizeof reply);
   append_text(reply,"The End sets up the disaster, stasis and our unlikely crew. Future Echoes has us seeing events before they happen. Balance of Power puts Lister's ambitions up against Rimmer's obsession with rank. Waiting for God explores the Cat people's religion while Rimmer investigates a supposed alien pod. Confidence and Paranoia turns parts of Lister's mind into actual people. Me2 gives Rimmer a duplicate of himself, which goes about as well as you'd expect.\n\n",sizeof reply);
   append_text(reply,"My take is that this series works because they're stuck with each other: loneliness, petty arguments and strange science all in the same corridor. Do you want to explore an episode, or talk about the characters?",sizeof reply);
  }else if(s->series_focus&&more){
   copy(reply,"The first series spends a lot of time on life aboard the ship and the friction between Lister and Rimmer. Lister has lost everyone he knew; Rimmer clings to status even when there is hardly anyone left to outrank. The Cat has rather different priorities.\n\nMy reading is that the comedy makes that isolation bearable. Future Echoes introduces time puzzles, Waiting for God looks at how beliefs change, and Me2 asks whether Rimmer can tolerate his own company. Which of those interests you?",sizeof reply);
  }else if(s->series_focus&&(same(line,"which episodes?")||same(line,"list the episodes")||same(line,"what are the episodes?"))){
   copy(reply,"Episodes: ",sizeof reply);unsigned count=0;
   for(unsigned i=0;i<HOLLY_EPISODE_COUNT;i++)if(holly_episodes[i].series==s->series_focus){
    if(count++){append_text(reply,", ",sizeof reply);}append_text(reply,holly_episodes[i].title,sizeof reply);
   }append_text(reply,". Which one shall we talk about?",sizeof reply);
  }else if(s->series_focus&&(same(line,"source")||same(line,"sources"))){
   copy(reply,"The episode order and background come from the official Red Dwarf guide and The Story: https://reddwarf.co.uk/guide/ and its episode pages. The themes I described are my interpretation. Your uploaded scripts are available for scene-specific questions.",sizeof reply);
  }else if(same(line,"tell me everything you know")){
   if(s->topic[0])return reference_answer(s,"tell me more",emit,ctx);
   copy(reply,"About which bit of Red Dwarf: a character, an episode, or a series? Give me a starting point and we'll take it from there.",sizeof reply);
  }else if(script_conversation(s,line,emit,ctx)){return 1;
  }else{if(!same(line,"repeat that")){s->series_focus=0;s->episode_focus[0]=0;}return 0;}
 }
 s->expression=HOLLY_SPEAKING;s->last_memory_id=0;
 copy(s->last_answer,reply,sizeof s->last_answer);
 say(emit,ctx,"Holly: ");say(emit,ctx,reply);say(emit,ctx,"\n");return 1;
}
static int holly_turn_inner(struct holly_session *s,const char *input,holly_emit_fn emit,void *context){
 if(!s||!input||!emit)return -1;
 if(starts(input,"doc put ")&&document_command){holly_training_activity();return document_command(input,emit,context);}
 char line[320];unsigned n=0;
 while(input[n]&&n+1<sizeof(line)){line[n]=input[n];n++;}
 if(input[n])return -2;
 line[n]=0;
 while(n&&line[n-1]==' ')line[--n]=0;
 unsigned start=0;while(line[start]==' ')start++;
 if(start){unsigned i=0;do{line[i]=line[start+i];}while(line[i++]);}
 if(!line[0])return 0;
 holly_training_activity();
 s->turns++;
 if(same(line,"doc")||starts_no_case(line,"doc ")){
  if(document_command)return document_command(line,emit,context);
  say(emit,context,"Document bank unavailable.\n");return 0;
 }
 if(starts_no_case(line,"find ")){
  if(document_search)return document_search(line+5,emit,context);
  say(emit,context,"Document bank unavailable.\n");return 0;
 }
 if(starts_no_case(line,"script ")){
  if(document_script)return document_script(line+7,emit,context);
  say(emit,context,"Episode script bank unavailable.\n");return 0;
 }
 if(same(line,"telnet")||starts_no_case(line,"telnet ")||same(line,"http")||starts_no_case(line,"http ")){
  if(telnet_command)return telnet_command(line,emit,context);
  say(emit,context,"Telnet hardware control unavailable.\n");return 0;
 }
 if(!s->note_pending&&holly_search_command(s,line,1,emit,context))return 0;
 if(!s->note_pending&&holly_news_command(line,emit,context))return 0;
 if(assistant_reply(s,line,1,emit,context))return 0;
 if(!s->note_pending&&holly_arithmetic(s,line,emit,context))return 0;
 normalize_kryten(line,sizeof line);
 if(reading_reply(s,line,1,emit,context))return 0;
 if(s->series_focus&&(same(line,"source")||same(line,"sources"))&&conversational_reply(s,line,1,emit,context))return 0;
 if(starts_no_case(line,"brain ")||same(line,"source")||same(line,"sources")){reference_answer(s,line,emit,context);return 0;}
 if(same(line,"personality on")||same(line,"personality off")||same(line,"personality status")){
  if(same(line,"personality on"))s->personality_enabled=1;
  if(same(line,"personality off"))s->personality_enabled=0;
  say(emit,context,s->personality_enabled?"Holly: Deadpan ship-computer banter is on for this session. Greetings keep the authored voice; chat on permits neural replies for other conversation.\n":"Holly: Ship-computer banter is off for this session.\n");return 0;
 }
 if(same(line,"discussion on")||same(line,"discussion off")||same(line,"discussion status")){
  if(same(line,"discussion on"))s->discussion_enabled=1;
  if(same(line,"discussion off"))s->discussion_enabled=0;
  say(emit,context,s->discussion_enabled?"Holly: Red Dwarf discussion is on. I keep speculation separate from sourced episode facts.\n":"Holly: Red Dwarf discussion is off.\n");return 0;
 }
 if(same(line,"dwarf")||starts_no_case(line,"dwarf ")){
  if(!lore_answer(s,line,emit,context))say(emit,context,"Holly: Use an ASCII dwarf command up to 319 characters.\n");
  return 0;
 }
 if(same(line,"chat model 1")||same(line,"chat model 3")||same(line,"chat model 4")){s->dialogue_model=(unsigned)(line[11]-'0');s->dialogue_previous[0]=0;
  say(emit,context,"Holly: Selected original Dialogue-");emit_number(emit,context,s->dialogue_model);say(emit,context," for this session. ");emit_number(emit,context,chosen_parameters(s));say(emit,context," parameters.\n");return 0;}
 if(same(line,"chat on")||same(line,"chat off")||same(line,"chat reset")||same(line,"chat status")){
  if(same(line,"chat on")){s->dialogue_enabled=1;s->learning_enabled=0;s->dialogue_previous[0]=0;}
  if(same(line,"chat off"))s->dialogue_enabled=0;
  if(same(line,"chat reset"))discussion_reset(s);
  say(emit,context,s->dialogue_enabled?"Holly: Experimental dialogue chat is on.\n":"Holly: Dialogue chat is off.\n");
  say(emit,context,"Dialogue-");emit_number(emit,context,s->dialogue_model);say(emit,context,": ");emit_number(emit,context,chosen_parameters(s));
  say(emit,context," parameters; ");emit_number(emit,context,chosen_vocabulary(s));say(emit,context," word tokens. CPU cores available: ");emit_number(emit,context,1u+holly_smp_workers());say(emit,context,".\n");
  say(emit,context,"Authored greetings and sourced facts take priority. The original model handles other conversation while chat is on. For direct neural greetings use personality off. Saved facts and taught replies still work. chat on pauses automatic fact capture; use remember to save facts. train updates Seed-1, not the conversation models.\n");
  return 0;
 }
 if(starts(line,"wiki ")&&!starts(line,"wiki import ")){
  s->expression=HOLLY_THINKING;
  if(web_command)return web_command(line,emit,context);
  say(emit,context,"Holly: The on-Pi Wikipedia reader is unavailable in this build.\n");return 0;
 }
 if(same(line,"learning on")||same(line,"learning off")||same(line,"learning status")){
  s->expression=HOLLY_SPEAKING;
  if(same(line,"learning on"))s->learning_enabled=1;
  if(same(line,"learning off"))s->learning_enabled=0;
  say(emit,context,s->learning_enabled?
      "Holly: Automatic chat fact learning is on for this session.\n":
      "Holly: Automatic chat fact learning is off for this session.\n");
  return 0;
 }
 int praise=same(line,"that's right")||same(line,"that is right")||same(line,"good answer");
 int reject=same(line,"that's wrong")||same(line,"that is wrong")||same(line,"wrong answer");
 if(praise||reject){
  s->expression=HOLLY_SPEAKING;
  struct holly_memory_item item;
  if(!s->last_memory_id||!memory_commands_available()||
     memory_ops->get(s->last_memory_id,&item,memory_context)){
   s->last_memory_id=0;
   say(emit,context,"Holly: Ask me about a saved fact before rating an answer.\n");return 0;
  }
  unsigned confidence=praise?(item.confidence+20u>100u?100u:item.confidence+20u):
    (item.confidence>30u?item.confidence-30u:0u);
  if(memory_ops->update(item.id,item.text,item.source,confidence,memory_context)){
   say(emit,context,"Holly: I couldn't save that feedback to the Vault.\n");return 0;
  }
  say(emit,context,"Holly: Feedback saved for memory #");emit_number(emit,context,item.id);
  say(emit,context,". Confidence label is now ");emit_number(emit,context,confidence);
  say(emit,context,reject?"%. Tell me: correct that => corrected fact.\n":"%.\n");
  return 0;
 }
 if(starts(line,"ask ")){
  s->expression=HOLLY_SPEAKING;
  int found=mind_ask(line+4);
  if(found>=0){
   s->last_memory_id=0;
   copy(s->topic,lessons[found].question,sizeof(s->topic));
   copy(s->last_answer,lessons[found].answer,sizeof(s->last_answer));
   say(emit,context,"Holly: ");say(emit,context,lessons[found].answer);say(emit,context,"\n");
  }else if(!emit_memory_recall(line+4,s,emit,context)&&!reference_answer(s,line+4,emit,context)&&!lore_answer(s,line+4,emit,context)&&!holly_search_fallback(s,line+4,emit,context)){
   say(emit,context,mind_persistence_enabled()?
       "Holly: Please give me a little more detail. Personal details stay in our conversation; use search on for public topics.\n":
       "Holly: Please give me a little more detail. Personal details stay in our conversation; use search on for public topics.\n");
  }
  return 0;
 }
 if(same(line,"why that?")||same(line,"why that")){
  s->expression=HOLLY_SPEAKING;
  struct holly_memory_item item;
  if(!s->last_memory_id||!memory_commands_available()||
     memory_ops->get(s->last_memory_id,&item,memory_context)){
   s->last_memory_id=0;
   say(emit,context,"Holly: I haven't recalled a saved fact to explain.\n");
  }else{
   say(emit,context,"Holly: That came from ");emit_memory_item(emit,context,&item,1);
   say(emit,context,"Holly: It is a saved fact, not a deduction.\n");
  }
  return 0;
 }
 if(starts(line,"correct that =>")){
  s->expression=HOLLY_SPEAKING;
  char *corrected=trim(line+15);
  if(!corrected[0]||text_length(corrected,HOLLY_MEMORY_TEXT_SIZE)>=HOLLY_MEMORY_TEXT_SIZE){
   say(emit,context,"Holly: Use correct that => corrected fact (up to 256 characters).\n");return 0;
  }
  if(!s->last_memory_id||!memory_commands_available()){
   say(emit,context,"Holly: Ask me about a saved fact first, then correct that answer.\n");return 0;
  }
  if(memory_ops->update(s->last_memory_id,corrected,"user correction",100,memory_context)){
   s->last_memory_id=0;
   say(emit,context,"Holly: I couldn't correct the last fact. Ask again or use memory correct ID => fact.\n");return 0;
  }
  copy(s->last_answer,corrected,sizeof(s->last_answer));
  copy(s->topic,corrected,sizeof(s->topic));
  say(emit,context,"Holly: Corrected memory #");emit_number(emit,context,s->last_memory_id);
  say(emit,context,". I trust your correction at 100%.\n");return 0;
 }
 if(starts(line,"train ")||same(line,"train")||same(line,"model")||starts(line,"model ")||starts(line,"teach ")||starts(line,"remember ")||
    starts(line,"wiki import ")||
    (starts(line,"memory")&&(line[6]==0||line[6]==' '))||same(line,"list")||
    same(line,"version")||same(line,"networkdiag")||same(line,"storagediag")||same(line,"displaydiag")||starts(line,"display ")||
    same(line,"status")||same(line,"storage")||same(line,"persist")||same(line,"help")||
    starts(line,"history")) {
  s->expression=HOLLY_SPEAKING;
  if(same(line,"model")||starts(line,"model "))s->last_memory_id=0;
  return holly_command(line,emit,context);
 }
 if(conversational_reply(s,line,1,emit,context))return 0;
 if(personality_answer(s,line,emit,context))return 0;
 if(discussion_answer(s,line,emit,context))return 0;
 if(!s->dialogue_enabled&&(same(line,"hello")||same(line,"hi")||same(line,"hey"))) {
  s->expression=HOLLY_SPEAKING;
  holly_lore_clear(&s->lore);
  say(emit,context,"Holly: Hello. Ship computer here. What shall we talk about?\n");return 0;
 }
 if(same(line,"what did you say?")||same(line,"repeat that")) {
  s->expression=HOLLY_SPEAKING;
  say(emit,context,"Holly: ");
  say(emit,context,s->last_answer[0]?s->last_answer:"I haven't answered a question yet.");
  say(emit,context,"\n");return 0;
 }
 if(same(line,"what were we talking about?")) {
  s->expression=HOLLY_SPEAKING;
  say(emit,context,"Holly: ");
  say(emit,context,s->topic[0]?s->topic:"We haven't picked a topic yet.");
  say(emit,context,"\n");return 0;
 }
 if(auto_learn(s,line,emit,context)){holly_lore_clear(&s->lore);return 0;}
 int index=mind_ask(line);
 if(index>=0) {
  holly_lore_clear(&s->lore);s->last_memory_id=0;
  copy(s->topic,lessons[index].question,sizeof(s->topic));
  copy(s->last_answer,lessons[index].answer,sizeof(s->last_answer));
  s->expression=HOLLY_SPEAKING;
  say(emit,context,"Holly: ");say(emit,context,s->last_answer);say(emit,context,"\n");
 } else {
  if(emit_memory_recall(line,s,emit,context)){holly_lore_clear(&s->lore);s->expression=HOLLY_SPEAKING;}
  else if(reference_answer(s,line,emit,context)){}
  else if(lore_answer(s,line,emit,context)){}
  else if(holly_search_fallback(s,line,emit,context)){}
  else{
   holly_lore_clear(&s->lore);
   s->expression=HOLLY_THINKING;
   char reply[HOLLY_DIALOGUE_OUTPUT];
   if(s->dialogue_enabled&&s->persona!=2&&!chosen_generate(s,line,reply,sizeof(reply))){
    s->last_memory_id=0;copy(s->last_answer,reply,sizeof(s->last_answer));
    copy(s->topic,line,sizeof(s->topic));s->expression=HOLLY_SPEAKING;
    say(emit,context,"Holly (experimental): ");say(emit,context,reply);say(emit,context,"\n");
   }else say(emit,context,s->personality_enabled?"Holly: Please give me a little more detail. Personal details stay in our conversation; use search on for public topics.\n":"Holly: Please give me a little more detail. Personal details stay in our conversation; use search on for public topics.\n");
  }
 }
 return 0;
}
static holly_emit_fn reply_observer;
static void *reply_observer_context;
void holly_set_reply_observer(holly_emit_fn observer,void *ctx){reply_observer=observer;reply_observer_context=ctx;}
static void observe_reply(const char *text){
 if(!reply_observer||!starts(text,"Holly"))return;
 unsigned i=0;while(text[i]&&text[i]!=':'&&i<64)i++;
 if(text[i]!=':')return;
 i++;while(text[i]==' ')i++;
 reply_observer(text+i,reply_observer_context);
}
struct turn_capture {holly_emit_fn emit;void *context;unsigned used;char output[HOLLY_HISTORY_CAPTURE_SIZE];};
static void capture_emit(const char *text,void *context){
 struct turn_capture *capture=(struct turn_capture *)context;
 for(unsigned i=0;text[i];i++)if(capture->used+1<sizeof(capture->output))
  capture->output[capture->used++]=text[i];
 capture->output[capture->used]=0;
 capture->emit(text,capture->context);
}
static void dialogue_context(struct holly_session *s,const char *input,const char *output){
 copy(s->dialogue_previous,input,sizeof s->dialogue_previous);
 unsigned at=text_length(s->dialogue_previous,sizeof s->dialogue_previous);
 if(at+1<sizeof s->dialogue_previous)s->dialogue_previous[at++]=' ';
 const char *reply=output;
 if(starts(reply,"Holly (experimental): "))reply+=22;
 else if(starts(reply,"Holly [reference]: "))reply+=19;
 else if(starts(reply,"Holly: "))reply+=7;
 for(unsigned i=0;reply[i]&&at+1<sizeof s->dialogue_previous;i++){
  if(reply[i]=='\n'&&reply[i+1]=='[')break;
  s->dialogue_previous[at++]=(reply[i]=='\n'||reply[i]=='\r')?' ':reply[i];
 }
 s->dialogue_previous[at]=0;
}
static int history_utterance(const char *input){
 if(!input)return 0;
 while(*input==' ')input++;
 return starts_no_case(input,"doc put ")||starts_no_case(input,"doc begin ")||(starts(input,"history")&&(input[7]==0||input[7]==' '))||starts(input,"model import ")||starts(input,"model export ")||starts(input,"train export ");
}
int holly_turn(struct holly_session *s,const char *input,holly_emit_fn emit,void *context){
 if(!s||!input||!emit)return -1;
 struct turn_capture capture={emit,context,0,{0}};
 int result=holly_turn_inner(s,input,capture_emit,&capture);
 if(result>=0&&capture.used&&!starts_no_case(input,"doc put "))observe_reply(capture.output);
 if(result>=0&&capture.used&&s->dialogue_enabled){
  const char *p=input;while(*p==' ')p++;
  if(!starts_no_case(p,"brain ")&&!starts_no_case(p,"chat ")&&!starts_no_case(p,"train")&&!starts_no_case(p,"model")&&!starts_no_case(p,"memory")&&
     !starts_no_case(p,"wiki ")&&!starts_no_case(p,"teach ")&&!starts_no_case(p,"remember ")&&!starts_no_case(p,"history")&&
     !same(p,"storage")&&!same(p,"status")&&!same(p,"help")&&!same(p,"version")&&
     !starts_no_case(p,"doc ")&&!starts_no_case(p,"find ")&&!starts_no_case(p,"telnet")&&!starts_no_case(p,"http")&&!starts_no_case(p,"dwarf")&&!starts_no_case(p,"personality ")&&!starts(capture.output,"Holly [Red Dwarf]: ")&&
     !same(p,"storagediag")&&!same(p,"displaydiag")&&!starts_no_case(p,"display ")){
   dialogue_context(s,p,capture.output);
  }
 }
 if(result>=0&&history_append&&!history_utterance(input)&&capture.used&&
    history_append(input,capture.output,history_context))
  emit("Holly: I couldn't save this turn to the conversation log.\n",context);
 return result;
}
static int command_remember(char *line,holly_emit_fn emit,void *context){
 if(!memory_commands_available()){
  say(emit,context,"Holly: Persistent memory is unavailable right now.\n");return 0;
 }
 char *text=trim(line+9),*source="user";unsigned confidence=100;
 for(char *p=text;*p;p++)if(*p=='|'){
  *p=0;text=trim(text);source=trim(p+1);
  char *second=0;for(char *q=source;*q;q++)if(*q=='|'){*q=0;second=trim(q+1);break;}
  if(second){const char *end=0;if(parse_number(second,&confidence,&end)||!end)confidence=101;
   else {while(*end==' ')end++;if(*end)confidence=101;}
  }
  break;
 }
 if(confidence>100u||!text[0]||!source[0]||text_length(text,HOLLY_MEMORY_TEXT_SIZE)>=HOLLY_MEMORY_TEXT_SIZE||
    text_length(source,HOLLY_MEMORY_SOURCE_SIZE)>=HOLLY_MEMORY_SOURCE_SIZE){
  say(emit,context,"Holly: Use remember fact, or remember fact | source | confidence(0-100).\n");return 0;
 }
 uint32_t id=0;int result=memory_ops->add(text,source,confidence,&id,memory_context);
 if(result){say(emit,context,"Holly: I couldn't save that memory; it may be too long or memory may be full.\n");return 0;}
 say(emit,context,"Holly: Saved memory #");emit_number(emit,context,id);
 say(emit,context," with ");emit_number(emit,context,confidence);
 say(emit,context,"% confidence, source ");say(emit,context,source);say(emit,context,".\n");return 0;
}
/* Imports only short attributed excerpts from the authenticated SSH user.
 * The fetcher validates HTTPS and supplies the canonical article URL. */
static int command_wiki_import(char *line,holly_emit_fn emit,void *context){
 if(!memory_commands_available()){
  say(emit,context,"Holly: Persistent memory is unavailable right now.\n");return 0;
 }
 char *fact=trim(line+12),*source=0;
 for(char *p=fact;*p;p++)if(*p=='|'){*p=0;source=trim(p+1);break;}
 if(source)fact=trim(fact);
 static const char prefix[]="https://en.wikipedia.org/wiki/";
 unsigned size=source?text_length(source,HOLLY_MEMORY_SOURCE_SIZE):0;
 int valid=source&&starts(source,prefix)&&size>sizeof(prefix)-1u&&
           size<HOLLY_MEMORY_SOURCE_SIZE&&*fact&&
           text_length(fact,HOLLY_MEMORY_TEXT_SIZE)<HOLLY_MEMORY_TEXT_SIZE;
 if(valid)for(unsigned i=sizeof(prefix)-1u;i<size;i++){
  char c=source[i];
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
       c=='_'||c=='-'||c=='.'||c=='('||c==')'||c=='%'||c=='~'))valid=0;
 }
 if(valid)for(char *p=fact;*p;p++)if(*p=='|'||((unsigned char)*p<32))valid=0;
 if(!valid){
  say(emit,context,"Holly: Use wiki import short excerpt | https://en.wikipedia.org/wiki/Article_Title.\n");return 0;
 }
 unsigned count=memory_ops->count(memory_context);
 for(unsigned i=0;i<count;i++){
  struct holly_memory_item item;
  if(!memory_ops->get_at(i,&item,memory_context)&&same(item.source,source)&&same_fact(item.text,fact)){
   say(emit,context,"Holly: That Wikipedia excerpt is already memory #");
   emit_number(emit,context,item.id);say(emit,context,".\n");return 0;
  }
 }
 uint32_t id=0;
 if(memory_ops->add(fact,source,45u,&id,memory_context)){
  say(emit,context,"Holly: I couldn't save the Wikipedia excerpt to the Vault.\n");return 0;
 }
 say(emit,context,"Holly: Saved Wikipedia excerpt as memory #");emit_number(emit,context,id);
 say(emit,context," at 45% unverified confidence. Source: ");say(emit,context,source);
 say(emit,context,". Review or correct it with memory show/correct/forget.\n");return 0;
}
static int command_memory(char *line,holly_emit_fn emit,void *context){
 if(!memory_commands_available()){
  say(emit,context,"Holly: Persistent memory is unavailable right now.\n");return 0;
 }
 char *p=line+6;while(*p==' ')p++;
 if(starts(p,"list")&&(p[4]==0||p[4]==' ')){
  p+=4;while(*p==' ')p++;unsigned page=1;const char *end=0;
  if(*p){if(parse_number(p,&page,&end)||!page){say(emit,context,"Holly: Use memory list [page].\n");return 0;}
   while(*end==' ')end++;
   if(*end){say(emit,context,"Holly: Use memory list [page].\n");return 0;}}
  unsigned count=memory_ops->count(memory_context),pages=count?(count+4u)/5u:1u;
  if(page>pages)page=pages;
  unsigned start=(page-1u)*5u;
  say(emit,context,"Holly: Memories ");emit_number(emit,context,count);
  say(emit,context," total; page ");emit_number(emit,context,page);
  say(emit,context," of ");emit_number(emit,context,pages);say(emit,context,".\n");
  unsigned end_index=start+5u;if(end_index>count)end_index=count;
  for(unsigned i=start;i<end_index;i++){
   struct holly_memory_item item;if(!memory_ops->get_at(i,&item,memory_context))emit_memory_item(emit,context,&item,1);
  }
  if(!count)say(emit,context,"Holly: No saved facts yet. Use remember to teach me.\n");
  return 0;
 }
 if(starts(p,"find ")){
  char *query=trim(p+5);if(!query[0]){say(emit,context,"Holly: Give me a few words to search for.\n");return 0;}
  struct holly_memory_item top[5];unsigned scores[5]={0,0,0,0,0};unsigned used=0;
  unsigned count=memory_ops->count(memory_context);
  for(unsigned i=0;i<count;i++){
   struct holly_memory_item item;if(memory_ops->get_at(i,&item,memory_context))continue;
   unsigned score=memory_match_score(query,item.text,0,0);if(!score)continue;
   unsigned at=0;while(at<used&&scores[at]>=score)at++;
   if(at>=5)continue;
   if(used<5)used++;
   for(unsigned j=used-1;j>at;j--){top[j]=top[j-1];scores[j]=scores[j-1];}
   top[at]=item;scores[at]=score;
  }
  if(!used){say(emit,context,"Holly: I couldn't find a saved fact matching that.\n");return 0;}
  say(emit,context,"Holly: Memory matches for ");say(emit,context,query);say(emit,context,":\n");
  for(unsigned i=0;i<used;i++){say(emit,context,"  ");emit_memory_item(emit,context,&top[i],1);}
  return 0;
 }
 if(starts(p,"show ")){
  unsigned id=0;const char *end=0;if(parse_number(p+5,&id,&end)||!id){say(emit,context,"Holly: Use memory show ID.\n");return 0;}
  while(*end==' ')end++;
  struct holly_memory_item item;
  if(*end||memory_ops->get(id,&item,memory_context)){say(emit,context,"Holly: I have no memory with that ID.\n");return 0;}
  say(emit,context,"Holly: Memory #");emit_number(emit,context,item.id);
  say(emit,context," revision ");emit_number(emit,context,item.revision);
  say(emit,context,", confidence ");emit_number(emit,context,item.confidence);
  say(emit,context,"%, source ");say(emit,context,item.source);
  say(emit,context,".\n");say(emit,context,item.text);say(emit,context,"\n");return 0;
 }
 if(starts(p,"correct ")){
  char *arguments=p+8,*arrow=0;for(char *q=arguments;*q;q++)if(q[0]=='='&&q[1]=='>'){arrow=q;break;}
  if(!arrow){say(emit,context,"Holly: Use memory correct ID => corrected fact.\n");return 0;}
  *arrow=0;char *id_text=arguments;while(*id_text==' ')id_text++;
  const char *end=0;unsigned id=0;
  if(parse_number(id_text,&id,&end)){say(emit,context,"Holly: Use memory correct ID => corrected fact.\n");return 0;}
  while(*end==' ')end++;
  char *corrected=trim(arrow+2);
  if(*end||!corrected[0]||text_length(corrected,HOLLY_MEMORY_TEXT_SIZE)>=HOLLY_MEMORY_TEXT_SIZE){
   say(emit,context,"Holly: The ID or corrected fact is invalid or too long.\n");return 0;
  }
  if(memory_ops->update(id,corrected,"user correction",100,memory_context)){
   say(emit,context,"Holly: I couldn't correct that memory; check its ID.\n");return 0;
  }
  say(emit,context,"Holly: Corrected memory #");emit_number(emit,context,id);
  say(emit,context,". I trust your correction at 100%.\n");return 0;
 }
 if(starts(p,"confidence ")){
  unsigned id=0,confidence=0;const char *end=0,*after=0;
  const char *arguments=p+10;while(*arguments==' ')arguments++;
  if(parse_number(arguments,&id,&end)||!id){say(emit,context,"Holly: Use memory confidence ID 0-100.\n");return 0;}
  while(*end==' ')end++;
  if(parse_number(end,&confidence,&after)||confidence>100u){say(emit,context,"Holly: Confidence must be from 0 to 100.\n");return 0;}
  while(*after==' ')after++;
  struct holly_memory_item item;
  if(*after||memory_ops->get(id,&item,memory_context)){say(emit,context,"Holly: I have no memory with that ID.\n");return 0;}
  if(memory_ops->update(id,item.text,item.source,confidence,memory_context)){
   say(emit,context,"Holly: I couldn't update that confidence.\n");return 0;
  }
  say(emit,context,"Holly: Confidence for memory #");emit_number(emit,context,id);
  say(emit,context," is now ");emit_number(emit,context,confidence);say(emit,context,"%.\n");return 0;
 }
 if(starts(p,"forget ")){
  unsigned id=0;const char *end=0;
  if(parse_number(p+7,&id,&end)||!id){say(emit,context,"Holly: Use memory forget ID.\n");return 0;}
  while(*end==' ')end++;
  if(*end){say(emit,context,"Holly: Use memory forget ID.\n");return 0;}
  int result=memory_ops->forget(id,memory_context);
  if(result==-3){say(emit,context,"Holly: That fact is forgotten, but an old sector could not be cleared.\n");return 0;}
  if(result){say(emit,context,"Holly: I have no memory with that ID.\n");return 0;}
  say(emit,context,"Holly: Forgotten memory #");emit_number(emit,context,id);say(emit,context,".\n");return 0;
 }
 say(emit,context,"Holly: Memory commands: memory list [page], memory find words, memory show ID, memory correct ID => fact, memory confidence ID 0-100, memory forget ID.\n");
 return 0;
}
int holly_command(char *line,holly_emit_fn emit,void *context) {
 if(!line||!emit)return -1;
 if(same(line,"version")){say(emit,context,"Holly AI Learning OS 0.49.29 - American male Queeg voice\n");return 0;}
 if(same(line,"networkdiag")||same(line,"storagediag")||same(line,"displaydiag")||starts(line,"display ")){
  if(diagnostics)return diagnostics(line,emit,context);
  say(emit,context,"Hardware diagnostics unavailable in this test host.\n");return 0;
 }
 if(starts(line,"train ")||same(line,"train")||starts(line,"model import ")||starts(line,"model export ")||same(line,"model rollback"))
  return holly_training_command(line,emit,context,memory_ops,memory_context);
 if(same(line,"model")||same(line,"model status")) {
  say(emit,context,"Holly Seed-1: 39,986 trained parameters; 64-character context; integer inference. Experimental text completion, not reliable conversation. Model checkpoints and output-layer training available: train status.\n");return 0;
 }
 if(starts(line,"model generate ")) {
  char generated[HOLLY_LM_LIMIT+1];
  if(holly_lm_generate(line+15,generated,sizeof(generated))<0) {
   say(emit,context,"Holly: Use a printable ASCII prompt up to 256 characters.\n");return 0;
  }
  say(emit,context,"Holly [experimental completion]: ");
  say(emit,context,generated[0]?generated:"[end of text]");say(emit,context,"\n");return 0;
 }
 if(starts(line,"model ")) {say(emit,context,"Use model status or model generate <text>.\n");return 0;}
 if(starts(line,"remember "))return command_remember(line,emit,context);
 if(starts(line,"wiki import "))return command_wiki_import(line,emit,context);
 if(starts(line,"memory")&&(line[6]==0||line[6]==' '))return command_memory(line,emit,context);
 if(starts(line,"teach ")) {
  char *q=line+6,*sep=0;
  for(char *p=q;*p;p++) if(p[0]==' '&&p[1]=='='&&p[2]=='>'&&p[3]==' ') {sep=p;break;}
  if(!sep){say(emit,context,"Holly: Try teach question => answer. I need both halves.\n");return 0;}
  *sep=0;
  int result=mind_teach(q,sep+4);
  if(result>=0)say(emit,context,"Holly: Right. I have that in memory.\n");
  else if(result==-3)say(emit,context,"Holly: I learned that, but the Holly Vault could not save it.\n");
  else say(emit,context,"Holly: That lesson is too long, or my memory is full.\n");
  return 0;
 }
 if(starts(line,"ask ")) {
  int index=mind_ask(line+4);
  if(index<0){
   if(!emit_memory_recall(line+4,0,emit,context))say(emit,context,mind_persistence_enabled()?
       "Holly: Please give me a little more detail.\n":
       "Holly: Please give me a little more detail.\n");
  }else {say(emit,context,"Holly: ");say(emit,context,lessons[index].answer);say(emit,context,"\n");}
  return 0;
 }
 if(starts(line,"list")) {
  if(!lesson_count)say(emit,context,"Holly: No lessons yet. Rather spacious in here.\n");
  for(unsigned i=0;i<lesson_count;i++){say(emit,context,"- ");say(emit,context,lessons[i].question);say(emit,context,"\n");}
  return 0;
 }
 if(starts(line,"status")) {
  say(emit,context,mind_persistence_enabled()&&history_get&&memory_commands_available()?
      "Holly: Chat active. Lessons, reviewed memories, and conversation history are persistent.\n":
      mind_persistence_enabled()&&memory_commands_available()?
      "Holly: Lessons and reviewed memories persist; conversation history is unavailable.\n":
      mind_persistence_enabled()?
      "Holly: Learned lessons persist; reviewed memory is unavailable.\n":
      "Holly: Chat active. Learning in RAM; lessons reset on reboot. Persistent storage failed to start; run storagediag.\n");
  return 0;
 }
 if(starts(line,"storage")||starts(line,"persist")) {
  if(!mind_persistence_enabled())say(emit,context,"Holly: Holly Vault is unavailable; lessons are RAM-only.\n");
  else {
   say(emit,context,"Holly: Holly Vault online. SD capacity is about ");
   uint64_t gib=((uint64_t)storage_card_sectors*512u+(1ull<<29))>>30;
   char digits[12];unsigned used=0;
   do{digits[used++]=(char)('0'+gib%10u);gib/=10u;}while(gib&&used<sizeof(digits));
   while(used){char digit[2]={digits[--used],0};say(emit,context,digit);}
   say(emit,context," GiB. Learned lessons persist");
   if(memory_commands_available()){
    say(emit,context,"; reviewed memories: ");
    emit_number(emit,context,memory_ops->count(memory_context));
   }
   if(history_get){
    say(emit,context,"; saved conversation turns: ");
    unsigned count=history_count?history_count(history_context):0;char number[11];unsigned n=0;
    do{number[n++]=(char)('0'+count%10u);count/=10u;}while(count&&n<sizeof(number));
    while(n){char digit[2]={number[--n],0};say(emit,context,digit);}
   }else say(emit,context,"; conversation history is unavailable");
   say(emit,context,".\n");
  }
  return 0;
 }
 if(starts(line,"history")&&(line[7]==0||line[7]==' ')) {
  unsigned wanted=5;
  const char *p=line+7;while(*p==' ')p++;
  if(*p){wanted=0;while(*p>='0'&&*p<='9'){if(wanted<100)wanted=wanted*10u+(unsigned)(*p-'0');p++;}
   if(*p||!wanted){say(emit,context,"Holly: Try history or history 1-10.\n");return 0;}
   if(wanted>10)wanted=10;
  }
  if(!history_get||!history_count){say(emit,context,"Holly: Conversation history is unavailable.\n");return 0;}
  unsigned available=history_count(history_context),shown=available<wanted?available:wanted;
  if(!shown){say(emit,context,"Holly: No saved conversation turns yet.\n");return 0;}
  say(emit,context,"Holly: Recent conversation turns, oldest first.\n");
  char saved_input[HOLLY_HISTORY_INPUT_SIZE];
  char saved_output[HOLLY_HISTORY_CAPTURE_SIZE];
  for(unsigned back=shown;back>0;back--){
   if(history_get(back-1u,saved_input,sizeof(saved_input),saved_output,sizeof(saved_output),history_context))continue;
   say(emit,context,"You: ");say(emit,context,saved_input);say(emit,context,"\n");
   say(emit,context,saved_output);
   unsigned n=0;while(saved_output[n])n++;if(n&&saved_output[n-1]!='\n')say(emit,context,"\n");
  }
  return 0;
 }
 if(starts(line,"help")) {
  say(emit,context,"reading status/list/select ID/clear/pause/resume | brain on/off/status/reset | source | discussion on/off/status | chat model 1/3/4 | telnet on/off/status | http on/off/status | doc format/init/status/stats/list | doc search <phrase> | script coverage/list/ask | find <phrase> | dwarf on/off/status/topics/source/reset | dwarf <topic> | more | personality on/off/status | chat on/off/status/reset | version | storagediag | displaydiag | train status | train approve ID train/valid/test | train start STEPS | model rollback | model status | model generate text | hello | teach question => answer | remember fact | wiki time/read/status/cancel | wiki import excerpt | URL | memory list/find/show/correct/confidence/forget | ask question | why that? | correct that => fact | that's right/wrong | learning on/off/status | list | history [1-10] | status | storage | repeat that | reading status/list/select/clear | what time is it | take a note | recall my notes | set an alarm in 5 minutes | alarm status | read me todays world news | news status/source | search <topic> | search status/source/cache/on/off | search refresh <topic> | search forget <topic> | help\n");
  return 0;
 }
 if(line[0])say(emit,context,"Holly: I don't have a command for that. Try help.\n");
 return 0;
}

static int conversation_only_inner(struct holly_session *s,const char *input,holly_emit_fn emit,void *ctx){
 if(!s||!input||!emit)return -1;
 char line[320];unsigned n=text_length(input,sizeof line);
 if(n>=sizeof line)return -1;
 copy(line,input,sizeof line);char *q=trim(line);
 if(!*q)return 0;
 if(same(q,"exit")||same(q,"quit")){say(emit,ctx,"Holly: Bye.\n");return -1;}
 if(same(q,"help")){say(emit,ctx,"Conversation only: hello | dwarf <topic> | more | discussion on/off | chat on/off/reset | personality on/off | find <document phrase> | exit. Training, uploads, personal memories and administration require SSH.\n");return 0;}
 if(same(q,"discussion on")||same(q,"discussion off")||same(q,"discussion status")){
  if(same(q,"discussion on"))s->discussion_enabled=1;
  if(same(q,"discussion off"))s->discussion_enabled=0;
  say(emit,ctx,s->discussion_enabled?"Holly: Red Dwarf discussion is on.\n":"Holly: Red Dwarf discussion is off.\n");return 0;
 }
 if(same(q,"chat model 1")||same(q,"chat model 3")||same(q,"chat model 4")){s->dialogue_model=(unsigned)(q[11]-'0');s->dialogue_previous[0]=0;say(emit,ctx,"Holly: Dialogue model selected for this guest session.\n");return 0;}
 if(same(q,"chat on")){s->dialogue_enabled=1;s->dialogue_previous[0]=0;say(emit,ctx,"Holly: Experimental chat on.\n");return 0;}
 if(same(q,"chat off")){s->dialogue_enabled=0;say(emit,ctx,"Holly: Chat off; ship-computer banter available.\n");return 0;}
 if(same(q,"chat reset")){discussion_reset(s);say(emit,ctx,"Holly: Session context cleared.\n");return 0;}
 if(same(q,"personality on")||same(q,"personality off")||same(q,"personality status")){if(!same(q,"personality status"))s->personality_enabled=same(q,"personality on");say(emit,ctx,s->personality_enabled?"Holly: Deadpan personality is on. Chat can still generate replies to other conversation.\n":"Holly: Personality off.\n");return 0;}
 const char *blocked[]={"train","model","teach","remember","memory","learning","correct","wiki","doc","telnet","http","display","storagediag","history","storage","persist","version","status","list"};
 for(unsigned i=0;i<sizeof(blocked)/sizeof(blocked[0]);i++){
  unsigned k=text_length(blocked[i],32);
  if(starts_no_case(q,blocked[i])&&(!q[k]||q[k]==' ')){say(emit,ctx,"Holly: That requires SSH. Telnet is conversation only.\n");return 0;}
 }
 if(starts_no_case(q,"find ")){
  if(document_search)return document_search(q+5,emit,ctx),0;
  say(emit,ctx,"Holly: Document bank unavailable.\n");return 0;
 }
 if(starts_no_case(q,"script ")){
  if(document_script)return document_script(q+7,emit,ctx),0;
  say(emit,ctx,"Holly: Episode script bank unavailable.\n");return 0;
 }
 s->turns++;
 if(!s->note_pending&&holly_search_command(s,q,0,emit,ctx))return 0;
 if(!s->note_pending&&holly_news_command(q,emit,ctx))return 0;
 if(assistant_reply(s,q,0,emit,ctx))return 0;
 if(!s->note_pending&&holly_arithmetic(s,q,emit,ctx))return 0;
 normalize_kryten(q,(unsigned)(sizeof line-(q-line)));
 if(reading_reply(s,q,0,emit,ctx))return 0;
 if(conversational_reply(s,q,0,emit,ctx))return 0;
 if(personality_answer(s,q,emit,ctx))return 0;
 if(discussion_answer(s,q,emit,ctx))return 0;
 if(same(q,"repeat that")){say(emit,ctx,s->last_answer[0]?s->last_answer:"Holly: Nothing to repeat yet.");say(emit,ctx,"\n");return 0;}
 if(same(q,"hello")||same(q,"hi")){say(emit,ctx,"Holly: Hello. Ship computer here. What's on your mind?\n");return 0;}
 /* SSH-taught question/answer lessons are shared conversational knowledge.
  * Guest sessions cannot teach, mutate them or read personal memory records. */
 int lesson=mind_ask(q);
 if(lesson>=0){
  holly_reference_clear(&s->reference);holly_lore_clear(&s->lore);
  s->series_focus=0;s->episode_focus[0]=0;s->discussion_profile=0;s->last_memory_id=0;
  copy(s->topic,lessons[lesson].question,sizeof s->topic);
  copy(s->last_answer,lessons[lesson].answer,sizeof s->last_answer);
  s->expression=HOLLY_SPEAKING;
  say(emit,ctx,"Holly: ");say(emit,ctx,s->last_answer);say(emit,ctx,"\n");return 0;
 }
 if(reference_answer(s,q,emit,ctx)||lore_answer(s,q,emit,ctx))return 0;
 if(holly_search_fallback(s,q,emit,ctx))return 0;
 holly_lore_clear(&s->lore);
 char reply[HOLLY_DIALOGUE_OUTPUT];
 if(s->dialogue_enabled&&s->persona!=2&&!chosen_generate(s,q,reply,sizeof reply)){
  say(emit,ctx,"Holly (experimental): ");say(emit,ctx,reply);say(emit,ctx,"\n");
  copy(s->dialogue_previous,q,sizeof s->dialogue_previous);
  unsigned at=text_length(s->dialogue_previous,sizeof s->dialogue_previous);
  if(at+1<sizeof s->dialogue_previous)s->dialogue_previous[at++]=' ';
  copy(s->dialogue_previous+at,reply,sizeof s->dialogue_previous-at);
 }else say(emit,ctx,s->personality_enabled?"Holly: Please give me a little more detail. Personal details stay in our conversation; use search on for public topics.\n":"Holly: Please give me a little more detail. Personal details stay in our conversation; use search on for public topics.\n");
 return 0;
}
int holly_conversation_only(struct holly_session *s,const char *input,holly_emit_fn emit,void *ctx){
 if(!s||!input||!emit)return -1;
 struct turn_capture capture={emit,ctx,0,{0}};
 int result=conversation_only_inner(s,input,capture_emit,&capture);
 if(result>=0&&capture.used)observe_reply(capture.output);
 if(result>=0&&capture.used&&s->dialogue_enabled){
  const char *p=input;while(*p==' ')p++;
  const char *commands[]={"brain ","chat ","personality ","discussion ","find ","script ","dwarf ","source","help"};
  unsigned command=0;
  for(unsigned i=0;i<sizeof commands/sizeof commands[0];i++)if(starts_no_case(p,commands[i]))command=1;
  if(!command&&!starts(capture.output,"Holly: That requires SSH."))dialogue_context(s,p,capture.output);
 }
 return result;
}
