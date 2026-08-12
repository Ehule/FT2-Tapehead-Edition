#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#endif
#include "tapesister/ts_file_browser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define TS_MKDIR(p) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define TS_MKDIR(p) mkdir((p),0700)
#endif
static bool join(const char*d,const char*n,char*out,size_t c){int k=snprintf(out,c,"%s%s%s",d,(d[0]&&d[strlen(d)-1]=='/')?"":"/",n);return k>=0&&(size_t)k<c;}
static void copy_text(char*out,size_t capacity,const char*in){size_t n=strlen(in);if(n>=capacity)n=capacity-1;memcpy(out,in,n);out[n]=0;}
static int compare(const void*a,const void*b){const ts_browser_entry*x=a,*y=b;if(x->directory!=y->directory)return x->directory?-1:1;return strcmp(x->name,y->name);}
static bool compatible(ts_browser_mode mode,const char*n){if(mode!=TS_BROWSER_LOAD)return true;size_t l=strlen(n);return l>=4&&strcmp(n+l-4,".tsr")==0;}
void ts_file_browser_ensure_visible(ts_file_browser*b){if(!b)return;if(b->count==0){b->selected=b->scroll=0;}else{if(b->selected>=b->count)b->selected=b->count-1;if(b->selected<b->scroll)b->scroll=b->selected;if(b->selected>=b->scroll+TS_BROWSER_VISIBLE_ROWS)b->scroll=b->selected-TS_BROWSER_VISIBLE_ROWS+1;}ft2_ui_scrollbar_set(&b->scrollbar,b->count,TS_BROWSER_VISIBLE_ROWS,b->scroll);}
bool ts_file_browser_scroll_to(ts_file_browser*b,size_t scroll){if(!b)return false;size_t end=b->count>TS_BROWSER_VISIBLE_ROWS?b->count-TS_BROWSER_VISIBLE_ROWS:0;if(scroll>end)scroll=end;size_t old=b->scroll;b->scroll=scroll;if(b->count){if(b->selected<b->scroll)b->selected=b->scroll;else if(b->selected>=b->scroll+TS_BROWSER_VISIBLE_ROWS)b->selected=b->scroll+TS_BROWSER_VISIBLE_ROWS-1;}else b->selected=0;ft2_ui_scrollbar_set(&b->scrollbar,b->count,TS_BROWSER_VISIBLE_ROWS,b->scroll);return old!=b->scroll;}
void ts_file_browser_toggle_focus(ts_file_browser*b){if(b)b->focus=b->focus==TS_BROWSER_FOCUS_LIST?TS_BROWSER_FOCUS_FILENAME:TS_BROWSER_FOCUS_LIST;}
bool ts_file_browser_mouse_press(ts_file_browser*b,int x,int y,unsigned clicks){if(!b)return false;if(x>=b->scrollbar.x&&x<b->scrollbar.x+b->scrollbar.w&&y>=b->scrollbar.y&&y<b->scrollbar.y+b->scrollbar.h){b->focus=TS_BROWSER_FOCUS_LIST;size_t old=b->scrollbar.pos;bool hit=ft2_ui_scrollbar_press(&b->scrollbar,x,y);if(hit&&b->scrollbar.pos!=old)ts_file_browser_scroll_to(b,b->scrollbar.pos);return true;}if(x>=82&&x<520&&y>=101&&y<231){b->focus=TS_BROWSER_FOCUS_LIST;size_t row=(size_t)((y-101)/13)+b->scroll;if(row<b->count){b->selected=row;ts_file_browser_ensure_visible(b);if(clicks>=2)ts_file_browser_enter(b,row);}return true;}if(x>=82&&x<538&&y>=235&&y<255){b->focus=TS_BROWSER_FOCUS_FILENAME;return true;}return false;}
bool ts_file_browser_mouse_motion(ts_file_browser*b,int y){if(!b||!b->scrollbar.dragging)return false;bool changed=ft2_ui_scrollbar_drag(&b->scrollbar,y);if(changed)ts_file_browser_scroll_to(b,b->scrollbar.pos);return true;}
void ts_file_browser_mouse_release(ts_file_browser*b){if(b)ft2_ui_scrollbar_release(&b->scrollbar);}
bool ts_file_browser_wheel(ts_file_browser*b,int rows){if(!b)return false;b->focus=TS_BROWSER_FOCUS_LIST;size_t old=b->scrollbar.pos;ft2_ui_scrollbar_step(&b->scrollbar,rows);if(b->scrollbar.pos!=old)ts_file_browser_scroll_to(b,b->scrollbar.pos);return old!=b->scrollbar.pos;}
bool ts_file_browser_move(ts_file_browser*b,ts_browser_key k){if(!b||!b->count)return false;size_t old=b->selected;if(k==TS_BROWSER_KEY_UP&&b->selected)b->selected--;else if(k==TS_BROWSER_KEY_DOWN&&b->selected+1<b->count)b->selected++;else if(k==TS_BROWSER_KEY_PAGE_UP)b->selected=b->selected<TS_BROWSER_VISIBLE_ROWS?0:b->selected-TS_BROWSER_VISIBLE_ROWS;else if(k==TS_BROWSER_KEY_PAGE_DOWN){b->selected+=TS_BROWSER_VISIBLE_ROWS;if(b->selected>=b->count)b->selected=b->count-1;}else if(k==TS_BROWSER_KEY_HOME)b->selected=0;else if(k==TS_BROWSER_KEY_END)b->selected=b->count-1;ts_file_browser_ensure_visible(b);return old!=b->selected;}
static bool scan(ts_file_browser*b,const char*path){b->count=0;b->selected=0;b->scroll=0;b->scrollbar=(ft2_ui_scrollbar){.x=520,.y=101,.w=18,.h=130,.arrows=13};
#ifdef _WIN32
 char pattern[TS_PATH_MAX_BYTES+4];snprintf(pattern,sizeof pattern,"%s\\*",path);WIN32_FIND_DATAA data;HANDLE h=FindFirstFileA(pattern,&data);if(h==INVALID_HANDLE_VALUE)return false;do{if(strcmp(data.cFileName,".")==0||strcmp(data.cFileName,"..")==0)continue;bool dir=(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;if((dir||compatible(b->mode,data.cFileName))&&b->count<TS_BROWSER_MAX_ENTRIES){ts_browser_entry*e=&b->entries[b->count++];copy_text(e->name,sizeof e->name,data.cFileName);e->directory=dir;}}while(FindNextFileA(h,&data));FindClose(h);
#else
 DIR*dir=opendir(path);if(!dir)return false;struct dirent*de;while((de=readdir(dir))!=NULL){if(strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0)continue;char full[TS_PATH_MAX_BYTES+1];struct stat st;if(!join(path,de->d_name,full,sizeof full)||stat(full,&st)!=0)continue;bool isdir=S_ISDIR(st.st_mode);if((isdir||compatible(b->mode,de->d_name))&&b->count<TS_BROWSER_MAX_ENTRIES){ts_browser_entry*e=&b->entries[b->count++];copy_text(e->name,sizeof e->name,de->d_name);e->directory=isdir;}}closedir(dir);
#endif
 qsort(b->entries,b->count,sizeof(b->entries[0]),compare);copy_text(b->directory,sizeof b->directory,path);ts_file_browser_ensure_visible(b);return true;}
bool ts_file_browser_open(ts_file_browser*b,ts_browser_mode m,const char*d,const char*n){if(!b)return false;memset(b,0,sizeof(*b));b->mode=m;b->focus=m==TS_BROWSER_LOAD?TS_BROWSER_FOCUS_LIST:TS_BROWSER_FOCUS_FILENAME;if(!d||!d[0])d=".";char absolute[TS_PATH_MAX_BYTES+1];
#ifdef _WIN32
 if(!_fullpath(absolute,d,sizeof absolute))return false;
#else
 char *resolved=realpath(d,NULL);if(!resolved||strlen(resolved)>=sizeof absolute){free(resolved);return false;}copy_text(absolute,sizeof absolute,resolved);free(resolved);
#endif
 if(!scan(b,absolute)||!ts_text_edit_init(&b->filename,TS_BROWSER_NAME_BYTES+1U,n?n:"")){ts_file_browser_close(b);return false;}return true;}
void ts_file_browser_close(ts_file_browser*b){if(b){ts_text_edit_destroy(&b->filename);memset(b,0,sizeof(*b));}}
bool ts_file_browser_home(ts_file_browser*b){const char*h=getenv(
#ifdef _WIN32
 "USERPROFILE"
#else
 "HOME"
#endif
 );return b&&h&&scan(b,h);}
bool ts_file_browser_root(ts_file_browser*b){return b&&scan(b,
#ifdef _WIN32
 "C:/"
#else
 "/"
#endif
 );}
bool ts_file_browser_parent(ts_file_browser*b){if(!b)return false;char p[TS_PATH_MAX_BYTES+1];snprintf(p,sizeof p,"%s",b->directory);char*s=strrchr(p,'/');if(!s)return false;if(s==p)s[1]=0;else*s=0;return scan(b,p);}
bool ts_file_browser_enter(ts_file_browser*b,size_t i){if(!b||i>=b->count)return false;ts_browser_entry*e=&b->entries[i];if(e->directory){char p[TS_PATH_MAX_BYTES+1];return join(b->directory,e->name,p,sizeof p)&&scan(b,p);}ts_text_edit_destroy(&b->filename);return ts_text_edit_init(&b->filename,TS_BROWSER_NAME_BYTES+1U,e->name);}
bool ts_file_browser_mkdir(ts_file_browser*b,const char*n){if(!b||!n||!n[0]||strchr(n,'/'))return false;char p[TS_PATH_MAX_BYTES+1],directory[TS_PATH_MAX_BYTES+1];snprintf(directory,sizeof directory,"%s",b->directory);return join(directory,n,p,sizeof p)&&TS_MKDIR(p)==0&&scan(b,directory);}
static bool extension(const char*n,const char*ext,char*out,size_t c){size_t l=strlen(n),e=strlen(ext);int k=(l>=e&&strcmp(n+l-e,ext)==0)?snprintf(out,c,"%s",n):snprintf(out,c,"%s%s",n,ext);return n[0]&&k>=0&&(size_t)k<c;}
bool ts_file_browser_result(const ts_file_browser*b,char*f,size_t fc,char*s,size_t sc){if(!b||!f)return false;char name[TS_BROWSER_NAME_BYTES+8];if(b->mode==TS_BROWSER_LOAD||b->mode==TS_BROWSER_SAVE){if(!extension(b->filename.text,".tsr",name,sizeof name)||!join(b->directory,name,f,fc))return false;if(s&&sc)s[0]=0;return true;}char rn[TS_BROWSER_NAME_BYTES+8],wn[TS_BROWSER_NAME_BYTES+8];return s&&extension(b->filename.text,".tsr",rn,sizeof rn)&&extension(b->filename.text,".wav",wn,sizeof wn)&&join(b->directory,rn,f,fc)&&join(b->directory,wn,s,sc);}
