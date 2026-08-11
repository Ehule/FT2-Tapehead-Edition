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
 ts_file_browser b;CHECK(ts_file_browser_open(&b,TS_BROWSER_LOAD,root,""));CHECK(b.count==2&&b.entries[0].directory);CHECK(strcmp(b.entries[1].name,"voice.tsr")==0);CHECK(ts_file_browser_enter(&b,1));char first[1200],second[1200];CHECK(ts_file_browser_result(&b,first,sizeof first,second,sizeof second));CHECK(strcmp(first,recipe)==0);CHECK(ts_file_browser_enter(&b,0));CHECK(strcmp(b.directory,sub)==0);CHECK(ts_file_browser_parent(&b));CHECK(strcmp(b.directory,root)==0);ts_file_browser_close(&b);
 CHECK(ts_file_browser_open(&b,TS_BROWSER_SAVE,root,"new voice"));CHECK(ts_file_browser_result(&b,first,sizeof first,second,sizeof second));CHECK(strstr(first,"new voice.tsr")!=NULL);CHECK(ts_file_browser_mkdir(&b,"created"));CHECK(ts_file_browser_root(&b));CHECK(b.directory[0]);CHECK(ts_file_browser_home(&b));ts_file_browser_close(&b);
 CHECK(ts_file_browser_open(&b,TS_BROWSER_BAKE,root,"pair"));CHECK(ts_file_browser_result(&b,first,sizeof first,second,sizeof second));CHECK(strstr(first,"pair.tsr")&&strstr(second,"pair.wav"));ts_file_browser_close(&b);CHECK(!ts_file_browser_open(&b,TS_BROWSER_LOAD,"/definitely/not/accessible",NULL));
 remove(recipe);remove(other);char created[1200];snprintf(created,sizeof created,"%s/created",root);rmdir(created);rmdir(sub);rmdir(root);puts("file browser tests passed");return 0;}
