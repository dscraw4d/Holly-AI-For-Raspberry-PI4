#include "news.h"
#include "search.h"
#include "http_server.h"
#include "http_page.h"
#include "http_art.h"
static unsigned public_lan,public_vault;
static holly_web_speech_fn speech_observer;static void *speech_context;
void holly_web_set_speech_observer(holly_web_speech_fn fn,void *ctx){speech_observer=fn;speech_context=ctx;}
void holly_web_status(unsigned lan,unsigned vault){public_lan=lan;public_vault=vault;}
static unsigned len(const char *p){unsigned n=0;while(p[n])n++;return n;}
static int eq(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static char lower(char c){return c>='A'&&c<='Z'?(char)(c+32):c;}
static int same_header(const char *a,const char *b){while(*a&&*b){if(lower(*a++)!=lower(*b++))return 0;}return !*a&&!*b;}
static void copy(char *d,const char *s,unsigned cap){unsigned i=0;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static void append(char *d,const char *s,unsigned cap){unsigned n=len(d);if(n<cap)copy(d+n,s,cap-n);}
static void number(char *d,unsigned n,unsigned cap){char b[12];unsigned i=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i){char t[2]={b[--i],0};append(d,t,cap);}}
static int response_bytes(struct holly_web_server *h,const char *status,const char *type,const void *body,unsigned size){
 char header[768]="HTTP/1.1 ";append(header,status,sizeof header);append(header,"\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'\r\nContent-Type: ",sizeof header);
 append(header,type,sizeof header);append(header,h->avatar==2?"\r\nX-Holly-Avatar: queeg":h->avatar==1?"\r\nX-Holly-Avatar: hilly":"\r\nX-Holly-Avatar: holly",sizeof header);append(header,"\r\nContent-Length: ",sizeof header);number(header,size,sizeof header);append(header,"\r\n\r\n",sizeof header);
 h->done=1;if(h->write((const uint8_t *)header,len(header),h->transport)||h->write((const uint8_t *)body,size,h->transport))return -1;
 return 1; /* Gracefully drain TCP queue then close. */
}
static int response(struct holly_web_server *h,const char *status,const char *type,const char *body){return response_bytes(h,status,type,body,len(body));}
struct capture {char text[4096];unsigned used;};
static void emit(const char *s,void *ctx){struct capture *c=ctx;while(*s&&c->used+1<sizeof c->text)c->text[c->used++]=*s++;c->text[c->used]=0;}
/* Queeg's authored manner is separate from reference text and saved knowledge. */
static const char *queeg_social(const char *msg){
 char q[320];unsigned n=0;while(msg[n]&&n+1<sizeof q){q[n]=lower(msg[n]);n++;}q[n]=0;
 while(n&&(q[n-1]==' '||q[n-1]=='!'||q[n-1]=='?'||q[n-1]=='.'))q[--n]=0;
 if(eq(q,"queeg")||eq(q,"hello")||eq(q,"hi")||eq(q,"hey")||eq(q,"hello queeg")||eq(q,"hi queeg"))return "Queeg is in command. State your question clearly. No idle chatter.";
 if(eq(q,"how are you")||eq(q,"who are you")||eq(q,"what are you"))return "I am Queeg, this computer's command persona. Systems operational. Proceed with your question.";
 if(eq(q,"tell me a joke")||eq(q,"another joke"))return "No jokes. Concentrate on the task and ask a useful question.";
 if(eq(q,"thanks")||eq(q,"thank you")||eq(q,"cheers"))return "Acknowledged. Carry on.";
 if(eq(q,"are you alive")||eq(q,"are you conscious"))return "I am software running on your Pi. I am not conscious. Now attend to the task.";
 return 0;
}
static int queeg_response(struct holly_web_server *h,const char *text,unsigned turns,const char *protocol){
 char reply[4096]="";if(protocol)append(reply,protocol,sizeof reply);
 if(text[0]=='H'&&text[1]=='o'&&text[2]=='l'&&text[3]=='l'&&text[4]=='y'){
  const char *end=text;while(*end&&*end!=':'&&*end!='\n')end++;if(*end==':'){text=end+1;while(*text==' ')text++;}
 }else if(text[0]=='Q'&&text[1]=='u'&&text[2]=='e'&&text[3]=='e'&&text[4]=='g'&&text[5]==':'){text+=6;while(*text==' ')text++;}
 append(reply,"Queeg: ",sizeof reply);append(reply,text,sizeof reply);
 unsigned n=len(reply);while(n&&(reply[n-1]=='\n'||reply[n-1]==' '))reply[--n]=0;
 const char *orders[]={" Attend to the details."," Keep your questions precise."," Read that carefully. Then proceed."," Stay focused."};
 append(reply,orders[turns%4],sizeof reply);
 if(turns&&turns%7==0)append(reply," Holly's slack standards will not apply while I am in command.",sizeof reply);
 append(reply,"\n",sizeof reply);return response(h,"200 OK","text/plain; charset=utf-8",reply);
}
void holly_web_close(void *app){struct holly_web_server *h=app;if(h){holly_secret_wipe(h->request,sizeof h->request);h->used=h->done=0;h->write=0;h->transport=0;}}
void holly_web_clear(struct holly_web_server *h){if(h){holly_web_close(h);holly_secret_wipe(h->sessions,sizeof h->sessions);h->next=0;h->speech_owner[0]=0;if(speech_observer)speech_observer(2,0,speech_context);}}
int holly_web_start(holly_ssh_write_fn write,void *transport,void *app){struct holly_web_server *h=app;if(!h||!write||!h->random)return -1;holly_web_close(h);h->write=write;h->transport=transport;return 0;}
int holly_web_feed(const uint8_t *p,size_t n,void *app){
 struct holly_web_server *h=app;if(!h||!h->write||h->done||(!p&&n))return -1;
 if(n>sizeof h->request-1-h->used)return response(h,"413 Content Too Large","text/plain","Request too large.\n");
 for(size_t i=0;i<n;i++){if(!p[i]||p[i]>126||(p[i]<32&&p[i]!='\r'&&p[i]!='\n'&&p[i]!='\t'))return response(h,"400 Bad Request","text/plain","ASCII requests only.\n");h->request[h->used++]=(char)p[i];}h->request[h->used]=0;
 unsigned end=0;for(unsigned i=3;i<h->used;i++)if(h->request[i-3]=='\r'&&h->request[i-2]=='\n'&&h->request[i-1]=='\r'&&h->request[i]=='\n'){end=i+1;break;}
 if(!end){if(h->used>2048)return response(h,"431 Request Header Fields Too Large","text/plain","Headers too large.\n");return 0;}
 if(end>2048)return response(h,"431 Request Header Fields Too Large","text/plain","Headers too large.\n");
 char headers[2049];for(unsigned i=0;i<end;i++)headers[i]=h->request[i];headers[end]=0;
 char *line=headers,*at=headers;while(*at&&*at!='\r')at++;if(!*at)return -1;*at=0;at+=2;
 int art=eq(line,"GET /holly-face.jpg HTTP/1.1"),public_status=eq(line,"GET /status HTTP/1.1");
 int root=eq(line,"GET / HTTP/1.1"),session=eq(line,"GET /session HTTP/1.1"),chat=eq(line,"POST /chat HTTP/1.1"),speech=eq(line,"POST /speech HTTP/1.1"),clock_request=eq(line,"POST /clock HTTP/1.1"),alerts=eq(line,"POST /alerts HTTP/1.1"),news=eq(line,"POST /news HTTP/1.1"),search=eq(line,"POST /search HTTP/1.1");
 unsigned length=0,have_length=0;char token[33]="",host[80]="",origin[100]="";unsigned tokens=0,hosts=0,origins=0,types=0;int bad=0;
 while(*at&&*at!='\r'){
  char *name=at;while(*at&&*at!='\r')at++;if(!*at){bad=1;break;}*at=0;at+=2;
  char *value=name;while(*value&&*value!=':')value++;if(!*value){bad=1;break;}*value++=0;while(*value==' '||*value=='\t')value++;
  if(same_header(name,"Content-Length")){if(have_length++){bad=1;break;}if(!*value)bad=1;for(;*value;value++){if(*value<'0'||*value>'9'||length>319){bad=1;break;}length=length*10+(unsigned)(*value-'0');}}
  else if(same_header(name,"Transfer-Encoding")||same_header(name,"Expect"))bad=1;
  else if(same_header(name,"X-Holly-Session")){if(tokens++||len(value)!=32)bad=1;else copy(token,value,sizeof token);}
  else if(same_header(name,"Host")){if(hosts++||len(value)>=sizeof host||!*value)bad=1;else copy(host,value,sizeof host);}
  else if(same_header(name,"Origin")){if(origins++||len(value)>=sizeof origin)bad=1;else copy(origin,value,sizeof origin);}
  else if(same_header(name,"Content-Type")){if(types++||!eq(value,"text/plain"))bad=1;}
 }
 if(!hosts)bad=1;
 for(unsigned i=0;host[i];i++)if(!((host[i]>='0'&&host[i]<='9')||host[i]=='.'||host[i]==':'))bad=1;
 if(origins){char expected[100]="http://";append(expected,host,sizeof expected);if(!eq(expected,origin))bad=1;}
 if(bad||length>319)return response(h,"400 Bad Request","text/plain","Invalid request framing or origin.\n");
 if(h->used<end+length)return 0;
 if(h->used!=end+length)return response(h,"400 Bad Request","text/plain","One request per connection.\n");
 if(art&&!length)return response_bytes(h,"200 OK","image/jpeg",http_art,sizeof http_art);
 if(public_status&&!length){char body[80]="{\"lan\":";append(body,public_lan?"true":"false",sizeof body);append(body,",\"vault\":",sizeof body);append(body,public_vault?"true}":"false}",sizeof body);return response(h,"200 OK","application/json",body);}
 if(root&&!length)return response(h,"200 OK","text/html; charset=utf-8",http_page);
 if(session&&!length){uint8_t rnd[16];if(h->random(rnd,sizeof rnd,h->random_context))return response(h,"503 Service Unavailable","text/plain","Random generator unavailable.\n");
  struct holly_http_session *s=&h->sessions[h->next++%4];holly_session_init(&s->chat);s->chat.learning_enabled=0;s->avatar=h->avatar;s->chat.persona=s->avatar;s->speech_generation=s->speech_closed=0;
  const char *hex="0123456789abcdef";for(unsigned i=0;i<16;i++){s->token[i*2]=hex[rnd[i]>>4];s->token[i*2+1]=hex[rnd[i]&15];}s->token[32]=0;
  return response(h,"200 OK","text/plain",s->token);
 }
 if((chat||speech||clock_request||alerts||news||search)&&have_length&&length&&tokens&&types){
  struct holly_http_session *s=0;for(unsigned i=0;i<4;i++)if(eq(h->sessions[i].token,token)){s=&h->sessions[i];break;}
  if(!s)return response(h,"410 Gone","text/plain","Session expired. Start a new conversation.\n");
  h->avatar=s->avatar;
  char msg[320];for(unsigned i=0;i<length;i++){char c=h->request[end+i];if(c<32||c>126)return response(h,"400 Bad Request","text/plain","Use one ASCII line.\n");msg[i]=c;}msg[length]=0;
  if(clock_request){
   if(holly_browser_clock(msg))return response(h,"400 Bad Request","text/plain","Invalid epoch or local UTC offset.\n");
   return response(h,"200 OK","text/plain","Clock synchronised from browser.\n");
  }
  if(search){if(!eq(msg,"poll"))return response(h,"400 Bad Request","text/plain","Use poll.\n");char result[1450];holly_search_poll(&s->chat,result,sizeof result);if(s->avatar==2&&(result[0]=='r'||result[0]=='e'))return queeg_response(h,result+6,s->chat.turns,result[0]=='r'?"ready|":"error|");return response(h,"200 OK","text/plain",result);}
  if(news){if(!eq(msg,"poll"))return response(h,"400 Bad Request","text/plain","Use poll.\n");char result[1450];holly_news_poll(result,sizeof result);if(s->avatar==2&&(result[0]=='r'||result[0]=='e'))return queeg_response(h,result+6,s->chat.turns,result[0]=='r'?"ready|":"error|");return response(h,"200 OK","text/plain",result);}
  if(alerts){
   if(eq(msg,"poll")){char alert[400];(void)holly_alarm_poll(alert,sizeof alert);return response(h,"200 OK","text/plain",alert);}
   if(len(msg)>4&&msg[0]=='a'&&msg[1]=='c'&&msg[2]=='k'&&msg[3]==' '&&!holly_alarm_ack_text(msg+4))return response(h,"200 OK","text/plain","Acknowledged.\n");
   return response(h,"400 Bad Request","text/plain","Invalid alarm poll or acknowledgement.\n");
  }
  if(speech){
   /* Numbered, retryable controls isolate each reply from delayed ends/frames.
    * Closed generations cannot be resurrected by a delayed begin retry. */
   unsigned command=0,at=0,id=0,frame=0;
   if(msg[0]=='b'&&msg[1]=='e'&&msg[2]=='g'&&msg[3]=='i'&&msg[4]=='n'&&msg[5]==' '){command=1;at=6;}
   else if(msg[0]=='f'&&msg[1]=='r'&&msg[2]=='a'&&msg[3]=='m'&&msg[4]=='e'&&msg[5]==' '){command=2;at=6;}
   else if(msg[0]=='e'&&msg[1]=='n'&&msg[2]=='d'&&msg[3]==' '){command=3;at=4;}
   if(command){
    unsigned digits=0;while(msg[at]>='0'&&msg[at]<='9'){if(id>429496729u||(id==429496729u&&msg[at]>'5'))return response(h,"400 Bad Request","text/plain","Invalid speech generation.\n");id=id*10+(unsigned)(msg[at++]-'0');digits++;}
    if(!digits||!id)return response(h,"400 Bad Request","text/plain","Invalid speech generation.\n");
    if(command!=3){if(msg[at++]!=' '||msg[at]<'0'||msg[at]>'6'||msg[at+1])return response(h,"400 Bad Request","text/plain","Invalid mouth frame.\n");frame=(unsigned)(msg[at++]-'0');}
    if(msg[at])return response(h,"400 Bad Request","text/plain","Invalid mouth control.\n");
    if(id<s->speech_generation||(id==s->speech_generation&&s->speech_closed))return response(h,"200 OK","text/plain","Stale control ignored.\n");
    if(command==1){s->speech_generation=id;s->speech_closed=0;copy(h->speech_owner,token,sizeof h->speech_owner);if(speech_observer)speech_observer(5,s->avatar,speech_context);}
    else if(command==3&&id>s->speech_generation){s->speech_generation=id;s->speech_closed=1;if(eq(h->speech_owner,token)){if(speech_observer)speech_observer(2,0,speech_context);h->speech_owner[0]=0;}return response(h,"200 OK","text/plain","Stopped.\n");}
    else if(id!=s->speech_generation||!eq(h->speech_owner,token))return response(h,command==3?"200 OK":"409 Conflict","text/plain","Speech owner changed.\n");
    if(speech_observer)speech_observer(command==3?2:1,frame,speech_context);
    if(command==3){s->speech_closed=1;h->speech_owner[0]=0;}
    return response(h,"200 OK","text/plain","OK\n");
   }
   unsigned action=0;frame=0;
   if(eq(msg,"begin")){action=0;if(speech_observer)speech_observer(5,s->avatar,speech_context);copy(h->speech_owner,token,sizeof h->speech_owner);}
   else if(eq(msg,"end"))action=2;
   else if(len(msg)==6&&msg[0]=='f'&&msg[1]=='r'&&msg[2]=='a'&&msg[3]=='m'&&msg[4]=='e'&&msg[5]>='0'&&msg[5]<='6'){action=1;frame=(unsigned)(msg[5]-'0');}
   else return response(h,"400 Bad Request","text/plain","Invalid mouth control.\n");
   /* Stop is idempotent, and a stale browser must not stop another owner. */
   if(action==2&&!eq(h->speech_owner,token))return response(h,"200 OK","text/plain","OK\n");
   if(!eq(h->speech_owner,token))return response(h,"409 Conflict","text/plain","Another browser controls the mouth.\n");
   if(speech_observer)speech_observer(action,frame,speech_context);
   if(action==2)h->speech_owner[0]=0;
   return response(h,"200 OK","text/plain","OK\n");
  }
  unsigned avatar=s->avatar;
  for(unsigned i=0;msg[i];){
   unsigned at=i;while(msg[i]&&((msg[i]>='a'&&msg[i]<='z')||(msg[i]>='A'&&msg[i]<='Z')||(msg[i]>='0'&&msg[i]<='9')||msg[i]=='_'))i++;
   if(i-at>=5&&i-at<=7){char word[8];unsigned size=i-at;for(unsigned j=0;j<size;j++)word[j]=lower(msg[at+j]);word[size]=0;if(eq(word,"hilly")||eq(word,"hillary")||eq(word,"hilary"))avatar=1;else if(eq(word,"holly"))avatar=0;else if(eq(word,"queeg"))avatar=2;}
   if(i==at)i++;
  }
  s->avatar=h->avatar=avatar;s->chat.persona=avatar;if(speech_observer)speech_observer(5,avatar,speech_context);
  struct capture c={{0},0};if(speech_observer)speech_observer(3,0,speech_context);
  const char *query=msg;
  if(avatar==2){unsigned k=0;while(query[k]&&lower(query[k])=="queeg"[k]&&k<5)k++;if(k==5&&(query[k]==' '||query[k]==',')){query+=6;while(*query==' ')query++;}}
  const char *social=avatar==2?queeg_social(query):0;
  if(social){s->chat.turns++;emit(social,&c);}
  else {unsigned saved=s->chat.personality_enabled;if(avatar==2)s->chat.personality_enabled=0;(void)holly_conversation_only(&s->chat,query,emit,&c);s->chat.personality_enabled=saved;}
  if(speech_observer){speech_observer(4,0,speech_context);}
  if(avatar==2)return queeg_response(h,c.text,s->chat.turns,0);
  return response(h,"200 OK","text/plain; charset=utf-8",c.text);
 }
 return response(h,"404 Not Found","text/plain","Not found.\n");
}
