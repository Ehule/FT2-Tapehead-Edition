#include "tapesister/ts_preview.h"
#include <threads.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ts_render_request { ts_owned_recipe recipe; uint64_t generation, identity; bool present; } ts_render_request;
struct ts_render_worker {
  mtx_t mutex; cnd_t condition; thrd_t thread;
  bool stopping, running, started;
  uint64_t requested, rendering, failed;
  char error[128];
  ts_render_request pending;
  ts_preview *completed;
  ts_render_worker_function function; void *userdata;
};

static uint64_t hash_bytes(const uint8_t *bytes,size_t length){uint64_t h=UINT64_C(1469598103934665603);for(size_t i=0;i<length;i++){h^=bytes[i];h*=UINT64_C(1099511628211);}return h;}
static void preview_destroy(ts_preview*p){if(!p)return;ts_rendered_sample_free(&p->render);free(p);}
void ts_preview_retain(ts_preview*p){if(p)atomic_fetch_add_explicit(&p->references,1,memory_order_relaxed);}
void ts_preview_release_callback(ts_preview*p){if(p&&atomic_fetch_sub_explicit(&p->references,1,memory_order_release)==1)atomic_store_explicit(&p->retired,true,memory_order_release);}
void ts_preview_pool_init(ts_preview_pool*p){if(p)memset(p,0,sizeof(*p));}
void ts_preview_pool_publish(ts_preview_pool*p,ts_preview*n){if(!p||!n)return;n->next=p->all;p->all=n;p->retained++;ts_preview_retain(n);ts_preview *old=p->current;p->current=n;if(old)ts_preview_release_callback(old);}
void ts_preview_pool_collect(ts_preview_pool*p){if(!p)return;ts_preview **at=&p->all;while(*at){ts_preview*n=*at;if(atomic_load_explicit(&n->retired,memory_order_acquire)&&atomic_load_explicit(&n->references,memory_order_acquire)==0){*at=n->next;p->retained--;preview_destroy(n);}else at=&n->next;}}
void ts_preview_pool_destroy(ts_preview_pool*p){if(!p)return;if(p->current){ts_preview_release_callback(p->current);p->current=NULL;}ts_preview_pool_collect(p);ts_preview*n=p->all;while(n){ts_preview*next=n->next;preview_destroy(n);n=next;}memset(p,0,sizeof(*p));}
static bool default_render(const ts_recipe*r,ts_rendered_sample*out,ts_render_report*report,void*userdata){(void)userdata;return ts_render(r,out,report);}
static ts_preview *render_request(ts_render_worker*w,const ts_render_request*r){ts_preview*p=calloc(1,sizeof(*p));ts_render_report report;if(!p)return NULL;if(!w->function(&r->recipe.value,&p->render,&report,w->userdata)){free(p);return NULL;}uint8_t*pcm=NULL;size_t length=0;ts_io_error error;if(ts_pcm16_encode(&p->render,&pcm,&length,&error)!=TS_IO_OK){preview_destroy(p);return NULL;}p->generation=r->generation;p->recipe_identity=r->identity;p->pcm_identity=hash_bytes(pcm,length);free(pcm);p->source.samples=p->render.samples;p->source.frame_count=p->render.frame_count;p->source.sample_rate=p->render.sample_rate;p->source.root_midi_note=r->recipe.value.root_midi_note;p->source.owner=p;atomic_init(&p->references,0);atomic_init(&p->retired,false);return p;}
static int worker_main(void*argument){ts_render_worker*w=argument;for(;;){mtx_lock(&w->mutex);while(!w->stopping&&!w->pending.present)cnd_wait(&w->condition,&w->mutex);if(w->stopping){mtx_unlock(&w->mutex);break;}ts_render_request request=w->pending;memset(&w->pending,0,sizeof(w->pending));w->running=true;w->rendering=request.generation;mtx_unlock(&w->mutex);ts_preview*result=render_request(w,&request);ts_owned_recipe_destroy(&request.recipe);mtx_lock(&w->mutex);w->running=false;if(!w->stopping&&request.generation==w->requested){if(result){if(w->completed)preview_destroy(w->completed);w->completed=result;w->failed=0;w->error[0]=0;}else{w->failed=request.generation;snprintf(w->error,sizeof w->error,"render failed for generation %llu",(unsigned long long)request.generation);}}else preview_destroy(result);mtx_unlock(&w->mutex);}return 0;}
bool ts_render_worker_create_with_function(ts_render_worker**out,ts_render_worker_function function,void*userdata){if(!out)return false;ts_render_worker*w=calloc(1,sizeof(*w));if(!w)return false;if(mtx_init(&w->mutex,mtx_plain)!=thrd_success){free(w);return false;}if(cnd_init(&w->condition)!=thrd_success){mtx_destroy(&w->mutex);free(w);return false;}w->function=function?function:default_render;w->userdata=userdata;if(thrd_create(&w->thread,worker_main,w)!=thrd_success){cnd_destroy(&w->condition);mtx_destroy(&w->mutex);free(w);return false;}w->started=true;*out=w;return true;}
bool ts_render_worker_create(ts_render_worker**out){return ts_render_worker_create_with_function(out,NULL,NULL);}
uint64_t ts_render_worker_request(ts_render_worker*w,const ts_recipe*r){if(!w||!r||!ts_recipe_validate(r))return 0;ts_owned_recipe copy={0};if(!ts_owned_recipe_copy(&copy,r))return 0;mtx_lock(&w->mutex);if(w->stopping){mtx_unlock(&w->mutex);ts_owned_recipe_destroy(&copy);return 0;}if(w->pending.present)ts_owned_recipe_destroy(&w->pending.recipe);w->requested++;w->pending.recipe=copy;w->pending.generation=w->requested;w->pending.identity=ts_recipe_identity(r);w->pending.present=true;w->failed=0;w->error[0]=0;cnd_signal(&w->condition);uint64_t generation=w->requested;mtx_unlock(&w->mutex);return generation;}
ts_preview *ts_render_worker_take(ts_render_worker*w){if(!w)return NULL;mtx_lock(&w->mutex);ts_preview*p=w->completed;w->completed=NULL;mtx_unlock(&w->mutex);return p;}
bool ts_render_worker_rendering(ts_render_worker*w){if(!w)return false;mtx_lock(&w->mutex);bool value=w->running||w->pending.present;mtx_unlock(&w->mutex);return value;}
bool ts_render_worker_failed(ts_render_worker*w,uint64_t*g,char*e,size_t c){if(!w)return false;mtx_lock(&w->mutex);bool failed=w->failed==w->requested&&w->failed!=0;if(g)*g=w->failed;if(e&&c)snprintf(e,c,"%s",w->error);mtx_unlock(&w->mutex);return failed;}
uint64_t ts_render_worker_requested_generation(ts_render_worker*w){if(!w)return 0;mtx_lock(&w->mutex);uint64_t g=w->requested;mtx_unlock(&w->mutex);return g;}
void ts_render_worker_destroy(ts_render_worker*w){if(!w)return;mtx_lock(&w->mutex);w->stopping=true;if(w->pending.present){ts_owned_recipe_destroy(&w->pending.recipe);w->pending.present=false;}cnd_broadcast(&w->condition);mtx_unlock(&w->mutex);if(w->started)thrd_join(w->thread,NULL);preview_destroy(w->completed);cnd_destroy(&w->condition);mtx_destroy(&w->mutex);free(w);}
