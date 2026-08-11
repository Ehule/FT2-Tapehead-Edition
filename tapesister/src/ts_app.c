#include "tapesister/ts_app.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *factory_names[TS_FACTORY_RECIPES] = {
    "clean_sustain.tsr",  "percussive_pluck.tsr", "noisy_metal.tsr",
    "unstable_drone.tsr", "digital_bass.tsr",     "spacious_decay.tsr"};

static bool history_copy(ts_recipe_history *dst, const ts_recipe_history *src) {
  ts_recipe_history copy = {0};
  for (size_t i = 0; i < src->undo_count; ++i) {
    if (!ts_owned_recipe_copy(&copy.undo[i], &src->undo[i].value)) goto fail;
    copy.undo_count++;
  }
  for (size_t i = 0; i < src->redo_count; ++i) {
    if (!ts_owned_recipe_copy(&copy.redo[i], &src->redo[i].value)) goto fail;
    copy.redo_count++;
  }
  ts_recipe_history_destroy(dst); *dst = copy; return true;
fail:
  ts_recipe_history_destroy(&copy); return false;
}

static bool accept_working_state(ts_app_state *a) {
  ts_owned_recipe recipe = {0}; ts_recipe_history history = {0};
  if (!ts_owned_recipe_copy(&recipe, &a->bank[a->selected].recipe) ||
      !history_copy(&history, &a->history)) {
    ts_owned_recipe_destroy(&recipe); ts_recipe_history_destroy(&history);
    return false;
  }
  ts_owned_recipe_destroy(&a->accepted); a->accepted = recipe;
  ts_recipe_history_destroy(&a->accepted_history); a->accepted_history = history;
  a->has_accepted = true; a->accepted_session_identity = a->session_identity;
  a->rejected_edit = false; return true;
}

static bool restore_accepted_state(ts_app_state *a) {
  ts_owned_recipe recipe = {0}; ts_recipe_history history = {0};
  if (!a->has_accepted || a->accepted_session_identity != a->session_identity ||
      !ts_owned_recipe_copy(&recipe, &a->accepted.value) ||
      !history_copy(&history, &a->accepted_history)) {
    ts_owned_recipe_destroy(&recipe); ts_recipe_history_destroy(&history);
    return false;
  }
  free((void *)a->bank[a->selected].recipe.name);
  a->bank[a->selected].recipe = recipe.value; recipe.name = NULL;
  ts_recipe_history_destroy(&a->history); a->history = history;
  return true;
}

bool ts_cli_parse(int argc, char **argv, ts_cli_options *o, char *error,
                  size_t cap) {
  if (!o)
    return false;
  memset(o, 0, sizeof(*o));
  o->palette_name = "default";
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--smoke-test") == 0)
      o->smoke_test = true;
    else if (strcmp(a, "--help") == 0)
      o->help = true;
    else if (strcmp(a, "--recipe") == 0 || strcmp(a, "--palette-file") == 0 ||
             strcmp(a, "--palette") == 0 || strcmp(a, "--resource-dir") == 0) {
      if (++i >= argc) {
        snprintf(error, cap, "missing value after %s", a);
        return false;
      }
      if (strcmp(a, "--recipe") == 0)
        o->recipe_path = argv[i];
      else if (strcmp(a, "--palette-file") == 0)
        o->palette_file = argv[i];
      else if (strcmp(a, "--palette") == 0)
        o->palette_name = argv[i];
      else
        o->resource_dir = argv[i];
    } else {
      snprintf(error, cap, "unknown option: %s", a);
      return false;
    }
  }
  if (strcmp(o->palette_name, "default") != 0 &&
      strcmp(o->palette_name, "dark") != 0) {
    snprintf(error, cap, "unknown built-in palette: %s", o->palette_name);
    return false;
  }
  return true;
}

