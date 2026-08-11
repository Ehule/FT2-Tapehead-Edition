#include "tapesister/ts_preview.h"
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

static ts_preview *make_preview(const ts_recipe *recipe,uint64_t generation){
  ts_preview*p=calloc(1,sizeof(*p));ts_render_report report;if(!p)return NULL;
  if(!ts_render(recipe,&p->render,&report)){free(p);return NULL;}
  p->generation=generation;p->recipe_identity=ts_recipe_identity(recipe);
  p->source.samples=p->render.samples;p->source.frame_count=p->render.frame_count;
  p->source.sample_rate=p->render.sample_rate;p->source.root_midi_note=recipe->root_midi_note;p->source.owner=p;
  atomic_init(&p->references,0);atomic_init(&p->retired,false);return p;
}
static ts_preview *wait_result(ts_render_worker*w){for(unsigned i=0;i<1000000;i++){ts_preview*p=ts_render_worker_take(w);if(p)return p;thrd_yield();}return NULL;}
static bool controlled_render(const ts_recipe*r,ts_rendered_sample*out,ts_render_report*report,void*userdata){(void)userdata;if(r->seed==42)return false;return ts_render(r,out,report);}
int main(void){
  ts_render_worker*w=NULL;CHECK(ts_render_worker_create_with_function(&w,controlled_render,NULL));
  CHECK(ts_render_worker_requested_generation(w)==0);
  uint64_t g1=ts_render_worker_request(w,ts_fixture_recipe(0));
  uint64_t g2=ts_render_worker_request(w,ts_fixture_recipe(1));
  uint64_t g3=ts_render_worker_request(w,ts_fixture_recipe(2));
  CHECK(g1==1&&g2==2&&g3==3);ts_preview*latest=wait_result(w);CHECK(latest&&latest->generation==3);
  atomic_store(&latest->retired,true);ts_preview_pool trash;ts_preview_pool_init(&trash);trash.all=latest;trash.retained=1;ts_preview_pool_collect(&trash);CHECK(trash.retained==0);
  ts_recipe failing=*ts_fixture_recipe(0);failing.seed=42;uint64_t failed_request=ts_render_worker_request(w,&failing);uint64_t failed=0;char error[128];bool saw_failure=false;
  for(unsigned i=0;i<1000000&&!saw_failure;i++){saw_failure=ts_render_worker_failed(w,&failed,error,sizeof error);thrd_yield();}
  CHECK(saw_failure&&failed==failed_request&&error[0]);
  uint64_t recovery=ts_render_worker_request(w,ts_fixture_recipe(0));latest=wait_result(w);CHECK(latest&&latest->generation==recovery);
  atomic_store(&latest->retired,true);trash.all=latest;trash.retained=1;ts_preview_pool_collect(&trash);
  ts_render_worker_destroy(w);

  ts_preview_pool pool;ts_preview_pool_init(&pool);ts_preview*a=make_preview(ts_fixture_recipe(0),10),*b=make_preview(ts_fixture_recipe(1),11);CHECK(a&&b);
  ts_preview_pool_publish(&pool,a);CHECK(atomic_load(&a->references)==1);
  ts_audition_mixer mixer;CHECK(ts_audition_init(&mixer,48000));CHECK(ts_audition_note_on(&mixer,&a->source,60));CHECK(atomic_load(&a->references)==2);
  ts_preview_pool_publish(&pool,b);CHECK(pool.current==b&&atomic_load(&a->references)==1);
  CHECK(ts_audition_note_on(&mixer,&b->source,64));CHECK(atomic_load(&b->references)==2);
  float audio[512];ts_audition_mix(&mixer,audio,256);CHECK(ts_audition_cursor_position(&mixer)>=0.0);
  mixer.mode=TS_AUDITION_GATED;ts_audition_note_off(&mixer,60);ts_audition_note_off(&mixer,64);ts_audition_mix(&mixer,audio,128);
  ts_preview_pool_collect(&pool);CHECK(pool.retained==1&&pool.current==b);
  ts_audition_stop_all(&mixer);CHECK(ts_audition_cursor_position(&mixer)<0.0);ts_audition_discard_all(&mixer);ts_preview_pool_collect(&pool);ts_preview_pool_destroy(&pool);
  puts("preview worker/ownership tests passed");return 0;
}
