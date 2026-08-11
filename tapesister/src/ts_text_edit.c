#include "tapesister/ts_text_edit.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
bool ts_utf8_valid(const char*s,size_t n){for(size_t i=0;i<n;i++){uint8_t c=(uint8_t)s[i];if(c<0x80){if(c==0)return false;continue;}size_t k;uint32_t v;if((c&0xe0)==0xc0){k=1;v=c&31;if(v<2)return false;}else if((c&0xf0)==0xe0){k=2;v=c&15;}else if((c&0xf8)==0xf0){k=3;v=c&7;}else return false;if(i+k>=n)return false;for(size_t j=0;j<k;j++){c=(uint8_t)s[++i];if((c&0xc0)!=0x80)return false;v=(v<<6)|(c&63);}if((k==2&&v<0x800)||(k==3&&v<0x10000)||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return false;}return true;}
bool ts_text_edit_init(ts_text_edit*e,size_t c,const char*i){if(!e||c<2)return false;memset(e,0,sizeof(*e));e->text=calloc(c,1);if(!e->text)return false;e->capacity=c;e->length=i?strlen(i):0;if(e->length>=c||!ts_utf8_valid(i?i:"",e->length)){ts_text_edit_destroy(e);return false;}memcpy(e->text,i?i:"",e->length+1);e->cursor=e->length;return true;}
void ts_text_edit_destroy(ts_text_edit*e){if(e){free(e->text);memset(e,0,sizeof(*e));}}
static size_t previous(const char*s,size_t p){if(!p)return 0;do p--;while(p&&((uint8_t)s[p]&0xc0)==0x80);return p;}
static size_t next(const char*s,size_t n,size_t p){if(p>=n)return n;p++;while(p<n&&((uint8_t)s[p]&0xc0)==0x80)p++;return p;}
bool ts_text_edit_insert(ts_text_edit*e,const char*s){if(!e||!s)return false;size_t n=strlen(s);if(!n||!ts_utf8_valid(s,n)||e->length+n>=e->capacity)return false;memmove(e->text+e->cursor+n,e->text+e->cursor,e->length-e->cursor+1);memcpy(e->text+e->cursor,s,n);e->cursor+=n;e->length+=n;return true;}
bool ts_text_edit_backspace(ts_text_edit*e){if(!e||!e->cursor)return false;size_t p=previous(e->text,e->cursor);memmove(e->text+p,e->text+e->cursor,e->length-e->cursor+1);e->length-=e->cursor-p;e->cursor=p;return true;}
bool ts_text_edit_delete(ts_text_edit*e){if(!e||e->cursor>=e->length)return false;size_t p=next(e->text,e->length,e->cursor);memmove(e->text+e->cursor,e->text+p,e->length-p+1);e->length-=p-e->cursor;return true;}
void ts_text_edit_left(ts_text_edit*e){if(e)e->cursor=previous(e->text,e->cursor);}void ts_text_edit_right(ts_text_edit*e){if(e)e->cursor=next(e->text,e->length,e->cursor);}void ts_text_edit_home(ts_text_edit*e){if(e)e->cursor=0;}void ts_text_edit_end(ts_text_edit*e){if(e)e->cursor=e->length;}
