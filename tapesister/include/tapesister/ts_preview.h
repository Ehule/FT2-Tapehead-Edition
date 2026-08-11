#pragma once

#include "tapesister/ts_editor.h"
#include "tapesister/ts_audition.h"
#include <stdatomic.h>

typedef struct ts_preview ts_preview;
typedef struct ts_preview_pool ts_preview_pool;
typedef struct ts_render_worker ts_render_worker;
typedef bool (*ts_render_worker_function)(const ts_recipe *recipe,
    ts_rendered_sample *output, ts_render_report *report, void *userdata);

struct ts_preview {
  ts_rendered_sample render;
  ts_audition_source source;
  uint64_t generation, recipe_identity, pcm_identity;
  atomic_uint references;
  atomic_bool retired;
  ts_preview *next;
};

struct ts_preview_pool { ts_preview *all, *current; size_t retained; };

void ts_preview_retain(ts_preview *preview);
void ts_preview_release_callback(ts_preview *preview);
void ts_preview_pool_init(ts_preview_pool *pool);
void ts_preview_pool_publish(ts_preview_pool *pool, ts_preview *preview);
void ts_preview_pool_collect(ts_preview_pool *pool);
void ts_preview_pool_destroy(ts_preview_pool *pool);

bool ts_render_worker_create(ts_render_worker **worker);
bool ts_render_worker_create_with_function(ts_render_worker **worker,
    ts_render_worker_function function, void *userdata);
uint64_t ts_render_worker_request(ts_render_worker *worker,
                                  const ts_recipe *recipe);
/* Transfers a matching completed result to the caller. */
ts_preview *ts_render_worker_take(ts_render_worker *worker);
bool ts_render_worker_rendering(ts_render_worker *worker);
bool ts_render_worker_failed(ts_render_worker *worker, uint64_t *generation,
                             char *error, size_t capacity);
uint64_t ts_render_worker_requested_generation(ts_render_worker *worker);
void ts_render_worker_destroy(ts_render_worker *worker);
