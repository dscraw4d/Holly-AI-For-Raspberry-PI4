#include "language_model.h"
#include "language_model_weights.h"
#define EMB 8
#define HID 64
#define SCALE 4096
static const int16_t *active_weights;
void holly_lm_use(const int16_t *weights){active_weights=weights;}
void holly_lm_default(int16_t *w){
 const int16_t *parts[]={lm_embedding,lm_hidden,lm_hidden_bias,lm_output,lm_output_bias};
 const unsigned sizes[]={784,32768,64,6272,98};
 for(unsigned p=0;p<5;p++)for(unsigned i=0;i<sizes[p];i++)*w++=parts[p][i];
}
void holly_lm_forward(const int16_t *w,const uint8_t *context,int32_t *hidden,int32_t *logits){
 const int16_t *embedding=w?w:lm_embedding,*matrix=w?w+784:lm_hidden;
 const int16_t *bias=w?w+33552:lm_hidden_bias,*output=w?w+33616:lm_output,*ob=w?w+39888:lm_output_bias;
 for(unsigned j=0;j<HID;j++){
  int64_t sum=0;
  for(unsigned i=0;i<HOLLY_LM_CONTEXT;i++){
   unsigned token=context[i]<HOLLY_LM_VOCAB?context[i]:0;
   for(unsigned k=0;k<EMB;k++)sum+=(int64_t)embedding[token*EMB+k]*matrix[(i*EMB+k)*HID+j];
  }
  sum=sum/SCALE+bias[j];hidden[j]=(int32_t)(sum>SCALE?SCALE:sum < -SCALE?-SCALE:sum);
 }
 for(unsigned j=0;j<HOLLY_LM_VOCAB;j++){
  int64_t sum=0;for(unsigned k=0;k<HID;k++)sum+=(int64_t)hidden[k]*output[k*HOLLY_LM_VOCAB+j];
  logits[j]=(int32_t)(sum/SCALE+ob[j]);
 }
}
void holly_lm_logits(const uint8_t context[HOLLY_LM_CONTEXT],int32_t logits[HOLLY_LM_VOCAB]){
 int32_t hidden[HID];holly_lm_forward(active_weights,context,hidden,logits);
}
static void push(uint8_t *context,unsigned token) {
    for(unsigned i=1;i<HOLLY_LM_CONTEXT;i++)context[i-1]=context[i];
    context[HOLLY_LM_CONTEXT-1]=(uint8_t)token;
}
int holly_lm_generate(const char *prompt,char *output,unsigned capacity) {
    if(!output||!capacity)return -1;
    output[0]=0;
    if(!prompt)return -1;
    uint8_t context[HOLLY_LM_CONTEXT]={0};
    unsigned n=0;
    for(;prompt[n];n++) {
        unsigned char c=(unsigned char)prompt[n];
        if(n>=256 || (c!='\n' && (c<32 || c>126)))return -1;
        push(context,c=='\n'?2:c-29);
    }
    unsigned used=0;
    while(used+1<capacity && used<HOLLY_LM_LIMIT) {
        int32_t logits[HOLLY_LM_VOCAB];holly_lm_logits(context,logits);
        unsigned best=1;
        for(unsigned i=2;i<HOLLY_LM_VOCAB;i++)if(logits[i]>logits[best])best=i;
        if(best==1)break;
        output[used++]=best==2?'\n':(char)(best+29);push(context,best);
    }
    output[used]=0;return (int)used;
}
