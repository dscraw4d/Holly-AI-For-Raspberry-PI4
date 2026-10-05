/* Export the exact production HDMI renderer for visual checks. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "dashboard.h"
static uint32_t pixels[640*480];
int main(int argc,char **argv){
 unsigned frame=argc==2?(unsigned)strtoul(argv[1],0,10):0;if(frame>6)return 1;
 if(holly_dashboard_render(pixels,640,480,640,640*480,HOLLY_SPEAKING,frame))return 1;
 if(printf("P6\n640 480\n255\n")<0)return 1;
 for(unsigned i=0;i<640*480;i++){
  unsigned char rgb[]={(unsigned char)(pixels[i]>>16),(unsigned char)(pixels[i]>>8),(unsigned char)pixels[i]};
  if(fwrite(rgb,1,3,stdout)!=3)return 1;
 }
 return ferror(stdout)?1:0;
}
