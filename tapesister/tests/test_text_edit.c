#include "tapesister/ts_text_edit.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){ts_text_edit e;CHECK(ts_text_edit_init(&e,16,"a b"));CHECK(ts_text_edit_insert(&e,"é"));CHECK(strcmp(e.text,"a bé")==0);ts_text_edit_left(&e);CHECK(ts_text_edit_backspace(&e));CHECK(strcmp(e.text,"a é")==0);ts_text_edit_home(&e);CHECK(ts_text_edit_delete(&e));CHECK(strcmp(e.text," é")==0);ts_text_edit_end(&e);CHECK(!ts_text_edit_insert(&e,"\xff"));CHECK(ts_utf8_valid("é",2));CHECK(!ts_utf8_valid("\xc0\x80",2));ts_text_edit_destroy(&e);CHECK(ts_text_edit_init(&e,5,""));CHECK(ts_text_edit_insert(&e,"éé"));CHECK(!ts_text_edit_insert(&e,"x"));ts_text_edit_destroy(&e);puts("text edit tests passed");return 0;}
