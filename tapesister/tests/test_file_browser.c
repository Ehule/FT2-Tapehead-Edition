#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "tapesister/ts_file_browser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
#ifdef _WIN32
 char root[MAX_PATH];CHECK(GetTempPathA(MAX_PATH,root)>0);
#else
 char root[]="/tmp/tapesister_browser_XXXXXX";CHECK(mkdtemp(root)!=NULL);
#endif
 char sub[1200],recipe[1200],other[1200];snprintf(sub,sizeof sub,"%s/sub",root);snprintf(recipe,sizeof recipe,"%s/voice.tsr",root);snprintf(other,sizeof other,"%s/ignore.txt",root);
#ifdef _WIN32
 CHECK(_mkdir(sub)==0);
#else
 CHECK(mkdir(sub,0700)==0);
#endif
 FILE*f=fopen(recipe,"wb");CHECK(f);fputs("x",f);fclose(f);f=fopen(other,"wb");CHECK(f);fputs("x",f);fclose(f);
 ts_file_browser b;CHECK(ts_file_browser_open(&b,TS_BROWSER_LOAD,root,""));CHECK(b.focus==TS_BROWSER_FOCUS_LIST);CHECK(b.count==2&&b.entries[0].directory);CHECK(strcmp(b.entries[1].name,"voice.tsr")==0);CHECK(ts_file_browser_enter(&b,1));char first[1200],second[1200];CHECK(ts_file_browser_result(&b,first,sizeof first,second,sizeof second));CHECK(strcmp(first,recipe)==0);CHECK(ts_file_browser_enter(&b,0));CHECK(strcmp(b.directory,sub)==0);CHECK(ts_file_browser_parent(&b));CHECK(strcmp(b.directory,root)==0);ts_file_browser_close(&b);
 CHECK(ts_file_browser_open(&b,TS_BROWSER_LOAD,root,""));CHECK(ts_file_browser_move(&b,TS_BROWSER_KEY_END));CHECK(b.selected==b.count-1&&b.selected>=b.scroll);CHECK(ts_file_browser_move(&b,TS_BROWSER_KEY_HOME));CHECK(b.selected==0&&b.scroll==0);ts_file_browser_close(&b);
 CHECK(ts_file_browser_open(&b,TS_BROWSER_SAVE,root,"new voice"));CHECK(b.focus==TS_BROWSER_FOCUS_FILENAME);size_t selected=b.selected,scroll=b.scroll;ts_text_edit_home(&b.filename);CHECK(b.filename.cursor==0&&b.selected==selected&&b.scroll==scroll);ts_text_edit_end(&b.filename);CHECK(b.filename.cursor==b.filename.length&&b.selected==selected);ts_file_browser_toggle_focus(&b);CHECK(b.focus==TS_BROWSER_FOCUS_LIST);ts_file_browser_move(&b,TS_BROWSER_KEY_END);CHECK(b.filename.cursor==b.filename.length);CHECK(ts_file_browser_result(&b,first,sizeof first,second,sizeof second));CHECK(strstr(first,"new voice.tsr")!=NULL);CHECK(ts_file_browser_mkdir(&b,"created"));CHECK(ts_file_browser_root(&b));CHECK(b.directory[0]);CHECK(ts_file_browser_home(&b));ts_file_browser_close(&b);
 CHECK(ts_file_browser_open(&b,TS_BROWSER_BAKE,root,"pair"));CHECK(b.focus==TS_BROWSER_FOCUS_FILENAME);CHECK(ts_file_browser_result(&b,first,sizeof first,second,sizeof second));CHECK(strstr(first,"pair.tsr")&&strstr(second,"pair.wav"));ts_file_browser_close(&b);CHECK(!ts_file_browser_open(&b,TS_BROWSER_LOAD,"/definitely/not/accessible",NULL));
 CHECK(ts_file_browser_open(&b,TS_BROWSER_LOAD,root,""));b.count=30;b.selected=0;b.scroll=0;for(size_t i=0;i<b.count;i++){snprintf(b.entries[i].name,sizeof b.entries[i].name,"item%02zu.tsr",i);b.entries[i].directory=false;}ts_file_browser_ensure_visible(&b);
 CHECK(ts_file_browser_mouse_press(&b,521,230,1));CHECK(b.scroll==1&&b.selected==1&&b.scrollbar.pos==b.scroll);CHECK(ts_file_browser_mouse_press(&b,521,102,1));CHECK(b.scroll==0&&b.selected==1);
 CHECK(ts_file_browser_mouse_press(&b,521,180,1));CHECK(b.scroll==10&&b.selected==10);CHECK(ts_file_browser_mouse_press(&b,521,140,1));CHECK(b.scroll==0&&b.selected==9);
 size_t before=b.selected;char unchanged[TS_PATH_MAX_BYTES+1U];snprintf(unchanged,sizeof unchanged,"%s",b.directory);CHECK(ts_file_browser_mouse_press(&b,521,160,2));CHECK(strcmp(b.directory,unchanged)==0&&b.count==30);int thumb=b.scrollbar.thumb_y+b.scrollbar.thumb_h/2;CHECK(ts_file_browser_mouse_press(&b,521,thumb,1));CHECK(b.scrollbar.dragging);CHECK(ts_file_browser_mouse_motion(&b,400));CHECK(b.scroll==20&&b.selected>=b.scroll&&b.selected<b.scroll+TS_BROWSER_VISIBLE_ROWS);CHECK(b.scrollbar.pos==b.scroll);ts_file_browser_mouse_release(&b);CHECK(!b.scrollbar.dragging);CHECK(!ts_file_browser_mouse_motion(&b,-100));
 CHECK(ts_file_browser_wheel(&b,-1));CHECK(b.scroll==19&&b.scrollbar.pos==19);ts_file_browser_move(&b,TS_BROWSER_KEY_HOME);CHECK(b.selected==0&&b.scroll==0&&b.scrollbar.pos==0);ts_file_browser_move(&b,TS_BROWSER_KEY_END);CHECK(b.selected==29&&b.scroll==20&&b.scrollbar.pos==20);
 CHECK(ts_file_browser_mouse_press(&b,90,240,1)&&b.focus==TS_BROWSER_FOCUS_FILENAME);ts_file_browser_toggle_focus(&b);CHECK(b.focus==TS_BROWSER_FOCUS_LIST);before=b.selected;CHECK(ts_file_browser_mouse_press(&b,521,102,2));CHECK(b.selected>=b.scroll&&b.selected<b.scroll+TS_BROWSER_VISIBLE_ROWS&&b.selected!=before);ts_file_browser_close(&b);
 remove(recipe);remove(other);char created[1200];snprintf(created,sizeof created,"%s/created",root);rmdir(created);rmdir(sub);rmdir(root);puts("file browser tests passed");return 0;}