static bool has_factory(const char *d) {
  char p[1024];
  for (int i = 0; i < TS_FACTORY_RECIPES; i++) {
    snprintf(p, sizeof p, "%s/%s", d, factory_names[i]);
    FILE *f = fopen(p, "rb");
    if (!f)
      return false;
    fclose(f);
  }
  return true;
}
bool ts_app_find_factory(const char *override, const char *exe, char *out,
                         size_t cap, char *error, size_t ecap) {
  const char *candidates[3] = {override, NULL, TS_FACTORY_SOURCE_DIR};
  char beside[1024] = {0};
  if (exe) {
    snprintf(beside, sizeof beside, "%s", exe);
    char *s1 = strrchr(beside, '/');
    char *s2 = strrchr(beside, '\\');
    char *s = s1;
    if (s2 != NULL && (s1 == NULL || s2 > s1))
      s = s2;
    if (s)
      *s = '\0';
    else
      strcpy(beside, ".");
    strncat(beside, "/resources/recipes", sizeof(beside) - strlen(beside) - 1);
    candidates[1] = beside;
  }
  for (int i = 0; i < 3; i++)
    if (candidates[i] && has_factory(candidates[i])) {
      if (strlen(candidates[i]) + 1 > cap)
        break;
      strcpy(out, candidates[i]);
      return true;
    }
  snprintf(error, ecap, "factory recipes not found; use --resource-dir PATH");
  return false;
}

bool ts_app_load_bank(ts_app_state *a, const char *d, const char *extra,
                      char *error, size_t cap) {
  if (!a || !d)
    return false;
  memset(a, 0, sizeof(*a));
  a->base_octave = 3;
  a->mouse_note = -1;
  a->mode = TS_AUDITION_ONE_SHOT;
  a->page = TS_PAGE_SOURCE;
  a->focused_parameter = TS_P_SOURCE;
  a->session_identity = 1;
  ts_preview_pool_init(&a->previews);
  if (!ts_render_worker_create(&a->render_worker)) {
    snprintf(error, cap, "cannot start render worker");
    ts_app_dispose(a);
    return false;
  }
  for (int i = 0; i < TS_FACTORY_RECIPES; i++) {
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", d, factory_names[i]);
    ts_io_error e;
    if (ts_recipe_load_file(p, &a->bank[i].recipe, &e) != TS_IO_OK) {
      snprintf(error, cap, "%s: %s", factory_names[i], e.message);
      ts_app_dispose(a);
      return false;
    }
    a->bank[i].loaded = true;
    a->bank_count++;
  }
  if (!ts_owned_recipe_copy(&a->baseline, &a->bank[a->selected].recipe)) {
    snprintf(error, cap, "cannot establish session baseline");
    ts_app_dispose(a); return false;
  }
  a->has_baseline = true;
  if (!accept_working_state(a)) {
    snprintf(error, cap, "cannot establish accepted render state");
    ts_app_dispose(a); return false;
  }
  if (extra) {
    ts_io_error e;
    if (ts_recipe_load_file(extra, &a->bank[a->bank_count].recipe, &e) !=
        TS_IO_OK)
      snprintf(a->status, sizeof a->status, "EXTERNAL RECIPE ERROR: %s",
               e.message);
    else {
      a->bank[a->bank_count].loaded = true;
      a->bank_count++;
    }
  }
  return true;
}
bool ts_app_ensure_rendered(ts_app_state *a, size_t i, char *error,
                            size_t cap) {
  if (!a || i >= a->bank_count)
    return false;
  ts_app_bank_entry *e = &a->bank[i];
  if (e->rendered)
    return true;
  ts_render_report report;
  if (!ts_render(&e->recipe, &e->render, &report)) {
    snprintf(error, cap, "render failed: %s", e->recipe.name);
    return false;
  }
  e->source.samples = e->render.samples;
  e->source.frame_count = e->render.frame_count;
  e->source.sample_rate = e->render.sample_rate;
  e->source.root_midi_note = e->recipe.root_midi_note;
  e->rendered = true;
  return true;
}

int ts_app_key_note(int key, int octave) {
  const char *low = "ZSXDCVGBHNJM", *high = "Q2W3ER5T6Y7U";
  key = (key >= 'a' && key <= 'z') ? key - 32 : key;
  const char *p = strchr(low, key);
  int n;
  if (p)
    n = 12 * (octave + 1) + (int)(p - low);
  else if ((p = strchr(high, key)))
    n = 12 * (octave + 2) + (int)(p - high);
  else
    return -1;
  return n >= 0 && n <= 127 ? n : -1;
}
bool ts_app_key_press(ts_app_state *a, int key, bool repeat, int *note) {
  if (note)
    *note = -1;
  if (!a || repeat)
    return false;
  int n = ts_app_key_note(key, a->base_octave);
  if (n < 0 || a->key_down[n])
    return false;
  a->key_down[n] = true;
  if (note)
    *note = n;
  return true;
}
void ts_app_key_release(ts_app_state *a, int key, int *note) {
  if (note)
    *note = -1;
  if (!a)
    return;
  int n = ts_app_key_note(key, a->base_octave);
  if (n >= 0) {
    a->key_down[n] = false;
    if (note)
      *note = n;
  }
}

