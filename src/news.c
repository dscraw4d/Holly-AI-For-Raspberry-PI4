#include "news.h"
#include <string.h>
static void assign(char*out,const char*in){memcpy(out,in,strlen(in)+1);}
static holly_news_fetch_fn fetch_news;
static unsigned state;static uint32_t requested_at;static uint64_t ticks,start_ticks;
static char bulletin[1400];
static void add(char*out,const char*s,unsigned cap){unsigned n=(unsigned)strlen(out);while(*s&&n+1<cap)out[n++]=*s++;out[n]=0;}
void holly_news_bind(holly_news_fetch_fn fn){fetch_news=fn;state=0;bulletin[0]=0;}
int holly_news_pending(void){return state==1;}
void holly_news_fail(void){state=3;assign(bulletin,"I couldn't fetch a fresh, verified world-news bulletin. Please check the clock and internet connection, then ask again.");}
void holly_news_tick(uint64_t us){ticks=us;if(state==1&&us-start_ticks>=60000000ull)holly_news_fail();}
static const char*find(const char*p,const char*end,const char*s){unsigned n=(unsigned)strlen(s);for(;p<=end&&(unsigned)(end-p)>=n;p++)if(!memcmp(p,s,n))return p;return 0;}
static int number(const char*p,unsigned n){int v=0;for(unsigned i=0;i<n;i++){if(p[i]<'0'||p[i]>'9')return -1;v=v*10+p[i]-'0';}return v;}
static int stamp(const char*p,const char*end,uint32_t*out){
 char token[6][16];unsigned n=0;
 while(p<end){while(p<end&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))p++;if(p==end)break;if(n==6)return -1;unsigned k=0;while(p<end&&*p!=' '&&*p!='\t'&&*p!='\r'&&*p!='\n'){if(k==15)return -1;token[n][k++]=*p++;}token[n++][k]=0;}
 if(n!=6||strlen(token[1])<1||strlen(token[1])>2||strlen(token[0])!=4||token[0][3]!=','||strlen(token[3])!=4||strlen(token[4])!=8||(strcmp(token[5],"GMT")&&strcmp(token[5],"UTC")&&strcmp(token[5],"+0000")))return -1;
 int d=number(token[1],(unsigned)strlen(token[1])),y=number(token[3],4),h=number(token[4],2),m=number(token[4]+3,2),s=number(token[4]+6,2);
 if(y<2020||y>2100||h<0||h>23||m<0||m>59||s<0||s>59||token[4][2]!=':'||token[4][5]!=':')return -1;
 const char*months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};unsigned mo=0;while(mo<12&&strcmp(months[mo],token[2]))mo++;if(mo==12)return -1;
 const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};unsigned leap=y%4==0&&(y%100!=0||y%400==0);
 if(d<1||(unsigned)d>lengths[mo]+(mo==1&&leap))return -1;
 uint64_t days=0;for(int year=1970;year<y;year++)days+=365+(year%4==0&&(year%100!=0||year%400==0));for(unsigned i=0;i<mo;i++)days+=lengths[i]+(i==1&&leap);
 *out=(uint32_t)((days+d-1)*86400+h*3600+m*60+s);return 0;
}
static int title(const char*p,const char*end,char*out,unsigned cap){
 if(end-p>=12&&!memcmp(p,"<![CDATA[",9)){p+=9;if(memcmp(end-3,"]]>",3))return -1;end-=3;}
 unsigned n=0;int space=0;
 while(p<end){unsigned char c=(unsigned char)*p++;
  if(c=='&'){const char*semi=find(p,end,";");if(!semi||semi-p>12)return -1;unsigned k=(unsigned)(semi-p);
   if(k==3&&!memcmp(p,"amp",3))c='&';else if(k==4&&!memcmp(p,"quot",4))c='"';else if(k==4&&!memcmp(p,"apos",4))c='\'';else if(k==2&&!memcmp(p,"lt",2))c='<';else if(k==2&&!memcmp(p,"gt",2))c='>';
   else if(k>=2&&p[0]=='#'){unsigned v=0,base=10,at=1;if(p[1]=='x'||p[1]=='X'){base=16;at=2;}if(at==k)return -1;for(;at<k;at++){unsigned digit=p[at]>='0'&&p[at]<='9'?(unsigned)(p[at]-'0'):p[at]>='a'&&p[at]<='f'?(unsigned)(p[at]-'a'+10):p[at]>='A'&&p[at]<='F'?(unsigned)(p[at]-'A'+10):99u;if(digit>=base||v>0x10ffff/base)return -1;v=v*base+digit;}c=v>=32&&v<127?(unsigned char)v:v==0x2018||v==0x2019?'\'':v==0x201c||v==0x201d?'"':v==0x2013||v==0x2014?'-':v==160?' ':'?';}
   else return -1;
   p=semi+1;
  }else if(c=='<')return -1;
  else if(c>=128){if(c==0xe2&&end-p>=2&&(unsigned char)p[0]==0x80){unsigned char v=(unsigned char)p[1];c=v==0x98||v==0x99?'\'':v==0x9c||v==0x9d?'"':v==0x93||v==0x94?'-':'?';p+=2;}else {while(p<end&&((unsigned char)*p&0xc0)==0x80)p++;c='?';}}
  if(c==' '||c=='\t'||c=='\r'||c=='\n'){space=n!=0;continue;}if(c<32)return -1;
  if(n+5>=cap){memcpy(out+n,"...",4);return 0;}if(space){out[n++]=' ';space=0;}out[n++]=(char)c;
 }out[n]=0;return n?0:-1;
}
int holly_news_parse(const char*xml,unsigned size,uint32_t now,char*out,unsigned cap){
 if(!xml||!out||cap<1400||size>32768||!now)return -1;
 const char*end=xml+size;out[0]=0;
 if(!find(xml,end,"<rss")||!find(xml,end,"</rss>")||find(xml,end,"<!DOCTYPE")||find(xml,end,"<!ENTITY"))return -1;
 const char*channel=find(xml,end,"<channel>");const char*channel_end=channel?find(channel,end,"</channel>"):0;if(!channel_end)return -1;
 add(out,"Latest world headlines from BBC News. These are recent headlines, not necessarily all published today. ",cap);
 unsigned count=0;const char*p=channel;char seen[5][201];
 while(count<5&&(p=find(p,channel_end,"<item>"))){const char*finish=find(p,channel_end,"</item>");if(!finish)return -1;
  const char*t=find(p,finish,"<title>"),*d=find(p,finish,"<pubDate>");const char*te=t?find(t+7,finish,"</title>"):0,*de=d?find(d+9,finish,"</pubDate>"):0;uint32_t published;char headline[201];
  if(t&&te&&d&&de&&!stamp(d+9,de,&published)&&published<=now+600u&&now<=published+172800u&&!title(t+7,te,headline,sizeof headline)){
   unsigned duplicate=0;for(unsigned i=0;i<count;i++)if(!strcmp(seen[i],headline))duplicate=1;
   if(!duplicate){assign(seen[count],headline);char label[8]={(char)('1'+count),'.',' ',0};add(out,label,cap);add(out,headline,cap);add(out,". ",cap);count++;}
  }p=finish+7;
 }
 if(!count){out[0]=0;return -1;}return (int)count;
}
int holly_news_finish(const char*body,unsigned n){if(state!=1)return -1;int r=holly_news_parse(body,n,requested_at,bulletin,sizeof bulletin);if(r<0){holly_news_fail();return -1;}state=2;return 0;}
static void freshness(void){uint32_t now=holly_clock_utc();if(state==2&&(!now||now<requested_at||now-requested_at>900u)){state=3;assign(bulletin,"That fetched bulletin is now out of date. Ask read me todays world news for a fresh request.");}}
void holly_news_poll(char*out,unsigned cap){freshness();out[0]=0;add(out,state==1?"pending|":state==2?"ready|":state==3?"error|":"idle|",cap);if(state==2||state==3)add(out,bulletin,cap);}
int holly_news_command(const char*input,holly_emit_fn emit,void*ctx){
 char q[320]={0};unsigned i=0;while(input[i]&&i+1<sizeof q){q[i]=input[i]>='A'&&input[i]<='Z'?(char)(input[i]+32):input[i];i++;}q[i]=0;while(i&&(q[i-1]=='?'||q[i-1]=='!'||q[i-1]=='.'||q[i-1]==' '))q[--i]=0;
 const char*p=q;const char*prefixes[]={"holly ","hilly ","holly, ","hilly, ","hillary ","hilary ","hillary, ","hilary, "};for(unsigned n=0;n<sizeof prefixes/sizeof prefixes[0];n++){unsigned k=(unsigned)strlen(prefixes[n]);if(!memcmp(p,prefixes[n],k)){p+=k;break;}}
 if(!strcmp(p,"news source")){emit("Holly: BBC News World RSS: https://feeds.bbci.co.uk/news/world/rss.xml\n",ctx);return 1;}
 if(!strcmp(p,"news status")){freshness();emit("Holly: ",ctx);emit(state==1?"Fetching current world headlines. Please wait.":state==2||state==3?bulletin:"No world-news request this boot. Ask read me todays world news.",ctx);emit("\n",ctx);return 1;}
 const char*phrases[]={"news","world news","today's world news","todays world news","read me todays world news","read me today's world news","read me the world news","read me today's news","read me todays news","read me the news","tell me today's world news","tell me todays world news","what is happening in the world","what's happening in the world"};
 unsigned matched=0;for(unsigned n=0;n<sizeof phrases/sizeof phrases[0];n++)if(!strcmp(p,phrases[n]))matched=1;if(!matched)return 0;
 if(state==1){emit("Holly: Fetching current world headlines. Please wait.\n",ctx);return 1;}
 uint32_t now=holly_clock_utc();if(!now){emit("Holly: Synchronise my clock first: open my web page or use clock set over SSH. I need the correct time for HTTPS and news freshness.\n",ctx);return 1;}
 if(!fetch_news){emit("Holly: Live news fetching is unavailable on this build.\n",ctx);return 1;}
 int result=fetch_news(now);if(result){emit(result==-2?"Holly: The outbound reader is busy. Wait for the current request, then ask again.\n":result==-3?"Holly: Please wait 30 seconds between world-news requests.\n":"Holly: I need a DHCP internet connection with DNS and a gateway for world news.\n",ctx);return 1;}
 requested_at=now;start_ticks=ticks;state=1;bulletin[0]=0;emit("Holly: Fetching current world headlines. I'll read them when they're ready. SSH and Telnet users can ask news status.\n",ctx);return 1;
}
