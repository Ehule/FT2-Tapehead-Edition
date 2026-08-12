#include "ft2_shared_ui.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 uint8_t pixels[632*400];memset(pixels,0,sizeof pixels);ft2_ui_surface s={pixels,632,400,632};
 CHECK(ft2_ui_text_supported("> - HOME ROOT PARENT NEW FOLDER"));CHECK(!ft2_ui_text_supported("\x7f"));
 ft2_ui_text(&s,0,0,"DIR > FILE -",4);size_t ink=0;for(size_t i=0;i<sizeof pixels;i++)ink+=pixels[i]!=0;CHECK(ink>20);
 ft2_ui_bevel(&s,20,20,30,12,1,2,3,false);CHECK(pixels[20*632+20]==2);ft2_ui_bevel(&s,20,20,30,12,1,2,3,true);CHECK(pixels[20*632+20]==3);
 ft2_ui_scrollbar b={.x=0,.y=0,.w=18,.h=130,.arrows=13};ft2_ui_scrollbar_set(&b,0,10,0);CHECK(b.thumb_h==104);ft2_ui_scrollbar_set(&b,5,10,0);CHECK(b.thumb_h==104);ft2_ui_scrollbar_set(&b,100,10,0);CHECK(b.thumb_h==10);int top=b.thumb_y;CHECK(ft2_ui_scrollbar_step(&b,10)&&b.thumb_y>top);CHECK(ft2_ui_scrollbar_press(&b,5,b.thumb_y));CHECK(b.dragging);CHECK(ft2_ui_scrollbar_drag(&b,100));ft2_ui_scrollbar_release(&b);CHECK(!b.dragging);
 puts("shared UI tests passed");return 0;
}