bool ts_app_toggle_mode(ts_app_state *a, const bool repeat) {
  if (a == NULL || repeat)
    return false;
  a->mode = a->mode == TS_AUDITION_ONE_SHOT ? TS_AUDITION_GATED
                                            : TS_AUDITION_ONE_SHOT;
  return true;
}

static ts_app_mouse_result empty_mouse_result(void) {
  ts_app_mouse_result result = {-1, -1, -1};
  return result;
}

static void release_mouse_note(ts_app_state *a, ts_app_mouse_result *result) {
  if (a->mouse_note < 0)
    return;
  a->key_down[a->mouse_note] = false;
  if (a->mode == TS_AUDITION_GATED)
    result->note_off = a->mouse_note;
  a->mouse_note = -1;
}

ts_app_mouse_result ts_app_mouse_press(ts_app_state *a, const int x,
                                       const int y) {
  ts_app_mouse_result result = empty_mouse_result();
  if (a == NULL)
    return result;
  release_mouse_note(a, &result);
  const int recipe = ts_ui_recipe_hit(x, y, a->bank_count);
  if (recipe >= 0) {
    if (ts_app_select_recipe(a, (size_t)recipe)) result.selected_recipe = recipe;
    return result;
  }
  const int note = ts_ui_keyboard_hit(x, y, a->base_octave);
  if (note >= 0) {
    a->mouse_note = note;
    a->key_down[note] = true;
    result.note_on = note;
  }
  return result;
}

ts_app_mouse_result ts_app_mouse_move(ts_app_state *a, const int x,
                                      const int y) {
  ts_app_mouse_result result = empty_mouse_result();
  if (a == NULL || a->mouse_note < 0)
    return result;
  const int note = ts_ui_keyboard_hit(x, y, a->base_octave);
  if (note == a->mouse_note)
    return result;
  release_mouse_note(a, &result);
  if (note >= 0) {
    a->mouse_note = note;
    a->key_down[note] = true;
    result.note_on = note;
  }
  return result;
}

ts_app_mouse_result ts_app_mouse_release(ts_app_state *a) {
  ts_app_mouse_result result = empty_mouse_result();
  if (a != NULL)
    release_mouse_note(a, &result);
  return result;
}

ts_app_mouse_result ts_app_focus_lost(ts_app_state *a) {
  return ts_app_mouse_release(a);
}

bool ts_app_update_overload(ts_app_state *a, const uint32_t generation,
                            const uint64_t now_ms) {
  if (a == NULL)
    return false;
  if (generation != a->overload_generation) {
    a->overload_generation = generation;
    a->overload_last_ms = now_ms;
    a->overload_visible = true;
  } else if (a->overload_visible && (now_ms < a->overload_last_ms ||
                                     now_ms - a->overload_last_ms > 750)) {
    a->overload_visible = false;
  }
  return a->overload_visible;
}

