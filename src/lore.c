#include "lore.h"
struct lore_entry {
 const char *title,*aliases;unsigned guard;int cast;unsigned pages;
 const char *text[3],*source[3];
};
#include "lore_data.h"
static unsigned length(const char *s){unsigned n=0;while(s[n])n++;return n;}
static int same(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int starts(const char *a,const char *b){while(*b)if(*a++!=*b++)return 0;return 1;}
static void copy(char *d,const char *s,size_t cap){size_t i=0;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static void append(char *d,const char *s,size_t cap){unsigned n=length(d);if(n<cap)copy(d+n,s,cap-n);}
static void number(char *d,unsigned n,size_t cap){char b[12];unsigned i=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i){char t[2]={b[--i],0};append(d,t,cap);}}
static int normalize(const char *in,char *out,unsigned cap){
 unsigned i=0,n=0;int space=0;
 for(;in[i];i++){
  if(i>=319)return -1;
  unsigned char c=(unsigned char)in[i];
  if(c>127)return -1;
  if(c>='A'&&c<='Z')c+=32;
  if((c>='a'&&c<='z')||(c>='0'&&c<='9')){
   if(space&&n){if(n+1>=cap)return -1;out[n++]=' ';}space=0;
   if(n+1>=cap)return -1;
   out[n++]=(char)c;
  }else space=1;
 }
 out[n]=0;return 0;
}
static int phrase(const char *q,const char *key,unsigned *pos){
 unsigned n=length(key);
 for(unsigned i=0;q[i];i++)if(!i||q[i-1]==' '){
  unsigned j=0;while(j<n&&q[i+j]&&q[i+j]==key[j])j++;
  if(j==n&&(!q[i+j]||q[i+j]==' ')){if(pos)*pos=i;return 1;}
 }
 return 0;
}
unsigned holly_lore_count(void){return sizeof(entries)/sizeof(entries[0]);}
void holly_lore_clear(struct holly_lore *s){if(s){s->topic=-1;s->page=0;}}
void holly_lore_init(struct holly_lore *s){if(s){s->enabled=1;holly_lore_clear(s);}}
const char *holly_lore_topic(const struct holly_lore *s){return s&&s->topic>=0&&(unsigned)s->topic<holly_lore_count()?entries[s->topic].title:"";}
static unsigned match(const char *q,const char *list,unsigned *pos){
 unsigned best=0;
 while(*list){char a[64];unsigned n=0;while(*list&&*list!='|'){if(n+1<sizeof(a))a[n++]=*list;list++;}a[n]=0;if(*list)list++;
  unsigned at=0;if(phrase(q,a,&at)&&n>best){best=n;*pos=at;}
 }
 return best;
}
static int question(const char *q){
 static const char *p[]={"who ","what ","where ","when ","why ","how ","tell me ","explain ","describe ","do you know ","talk about "};
 for(unsigned i=0;i<sizeof(p)/sizeof(p[0]);i++)if(starts(q,p[i]))return 1;
 return 0;
}
static int follow(const char *q){return same(q,"more")||same(q,"tell me more")||same(q,"go on")||same(q,"what else")||same(q,"and then");}
static int actor(const char *q){return phrase(q,"plays",0)||phrase(q,"played",0)||phrase(q,"actor",0)||phrase(q,"actress",0)||phrase(q,"cast",0)||phrase(q,"voices",0);}
static int actor_follow(const char *q){return same(q,"who plays him")||same(q,"who plays her")||same(q,"who plays them")||same(q,"who played him")||same(q,"who played her")||same(q,"who is the actor");}
int holly_lore_reply(struct holly_lore *s,const char *input,char *out,size_t cap){
 if(!s||!input||!out||cap<257)return -1;
 out[0]=0;char normalized[320];if(normalize(input,normalized,sizeof(normalized)))return -1;
 const char *q=normalized;int explicit=same(q,"dwarf")||starts(q,"dwarf ");
 if(explicit){q+=5;while(*q==' ')q++;}
 if(explicit&&(same(q,"on")||same(q,"off")||same(q,"status"))){
  if(same(q,"on"))s->enabled=1;
  if(same(q,"off")){s->enabled=0;holly_lore_clear(s);}
  copy(out,"Red Dwarf reference is ",cap);append(out,s->enabled?"on. ":"off. ",cap);number(out,holly_lore_count(),cap);
  append(out," curated topics; offline retrieval, not neural training. TV spoilers included. Use dwarf topics, dwarf <topic>, more, dwarf source. Session setting.",cap);return 1;
 }
 if(explicit&&(!*q||same(q,"help"))){copy(out,"Red Dwarf: dwarf on/off/status | dwarf topics [1-6] | dwarf <question> | dwarf more | dwarf source | dwarf reset. Also ask a named topic naturally. Curated TV reference with spoilers; not exhaustive.",cap);return 1;}
 if(explicit&&same(q,"reset")){holly_lore_clear(s);copy(out,"Red Dwarf conversation context cleared. Saved memories are unchanged.",cap);return 1;}
 if(explicit&&(same(q,"topics")||starts(q,"topics "))){
  unsigned page=1;const char *p=q+6;while(*p==' ')p++;
  if(*p){page=0;while(*p>='0'&&*p<='9'){if(page>100)return copy(out,"Use dwarf topics 1-6.",cap),1;page=page*10+(unsigned)(*p++-'0');}
   if(*p||!page)return copy(out,"Use dwarf topics 1-6.",cap),1;
  }
  unsigned pages=(holly_lore_count()+7)/8;if(page>pages)return copy(out,"That topic page does not exist. Use dwarf topics 1-6.",cap),1;
  copy(out,"Red Dwarf topics ",cap);number(out,page,cap);append(out,"/",cap);number(out,pages,cap);append(out,": ",cap);
  for(unsigned i=(page-1)*8;i<page*8&&i<holly_lore_count();i++){if(i>(page-1)*8)append(out,"; ",cap);append(out,entries[i].title,cap);}return 1;
 }
 if(explicit&&(same(q,"source")||same(q,"sources"))){
  if(!*holly_lore_topic(s))copy(out,"Ask a Red Dwarf topic first; I have no current reference source.",cap);
  else{copy(out,"Source for the last lore reply: ",cap);append(out,entries[s->topic].source[s->page],cap);}
  return 1;
 }
 if(!s->enabled&&!explicit)return 0;
 if(phrase(q,"red dwarf star",0)||phrase(q,"red dwarf stars",0)){
  if(!explicit)return 0;
  copy(out,"This reference covers the Red Dwarf television fiction, not stellar astronomy.",cap);return 1;
 }
 if(follow(q)){
  if(!*holly_lore_topic(s)){if(!explicit)return 0;copy(out,"Choose a topic first with dwarf <topic>.",cap);return 1;}
  const struct lore_entry *e=&entries[s->topic];
  if(s->page+1>=e->pages){copy(out,"That is all I have verified on this topic. Try dwarf source, or choose another topic with dwarf topics.",cap);return 1;}
  s->page++;copy(out,e->text[s->page],cap);return 1;
 }
 unsigned sizes[sizeof(entries)/sizeof(entries[0])],positions[sizeof(entries)/sizeof(entries[0])];
 int best=-1;unsigned high=0;
 for(unsigned i=0;i<holly_lore_count();i++){
  positions[i]=0;sizes[i]=match(q,entries[i].aliases,&positions[i]);
  if(!explicit&&sizes[i]&&entries[i].guard&&!phrase(q,"red dwarf",0)){
   /* Common words need an exact topic request, not incidental conversation. */
   const char *core=q;const char *prefix[]={"who is ","what is ","tell me about ","describe "};
   for(unsigned j=0;j<4;j++)if(starts(core,prefix[j])){core+=length(prefix[j]);break;}
   unsigned at=0;if(match(core,entries[i].aliases,&at)!=length(core)||at)sizes[i]=0;
  }
  if(sizes[i]>high){high=sizes[i];best=(int)i;}
 }
 /* Red Dwarf itself is a domain marker when a specific topic is present. */
 if(best==0){high=0;best=-1;for(unsigned i=1;i<holly_lore_count();i++)if(sizes[i]>high){high=sizes[i];best=(int)i;}if(best<0){best=0;high=sizes[0];}}
 if(best>=0&&!explicit&&!question(q)&&!(positions[best]==0&&high==length(q)))return 0;
 if(best>=0){
  for(unsigned i=1;i<holly_lore_count();i++)if((int)i!=best&&sizes[i]){
   unsigned a=positions[i],b=positions[best];
   if(a+sizes[i]<=b||b+high<=a){copy(out,"I found more than one Red Dwarf topic. Ask about one at a time; try dwarf topics.",cap);holly_lore_clear(s);return 1;}
  }
 }else if(actor_follow(q)&&*holly_lore_topic(s))best=s->topic;
 if(best<0){
  if(!explicit)return 0;
  holly_lore_clear(s);copy(out,"I don't have a verified entry for that. Try dwarf topics, or teach a specific question => answer. I won't invent Red Dwarf canon.",cap);return 1;
 }
 const struct lore_entry *e=&entries[best];s->topic=best;s->page=0;
 if(actor(q)){
  if(e->cast<0){copy(out,"I have no separate cast answer for that topic. Ask dwarf more for its notes or dwarf source for the reference.",cap);return 1;}
  s->page=(unsigned)e->cast;
 }
 copy(out,e->text[s->page],cap);return 1;
}
int holly_lore_passage(unsigned topic,unsigned page,const char **title,const char **aliases,const char **text,const char **source){
 if(topic>=holly_lore_count()||page>=entries[topic].pages)return 0;
 *title=entries[topic].title;*aliases=entries[topic].aliases;*text=entries[topic].text[page];*source=entries[topic].source[page];return 1;
}
