#include "reference.h"
#include "lore.h"
/* Deterministic, bounded lexical retrieval. No model or unsupported generated facts. */
static holly_reference_documents_fn documents;static void *storage;
void holly_reference_set_documents(holly_reference_documents_fn fn,void *ctx){documents=fn;storage=ctx;}
static unsigned length(const char *s){unsigned n=0;while(s[n])n++;return n;}
static void copy(char *d,const char *s,unsigned n){unsigned i=0;if(!n)return;while(s[i]&&i+1<n){d[i]=s[i];i++;}d[i]=0;}
static void add(char *d,const char *s,unsigned n){unsigned i=length(d);if(i<n)copy(d+i,s,n-i);}
static int eq(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int prefix(const char *a,const char *b){while(*b)if(*a++!=*b++)return 0;return 1;}
static char lower(char c){return c>='A'&&c<='Z'?(char)(c+32):c;}
static int letter(char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9');}
static void normalize(const char *s,char *d,unsigned cap){unsigned at=0;int space=0;for(unsigned i=0;s[i]&&at+1<cap;i++){char c=lower(s[i]);if(letter(c)){if(space&&at&&at+2<cap)d[at++]=' ';d[at++]=c;space=0;}else space=1;}d[at]=0;}
static int phrase(const char *q,const char *p){unsigned n=length(p);if(!n)return 0;for(unsigned i=0;q[i];i++)if(!i||q[i-1]==' '){unsigned j=0;while(j<n&&q[i+j]==p[j])j++;if(j==n&&(!q[i+j]||q[i+j]==' '))return 1;}return 0;}
struct words {char token[32][24];unsigned n;};
static const char *groups[][9]={
 {"contain","contains","contained",0},
 {"survive","survives","survived","surviving","survival",0},
 {"disaster","accident","leak","catastrophe",0},
 {"actor","plays","played","portrays","portrayed","actress","cast","voices",0},
 {"birth","born","baby","birthplace",0},
 {"create","created","creator","invent","invents","invented","inventor",0},
 {"hologram","holographic",0},
 {"computer","computers",0},
 {"intelligence","iq",0},
 {"shuttle","shuttlecraft","shuttles",0},
 {"death","dead","died","dies","killed",0},
 {"origin","origins","descended","evolved","evolve",0},
 {"leave","leaves","left",0},
 {"senility","senile",0},
 {"father","dad",0},
 {"universe","universes",0}};
static const char *canon(const char *w){
 for(unsigned i=0;i<sizeof groups/sizeof groups[0];i++)for(unsigned j=0;groups[i][j];j++)if(eq(w,groups[i][j]))return groups[i][0];
 return w;
}
static int stop(const char *w){
 const char *list="a an the is are was were be been being of to in on at for from with by and or about me you your my i we us it its he his him she her they their them who what which where when why how did does do can could would should please tell explain describe know really exactly about have has had happen happened happens then also this that those these more compare difference between versus vs s red dwarf";
 return phrase(list,w);
}
static int has(const struct words *w,const char *t){for(unsigned i=0;i<w->n;i++)if(eq(w->token[i],t))return 1;return 0;}
static void tokens(const char *s,unsigned size,struct words *w,int filter){w->n=0;for(unsigned i=0;i<size&&s[i];){char b[24];unsigned n=0;while(i<size&&s[i]&&!letter(lower(s[i])))i++;while(i<size&&letter(lower(s[i]))){if(n+1<sizeof b)b[n++]=lower(s[i]);i++;}b[n]=0;if(n&&(!filter||!stop(b))){const char *t=canon(b);if(!has(w,t)&&w->n<32)copy(w->token[w->n++],t,24);}}}
void holly_reference_clear(struct holly_reference *s){s->topic=-1;s->last_key=0;s->seen_count=0;s->subject[0]=s->answer[0]=s->sources[0]=0;}
void holly_reference_init(struct holly_reference *s){s->enabled=1;s->dry_voice=1;s->banter_state=0x484f4c4cu;s->banter_last=~0u;holly_reference_clear(s);}
/* Keep bibliography available through source, but out of normal spoken replies.
 * This is presentation cleanup of retrieved text, not generated paraphrasing. */
static void spoken_passage(char *out,const char *text,unsigned cap){
 for(unsigned i=0;text[i];){
  if(prefix(text+i,"https://")||prefix(text+i,"http://")||prefix(text+i,"www.")){
   while(text[i]&&text[i]!=' '&&text[i]!='\n'&&text[i]!='\t')i++;
   continue;
  }
  if(text[i]=='['){
   unsigned j=i+1;if(text[j]=='S')j++;
   unsigned start=j;while(text[j]>='0'&&text[j]<='9')j++;
   if(j>start&&text[j]==']'){i=j+1;continue;}
  }
  char c[2]={text[i++],0};add(out,c,cap);
 }
 unsigned n=length(out);while(n&&(out[n-1]==' '||out[n-1]=='\n'))out[--n]=0;
}
static void occasional_banter(struct holly_reference *s,const char *q,char *out,unsigned cap){
 if(!s->dry_voice)return;
 unsigned x=s->banter_state;
 for(unsigned i=0;q[i];i++)x=(x^(unsigned char)q[i])*16777619u;
 x^=x<<13;x^=x>>17;x^=x<<5;s->banter_state=x;
 if((x&3u)!=0)return;
 static const char *remarks[]={
  "It's brown trouser time.",
  "I've given that some thought. Quite tiring, thought.",
  "Space is enormous. Our staffing budget isn't.",
  "Still, at least nobody's asked me to parallel park the ship.",
  "Another mystery sorted. I'll invoice the universe later."
 };
 unsigned pick=(x>>8)%5u;if(pick==s->banter_last)pick=(pick+1u)%5u;
 const char *remark=remarks[pick];if(length(out)+length(remark)+2u>=cap)return;
 add(out," ",cap);add(out,remark,cap);s->banter_last=pick;
}
void holly_reference_banter(struct holly_reference *s,const char *q,char *out,size_t cap){
 if(s&&q&&out)occasional_banter(s,q,out,(unsigned)cap);
}
static unsigned named(const char *q,const char *aliases){unsigned best=0;char part[96];while(*aliases){unsigned n=0;while(*aliases&&*aliases!='|'){if(n+1<sizeof part)part[n++]=*aliases;aliases++;}part[n]=0;if(*aliases)aliases++;if(phrase(q,part)&&n>best)best=n;}return best;}
static unsigned named(const char *,const char *);
struct rank {struct words query;char title[81],source[240],text[401],anchor[81],aliases[192];unsigned score,key,skip;const struct holly_reference *previous;};
static void offer(const char *text,unsigned size,const char *title,const char *source,unsigned key,void *ctx){
 struct rank *r=ctx;if(key==r->skip)return;
 if(r->previous)for(unsigned i=0;i<r->previous->seen_count;i++)if(r->previous->seen[i]==key)return;
 if(size>400)size=400;
 char body[401];for(unsigned i=0;i<size;i++)body[i]=text[i];body[size]=0;
 struct words passage;tokens(body,size,&passage,0);
 if(r->anchor[0]){char normalized[512],heading[96];normalize(body,normalized,sizeof normalized);normalize(title,heading,sizeof heading);if(!named(normalized,r->aliases)&&!named(heading,r->aliases))return;}

 unsigned hits=0;for(unsigned i=0;i<r->query.n;i++)if(has(&passage,r->query.token[i]))hits++;
 /* Requiring every content term avoids answering a different question merely
    because it names the same character. This is deliberately conservative. */
 if(hits<r->query.n)return;
 unsigned score=20+hits*20;if(r->anchor[0])score+=10;
 if(score<=r->score)return;
 r->score=score;r->key=key;copy(r->text,body,sizeof r->text);copy(r->title,title,sizeof r->title);copy(r->source,source,sizeof r->source);
}
static int question(const char *q){const char *p[]={"who ","what ","where ","when ","why ","how ","tell ","explain ","describe ","compare ","do you know ","and ","was ","is ","did ","does ","which "};for(unsigned i=0;i<sizeof p/sizeof p[0];i++)if(prefix(q,p[i]))return 1;return 0;}
int holly_reference_reply(struct holly_reference *s,const char *input,char *out,size_t cap){
 if(!s||!input||!out||cap<HOLLY_REFERENCE_REPLY)return -1;
 if(length(input)>319)return -1;
 char q[320];normalize(input,q,sizeof q);out[0]=0;
 if(eq(q,"brain on")||eq(q,"brain off")||eq(q,"brain reset")||eq(q,"brain status")){
  if(eq(q,"brain on"))s->enabled=1;
  if(eq(q,"brain off"))s->enabled=0;
  if(eq(q,"brain reset"))holly_reference_clear(s);
  copy(out,s->enabled?"Red Dwarf question retrieval is on. ":"Red Dwarf question retrieval is off. ",cap);add(out,"Cited passages, session follow-ups and conservative unknown answers. Extractive retrieval; no new neural training.",cap);return 1;
 }
 if(!s->enabled)return 0;
 if(eq(q,"source")||eq(q,"sources")||eq(q,"where did you get that")||eq(q,"what is your source")){
  copy(out,s->sources[0]?s->sources:"Ask a Red Dwarf question first so I have a source to show.",cap);return 1;
 }
 if(eq(q,"repeat that")&&s->answer[0]){copy(out,s->answer,cap);return 1;}
 if(prefix(q,"dwarf ")||eq(q,"dwarf"))return 0;
 int more=eq(q,"more")||eq(q,"tell me more")||eq(q,"go on")||eq(q,"what else");
 int pronoun=phrase(q,"him")||phrase(q,"her")||phrase(q,"he")||phrase(q,"she")||phrase(q,"it")||phrase(q,"they")||prefix(q,"and ");
 if(!more&&!question(q))return 0;
 int selected[2]={-1,-1};unsigned count=0;
 unsigned matches[64]={0};
 for(unsigned i=1;i<holly_lore_count();i++){
  const char *title,*aliases,*text,*source;holly_lore_passage(i,0,&title,&aliases,&text,&source);
  unsigned hit=named(q,aliases);
  if(i<64){matches[i]=hit;}
 }
 for(unsigned i=1;i<holly_lore_count()&&i<64;i++){
  if(!matches[i])continue;
  /* Prefer a longer compound topic over a contained name (Ace Rimmer/Rimmer). */
  int contained=0;
  const char *t,*a,*x,*u;holly_lore_passage(i,0,&t,&a,&x,&u);
  for(unsigned j=1;j<holly_lore_count()&&j<64;j++)if(matches[j]>matches[i]){const char *jt,*ja,*jx,*ju;holly_lore_passage(j,0,&jt,&ja,&jx,&ju);char norm[96];normalize(jt,norm,sizeof norm);if(named(norm,a))contained=1;}
  if(!contained){if(count<2)selected[count]=(int)i;count++;}
 }
 int explicit_domain=phrase(q,"red dwarf");
 if(count>2){copy(out,"Please narrow that to one or two Red Dwarf subjects.",cap);return 1;}
 if(!count&&(pronoun||more)&&s->subject[0]){selected[count++]=s->topic;}
 if(!count&&explicit_domain){selected[count++]=0;}
 if(!count){if(pronoun||more){copy(out,"Which Red Dwarf character, episode or object do you mean?",cap);return 1;}holly_reference_clear(s);return 0;}
 if(count==2&&!(prefix(q,"compare ")||phrase(q,"difference")||phrase(q,"versus")||phrase(q,"vs"))){copy(out,"I can see two Red Dwarf subjects. Which one should I focus on, or would you like to compare them?",cap);return 1;}
 char source_list[512]="";unsigned success=0;int newtopic=selected[0];unsigned newkey=0;char newsubject[81]="";
 for(unsigned choice=0;choice<count;choice++){
  struct rank r={0};const char *title="",*aliases="",*text="",*source="";
  if(selected[choice]>=0)holly_lore_passage((unsigned)selected[choice],0,&title,&aliases,&text,&source);else title=s->subject;
  copy(r.anchor,title,sizeof r.anchor);copy(r.aliases,aliases,sizeof r.aliases);if(selected[choice]==0)r.anchor[0]=0;
  struct words query,entity;tokens(q,length(q),&query,1);tokens(aliases,length(aliases),&entity,1);
  if(count==2){const char *t,*a,*x,*u;holly_lore_passage((unsigned)selected[1-choice],0,&t,&a,&x,&u);struct words other;tokens(a,length(a),&other,1);for(unsigned i=0;i<other.n&&entity.n<32;i++)if(!has(&entity,other.token[i]))copy(entity.token[entity.n++],other.token[i],24);}
  if(!more)for(unsigned i=0;i<query.n;i++)if(!has(&entity,query.token[i]))copy(r.query.token[r.query.n++],query.token[i],24);
  if(more){r.skip=s->last_key;r.previous=s;}
  if(selected[choice]>=0)for(unsigned page=0;holly_lore_passage((unsigned)selected[choice],page,&title,&aliases,&text,&source);page++)offer(text,length(text),title,source,1+(unsigned)selected[choice]*4+page,&r);
  if(r.query.n)for(unsigned topic=0;topic<holly_lore_count();topic++)for(unsigned page=0;;page++){
   const char *t,*a,*x,*u;if(!holly_lore_passage(topic,page,&t,&a,&x,&u))break;offer(x,length(x),t,u,1+topic*4+page,&r);
  }
  /* Scores depend only on matching all query terms and this fixed anchor.
   * A curated match already has the maximum possible score; document ties
   * cannot replace it. Avoid scanning megabytes when the answer is known. */
  if(documents&&!r.score){
   char filter[320]="";
   if(r.query.n){
    copy(filter,r.query.token[0],sizeof filter);
    for(unsigned group=0;group<sizeof groups/sizeof groups[0];group++)if(eq(groups[group][0],r.query.token[0]))
     for(unsigned j=1;groups[group][j];j++){add(filter," ",sizeof filter);add(filter,groups[group][j],sizeof filter);}
   }
   documents(offer,&r,storage,filter);
  }
  if(!r.score){if(count==2)add(out,"For ",cap);else add(out,"I don't have a passage that answers that specific question about ",cap);add(out,title,cap);add(out,". ",cap);continue;}
  if(success||choice)add(out,"\n\n",cap);
  if(count==2){add(out,r.title,cap);add(out,": ",cap);}
  spoken_passage(out,r.text,cap);
  if(success)add(source_list,"\n",sizeof source_list);
  add(source_list,r.title,sizeof source_list);add(source_list,": ",sizeof source_list);add(source_list,r.source,sizeof source_list);
  success++;if(choice==0){newkey=r.key;copy(newsubject,title,sizeof newsubject);}
 }
 if(!success){
  if(newtopic!=s->topic)s->seen_count=0;
  s->topic=newtopic;
  const char *t,*a,*x,*u;if(newtopic>=0&&holly_lore_passage((unsigned)newtopic,0,&t,&a,&x,&u))copy(s->subject,t,sizeof s->subject);
  s->sources[0]=s->answer[0]=0;add(out,s->dry_voice?"I could guess. Let's not make the records worse. Try rephrasing, or add a reference through SSH.":"Try rephrasing, or upload a reference through SSH. I won't fill the gap with invented canon.",cap);return 2;}
 if(newtopic!=s->topic)s->seen_count=0;
 if(s->seen_count<16)s->seen[s->seen_count++]=newkey;
 occasional_banter(s,q,out,cap);
 s->topic=newtopic;s->last_key=newkey;copy(s->subject,newsubject,sizeof s->subject);copy(s->sources,source_list,sizeof s->sources);copy(s->answer,out,sizeof s->answer);if(count==2){s->topic=-1;s->subject[0]=0;}return 1;
}