bool ts_app_set_page(ts_app_state *a, ts_parameter_page page) {
  if (!a || page >= TS_EDITOR_PAGE_COUNT) return false;
  a->page = page;
  size_t count; const ts_parameter_desc *all=ts_parameter_descriptors(&count);
  for(size_t i=0;i<count;i++) if(all[i].page==page &&
      ts_parameter_enabled(all[i].id,&a->bank[a->selected].recipe)) {
    a->focused_parameter=(int)all[i].id; return true;
  }
  return false;
}
bool ts_app_page_move(ts_app_state *a,int direction){if(!a||direction==0)return false;int page=((int)a->page+(direction>0?1:5))%(int)TS_EDITOR_PAGE_COUNT;return ts_app_set_page(a,(ts_parameter_page)page);}
bool ts_app_focus_move(ts_app_state *a,int direction) {
  if(!a||direction==0)return false;
  size_t count; const ts_parameter_desc *all=ts_parameter_descriptors(&count);
  int at=a->focused_parameter;
  for(size_t attempt=0;attempt<count;attempt++){at=(at+direction+(int)count)%(int)count;
    if(all[at].page==a->page&&ts_parameter_enabled(all[at].id,&a->bank[a->selected].recipe)){a->focused_parameter=at;return true;}}
  return false;
}
bool ts_app_adjust_parameter(ts_app_state*a,ts_parameter_id id,double steps,bool commit) {
  if(!a||id>=TS_PARAMETER_COUNT||!ts_parameter_enabled(id,&a->bank[a->selected].recipe))return false;
  const ts_parameter_desc*d=ts_parameter_by_id(id);double value;if(!ts_parameter_get_number(id,&a->bank[a->selected].recipe,&value))return false;
  if(commit&&!ts_recipe_history_commit(&a->history,&a->bank[a->selected].recipe))return false;
  double next=value+steps*d->fine_step;if(d->type==TS_PARAM_BOOLEAN)next=value==0?1:0;
  if(d->type==TS_PARAM_ENUM){next=fmod(value+steps+(double)d->enum_count,(double)d->enum_count);}
  if(next<d->minimum)next=d->minimum;
  if(next>d->maximum)next=d->maximum;
  if(!ts_parameter_set_number(id,&a->bank[a->selected].recipe,next)){if(commit&&a->history.undo_count)ts_owned_recipe_destroy(&a->history.undo[--a->history.undo_count]);return false;}
  return ts_app_request_render(a);
}
static bool restore_history(ts_app_state*a,bool redo){ts_owned_recipe working={0};if(!ts_owned_recipe_copy(&working,&a->bank[a->selected].recipe))return false;
  bool ok=redo?ts_recipe_history_redo(&a->history,&working):ts_recipe_history_undo(&a->history,&working);if(!ok){ts_owned_recipe_destroy(&working);return false;}
  free((void*)a->bank[a->selected].recipe.name);a->bank[a->selected].recipe=working.value;working.name=NULL;return ts_app_request_render(a);}
