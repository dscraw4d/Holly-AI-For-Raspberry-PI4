#include "face.h"
static int oval(int x,int y,int cx,int cy,int rx,int ry) {
    int dx=x-cx,dy=y-cy;
    return (int64_t)dx*dx*ry*ry+(int64_t)dy*dy*rx*rx <= (int64_t)rx*rx*ry*ry;
}
int holly_face_render(uint32_t *pixels,unsigned width,unsigned height,
                      unsigned stride,size_t capacity,
                      enum holly_expression expression,unsigned voice_level) {
    if(!pixels||width<64||height<64||width>4096||height>4096||stride<width||
       (size_t)stride*(size_t)height>capacity||voice_level>255||expression>HOLLY_BLINKING)return -1;
    int mouth=expression==HOLLY_SPEAKING ? 11+(int)voice_level/7 : 9;
    for(unsigned y=0;y<height;y++) {
        int ny=(int)((uint64_t)y*1000u/height);
        for(unsigned x=0;x<width;x++) {
            int nx=(int)((uint64_t)x*1000u/width);
            uint32_t c=(y%7u==0) ? 0x000f20u : 0x071629u;
            if(oval(nx,ny,500,510,275,410))c=0x1d565fu;
            if(oval(nx,ny,500,510,255,391))c=0xa9c2c1u;
            if(oval(nx,ny,500,240,243,133) && ny<301)c=0x26333au;
            int brow=expression==HOLLY_THINKING ? -13 : 0;
            if(((nx>=352&&nx<=455)||(nx>=545&&nx<=648)) &&
               ny>=408+brow && ny<=418+brow)c=0x26333au;
            if(expression==HOLLY_BLINKING){
                if(((nx>=354&&nx<=456)||(nx>=544&&nx<=646))&&ny>=468&&ny<=474)c=0x20343bu;
            }else{
                if(oval(nx,ny,405,470,51,30)||oval(nx,ny,595,470,51,30))c=0xeaf5ecu;
                if(oval(nx,ny,409,472,17,23)||oval(nx,ny,591,472,17,23))c=0x20343bu;
            }
            if(nx>=493&&nx<=507&&ny>=515&&ny<=590)c=0x7c999cu;
            if(oval(nx,ny,500,660,100,mouth+10))c=0x80575bu;
            if(oval(nx,ny,500,660,88,mouth))c=0x202631u;
            if(mouth>17 && oval(nx,ny,500,680,58,9))c=0xe2d7c6u;
            pixels[(size_t)y*stride+x]=c;
        }
    }
    return 0;
}
