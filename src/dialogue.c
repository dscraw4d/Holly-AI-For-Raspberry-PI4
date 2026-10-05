#include "dialogue.h"
#include "smp.h"
#include "dialogue_weights.h"
#define E 24
#define H 192
#define SCALE 4096
unsigned holly_dialogue_parameters(void){return DL_PARAMETERS;}
unsigned holly_dialogue_vocabulary(void){return DL_VOCAB;}
static char lower(char c){return c>='A'&&c<='Z'?(char)(c+32):c;}
static int wordchar(char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9');}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int encode(const char *text,uint16_t *ids,unsigned limit,unsigned *count,unsigned *unknown){
    unsigned n=0,used=0,bad=0,total=0;
    while(text[n]){if(n>=640||(unsigned char)text[n]<32||(unsigned char)text[n]>126)return -1;n++;}
    unsigned at=0;
    while(at<n){
        if(text[at]==' '){at++;continue;}
        char token[65];unsigned length=0;
        if(wordchar(lower(text[at]))){
            while(at<n&&wordchar(lower(text[at]))){if(length<64)token[length]=lower(text[at]);length++;at++;}
        }else{token[0]=lower(text[at++]);length=1;}
        unsigned id=2;
        if(length<sizeof(token)){
            token[length]=0;
            for(unsigned i=4;i<DL_VOCAB;i++)if(equal(token,dl_vocab[i])){id=i;break;}
        }
        total++;if(id==2)bad++;
        if(used==limit){for(unsigned i=1;i<limit;i++)ids[i-1]=ids[i];used--;}
        ids[used++]=(uint16_t)id;
    }
    *count=used;*unknown=bad;
    return total?0:1;
}
struct inference_job {const int32_t *input;int32_t *hidden,*scores;};
static void hidden_work(unsigned core,void *context){
 struct inference_job *a=context;
 for(unsigned j=(H*core)/4;j<(H*(core+1))/4;j++){
  int64_t sum=0;
  for(unsigned i=0;i<(HOLLY_DIALOGUE_WORDS+2)*E;i++)sum+=(int64_t)a->input[i]*dl_hidden[i*H+j];
  sum=sum/SCALE+dl_hidden_bias[j];a->hidden[j]=(int32_t)(sum>SCALE?SCALE:sum < -SCALE?-SCALE:sum);
 }
}
static void output_work(unsigned core,void *context){
 struct inference_job *a=context;
 for(unsigned j=(DL_VOCAB*core)/4;j<(DL_VOCAB*(core+1))/4;j++){
  int64_t sum=0;for(unsigned i=0;i<H;i++)sum+=(int64_t)a->hidden[i]*dl_output[i*DL_VOCAB+j];
  a->scores[j]=(int32_t)(sum/SCALE+dl_output_bias[j]);
 }
}
void holly_dialogue_logits(const uint16_t *user,unsigned count,const uint16_t *previous,
                          unsigned previous_count,const uint16_t words[HOLLY_DIALOGUE_WORDS],int32_t *scores){
    int32_t input[(HOLLY_DIALOGUE_WORDS+2)*E]={0},hidden[H];
    if(count>HOLLY_DIALOGUE_USER_WORDS)count=HOLLY_DIALOGUE_USER_WORDS;
    if(previous_count>HOLLY_DIALOGUE_PREVIOUS_WORDS)previous_count=HOLLY_DIALOGUE_PREVIOUS_WORDS;
    for(unsigned j=0;j<E;j++){
        int64_t sum=0;unsigned used=0;
        for(unsigned i=0;i<count;i++)if(user[i]){sum+=dl_embedding[(user[i]<DL_VOCAB?user[i]:2)*E+j];used++;}
        input[j]=used?(int32_t)(sum/used):0;sum=0;used=0;
        for(unsigned i=0;i<previous_count;i++)if(previous[i]){sum+=dl_embedding[(previous[i]<DL_VOCAB?previous[i]:2)*E+j];used++;}
        input[E+j]=used?(int32_t)(sum/used):0;
    }
    for(unsigned i=0;i<HOLLY_DIALOGUE_WORDS;i++)for(unsigned j=0;j<E;j++)
        input[(i+2)*E+j]=dl_embedding[(words[i]<DL_VOCAB?words[i]:2)*E+j];
    struct inference_job job={input,hidden,scores};
    holly_smp_parallel(hidden_work,&job);
    holly_smp_parallel(output_work,&job);
}
int holly_dialogue_generate(const char *user,const char *previous,char *out,unsigned capacity){
    if(!user||!previous||!out||!capacity)return -1;
    out[0]=0;
    uint16_t ids[HOLLY_DIALOGUE_USER_WORDS],old[HOLLY_DIALOGUE_PREVIOUS_WORDS],words[HOLLY_DIALOGUE_WORDS]={0};
    unsigned count=0,old_count=0,unknown=0,old_unknown=0;
    if(encode(user,ids,HOLLY_DIALOGUE_USER_WORDS,&count,&unknown)||
       encode(previous,old,HOLLY_DIALOGUE_PREVIOUS_WORDS,&old_count,&old_unknown)<0||
       !count||unknown*2>count)return 1;
    words[HOLLY_DIALOGUE_WORDS-1]=3;
    unsigned used=0;
    for(unsigned step=0;step<32;step++){
        int32_t scores[DL_VOCAB];holly_dialogue_logits(ids,count,old,old_count,words,scores);
        unsigned best=1;
        for(unsigned j=4;j<DL_VOCAB;j++)if(scores[j]>scores[best])best=j;
        if(best==1)return used?0:1;
        const char *token=dl_vocab[best];unsigned n=0;while(token[n])n++;
        int punctuation=(n==1&&(token[0]=='.'||token[0]==','||token[0]=='?'||token[0]=='!'||token[0]==':'||token[0]==';'||token[0]=='>'));
        unsigned space=used&&!punctuation;
        if(used+space+n>=capacity){out[0]=0;return 1;}
        if(space)out[used++]=' ';
        for(unsigned i=0;i<n;i++)out[used++]=token[i];
        out[used]=0;
        if(out[0]>='a'&&out[0]<='z')out[0]=(char)(out[0]-32);
        for(unsigned i=1;i<HOLLY_DIALOGUE_WORDS;i++)words[i-1]=words[i];
        words[HOLLY_DIALOGUE_WORDS-1]=(uint16_t)best;
    }
    out[0]=0;return 1;
}