bool ts_app_undo(ts_app_state*a){return a&&restore_history(a,false);} bool ts_app_redo(ts_app_state*a){return a&&restore_history(a,true);}
bool ts_app_commit_parent(ts_app_state*a){if(!a||a->rejected_edit)return false;ts_owned_recipe_destroy(&a->parent);a->has_parent=ts_owned_recipe_copy(&a->parent,&a->bank[a->selected].recipe);return a->has_parent;}
bool ts_app_update_parent(ts_app_state*a,bool confirmed){return confirmed&&ts_app_commit_parent(a);}
bool ts_app_dirty(const ts_app_state*a){return a&&(!a->has_saved||!ts_recipe_fields_equal(&a->saved.value,&a->bank[a->selected].recipe));}
bool ts_app_request_render(ts_app_state*a){if(!a||!a->render_worker)return false;uint64_t g=ts_render_worker_request(a->render_worker,&a->bank[a->selected].recipe);if(!g)return false;a->working_generation=g;a->failed_generation=0;a->rejected_edit=false;a->render_error[0]=0;return true;}
bool ts_app_poll_render(ts_app_state*a){if(!a||!a->render_worker)return false;bool completed=false;ts_preview*p=ts_render_worker_take(a->render_worker);if(p){completed=true;if(p->generation==a->working_generation&&accept_working_state(a)){ts_preview_pool_publish(&a->previews,p);a->published_generation=p->generation;a->preview_session_identity=a->session_identity;p=NULL;}if(p){atomic_store(&p->retired,true);ts_preview_pool tmp;ts_preview_pool_init(&tmp);p->next=tmp.all;tmp.all=p;tmp.retained=1;ts_preview_pool_collect(&tmp);}}ts_preview_pool_collect(&a->previews);uint64_t failed=0;char worker_error[128]={0};if(ts_render_worker_failed(a->render_worker,&failed,worker_error,sizeof worker_error)&&failed==a->working_generation){a->failed_generation=failed;if(restore_accepted_state(a)){a->working_generation=a->published_generation;a->rejected_edit=true;snprintf(a->render_error,sizeof a->render_error,"REJECTED: %.108s",worker_error);}else snprintf(a->render_error,sizeof a->render_error,"RENDER FAILED: %.107s",worker_error);}return completed;}
bool ts_app_rendering(ts_app_state*a){return a&&a->render_worker&&a->working_generation!=a->published_generation&&!ts_app_render_failed(a)&&ts_render_worker_rendering(a->render_worker);}
bool ts_app_render_failed(ts_app_state*a){if(!a||!a->render_worker)return false;if(a->rejected_edit)return true;uint64_t failed=0;if(ts_render_worker_failed(a->render_worker,&failed,a->render_error,sizeof a->render_error))a->failed_generation=failed;return a->failed_generation==a->working_generation&&failed!=0;}
bool ts_app_render_matched(const ts_app_state*a){return a&&!a->rejected_edit&&a->previews.current&&a->published_generation==a->working_generation&&a->failed_generation!=a->working_generation;}
const ts_audition_source *ts_app_preview_source(const ts_app_state*a){return a&&a->previews.current&&a->preview_session_identity==a->session_identity?&a->previews.current->source:NULL;}
bool ts_app_set_parameter_text(ts_app_state*a,ts_parameter_id id,const char*text,char*error,size_t cap){if(!a||!text||id>=TS_PARAMETER_COUNT)return false;ts_recipe*r=&a->bank[a->selected].recipe;if(id==TS_P_NAME){size_t n=strlen(text);if(!n||n>TS_RECIPE_NAME_MAX_BYTES||!ts_utf8_valid(text,n)){snprintf(error,cap,"name must be valid UTF-8, 1..127 bytes");return false;}if(!ts_recipe_history_commit(&a->history,r))return false;char*name=malloc(n+1);if(!name){ts_owned_recipe_destroy(&a->history.undo[--a->history.undo_count]);return false;}memcpy(name,text,n+1);free((void*)r->name);r->name=name;return ts_app_request_render(a);}ts_recipe candidate=*r;if(!ts_parameter_parse(id,&candidate,text)){snprintf(error,cap,"invalid or out-of-range value");return false;}if(ts_recipe_fields_equal(&candidate,r))return true;if(!ts_recipe_history_commit(&a->history,r))return false;*r=candidate;return ts_app_request_render(a);}
bool ts_app_save_recipe_confirmed(ts_app_state*a,const char*path,bool replace,ts_io_error*error){if(!a||a->rejected_edit||!path||strlen(path)>TS_PATH_MAX_BYTES)return false;ts_recipe*r=&a->bank[a->selected].recipe;ts_io_status status=replace?ts_recipe_replace_file(path,r,error):ts_recipe_save_file(path,r,error);if(status!=TS_IO_OK)return false;ts_owned_recipe copy={0};if(!ts_owned_recipe_copy(&copy,r))return false;ts_owned_recipe_destroy(&a->saved);a->saved=copy;a->has_saved=true;snprintf(a->saved_path,sizeof a->saved_path,"%s",path);return true;}
bool ts_app_save_recipe(ts_app_state*a,const char*path,ts_io_error*error){return ts_app_save_recipe_confirmed(a,path,false,error);}
bool ts_app_load_recipe(ts_app_state*a,const char*path,ts_io_error*error){if(!a||!path||strlen(path)>TS_PATH_MAX_BYTES)return false;ts_recipe candidate;if(ts_recipe_load_file(path,&candidate,error)!=TS_IO_OK)return false;ts_rendered_sample rendered={0};ts_render_report report;if(!ts_render(&candidate,&rendered,&report)){ts_recipe_loaded_dispose(&candidate);if(error){error->status=TS_IO_INVALID_VALUE;snprintf(error->message,sizeof error->message,"candidate render failed");}return false;}ts_rendered_sample_free(&rendered);ts_owned_recipe saved={0},baseline={0};if(!ts_owned_recipe_copy(&saved,&candidate)||!ts_owned_recipe_copy(&baseline,&candidate)){ts_owned_recipe_destroy(&saved);ts_owned_recipe_destroy(&baseline);ts_recipe_loaded_dispose(&candidate);return false;}uint64_t generation=ts_render_worker_request(a->render_worker,&candidate);if(!generation){ts_owned_recipe_destroy(&saved);ts_owned_recipe_destroy(&baseline);ts_recipe_loaded_dispose(&candidate);return false;}ts_recipe_loaded_dispose(&a->bank[a->selected].recipe);a->bank[a->selected].recipe=candidate;a->bank[a->selected].loaded=true;ts_recipe_history_destroy(&a->history);ts_owned_recipe_destroy(&a->parent);a->has_parent=false;a->has_baked=false;ts_owned_recipe_destroy(&a->saved);a->saved=saved;a->has_saved=true;ts_owned_recipe_destroy(&a->baseline);a->baseline=baseline;a->has_baseline=true;snprintf(a->saved_path,sizeof a->saved_path,"%s",path);a->session_identity++;a->working_generation=generation;a->failed_generation=0;a->render_error[0]=0;if(!accept_working_state(a))return false;return true;}
bool ts_app_bake_confirmed(ts_app_state*a,const char*recipe_path,const char*wav_path,bool replace,ts_io_error*error){if(!a||!recipe_path||!wav_path||strlen(recipe_path)>TS_PATH_MAX_BYTES||strlen(wav_path)>TS_PATH_MAX_BYTES||!ts_app_render_matched(a))return false;ts_preview*p=a->previews.current;ts_recipe*r=&a->bank[a->selected].recipe;if(p->recipe_identity!=ts_recipe_identity(r))return false;ts_io_status status=replace?ts_bake_pair_replace_files(recipe_path,wav_path,r,&p->render,error):ts_bake_pair_files(recipe_path,wav_path,r,&p->render,error);if(status!=TS_IO_OK)return false;a->has_baked=true;a->baked_recipe_identity=p->recipe_identity;a->baked_pcm_identity=p->pcm_identity;snprintf(a->baked_recipe_path,sizeof a->baked_recipe_path,"%s",recipe_path);snprintf(a->baked_wav_path,sizeof a->baked_wav_path,"%s",wav_path);return true;}
bool ts_app_bake(ts_app_state*a,const char*recipe_path,const char*wav_path,ts_io_error*error){return ts_app_bake_confirmed(a,recipe_path,wav_path,false,error);}
bool ts_app_baked(const ts_app_state*a){return a&&a->has_baked&&ts_app_render_matched(a)&&a->previews.current->recipe_identity==a->baked_recipe_identity&&a->previews.current->pcm_identity==a->baked_pcm_identity&&ts_recipe_identity(&a->bank[a->selected].recipe)==a->baked_recipe_identity;}
bool ts_app_session_requires_discard(const ts_app_state*a){return a&&(a->has_parent||!a->has_baseline||!ts_recipe_fields_equal(&a->baseline.value,&a->bank[a->selected].recipe));}
bool ts_app_select_recipe_confirmed(ts_app_state*a,size_t index,bool confirmed){if(!a||index>=a->bank_count)return false;if(index==a->selected)return true;if(ts_app_session_requires_discard(a)&&!confirmed)return false;ts_rendered_sample candidate={0};ts_render_report report;if(!ts_render(&a->bank[index].recipe,&candidate,&report))return false;ts_rendered_sample_free(&candidate);ts_owned_recipe baseline={0};if(!ts_owned_recipe_copy(&baseline,&a->bank[index].recipe))return false;uint64_t generation=ts_render_worker_request(a->render_worker,&a->bank[index].recipe);if(!generation){ts_owned_recipe_destroy(&baseline);return false;}a->selected=index;a->session_identity++;a->working_generation=generation;a->failed_generation=0;a->render_error[0]=0;ts_recipe_history_destroy(&a->history);ts_owned_recipe_destroy(&a->parent);a->has_parent=false;a->has_baked=false;ts_owned_recipe_destroy(&a->saved);a->has_saved=false;a->saved_path[0]=0;ts_owned_recipe_destroy(&a->baseline);a->baseline=baseline;a->has_baseline=true;if(!accept_working_state(a))return false;return true;}
bool ts_app_select_recipe(ts_app_state*a,size_t index){return ts_app_select_recipe_confirmed(a,index,true);}
void ts_app_dispose(ts_app_state *a) {
  if (!a)
    return;
  ts_render_worker_destroy(a->render_worker);a->render_worker=NULL;
  ts_preview_pool_destroy(&a->previews);
  for (size_t i = 0; i < a->bank_count; i++) {
    ts_rendered_sample_free(&a->bank[i].render);
    if (a->bank[i].loaded)
      ts_recipe_loaded_dispose(&a->bank[i].recipe);
  }
  ts_recipe_history_destroy(&a->history);ts_recipe_history_destroy(&a->accepted_history);ts_owned_recipe_destroy(&a->saved);ts_owned_recipe_destroy(&a->parent);ts_owned_recipe_destroy(&a->baseline);ts_owned_recipe_destroy(&a->accepted);
  memset(a, 0, sizeof(*a));
}
