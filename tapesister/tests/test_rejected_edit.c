#include "tapesister/ts_app.h"
#include <stdio.h>
#include <threads.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static bool render_except_42(const ts_recipe *recipe, ts_rendered_sample *out,
    ts_render_report *report, void *unused) {
  (void)unused;
  if (recipe->seed == 42U) return false;
  return ts_render(recipe, out, report);
}

static bool wait_terminal(ts_app_state *app) {
  for (unsigned i = 0; i < 1000000U; ++i) {
    ts_app_poll_render(app);
    if (ts_app_render_matched(app) || ts_app_render_failed(app)) return true;
    thrd_yield();
  }
  return false;
}

int main(void) {
  ts_app_state app; char error[256] = {0};
  CHECK(ts_app_load_bank(&app, TS_FIXTURE_DIR, NULL, error, sizeof error));
  ts_render_worker_destroy(app.render_worker);
  CHECK(ts_render_worker_create_with_function(&app.render_worker,
      render_except_42, NULL));
  CHECK(ts_app_request_render(&app) && wait_terminal(&app));
  CHECK(ts_app_render_matched(&app));
  const uint64_t accepted = ts_recipe_identity(&app.bank[app.selected].recipe);
  ts_preview *preview = app.previews.current;
  const size_t undo_before = app.history.undo_count;

  CHECK(ts_app_set_parameter_text(&app, TS_P_SEED, "42", error, sizeof error));
  CHECK(wait_terminal(&app));
  CHECK(ts_app_render_failed(&app));
  CHECK(ts_recipe_identity(&app.bank[app.selected].recipe) == accepted);
  CHECK(app.history.undo_count == undo_before);
  CHECK(app.previews.current == preview);
  CHECK(ts_app_preview_source(&app) == &preview->source);
  CHECK(!ts_app_render_matched(&app));
  CHECK(!ts_app_save_recipe_confirmed(&app, "/tmp/rejected.tsr", false, NULL));
  CHECK(!ts_app_commit_parent(&app));

  CHECK(ts_app_set_parameter_text(&app, TS_P_SEED, "43", error, sizeof error));
  CHECK(wait_terminal(&app) && ts_app_render_matched(&app));
  CHECK(app.bank[app.selected].recipe.seed == 43U);
  CHECK(app.history.undo_count == undo_before + 1U);

  /* A superseded failing generation cannot roll back the newer valid edit. */
  CHECK(ts_app_set_parameter_text(&app, TS_P_SEED, "42", error, sizeof error));
  CHECK(ts_app_set_parameter_text(&app, TS_P_SEED, "44", error, sizeof error));
  CHECK(wait_terminal(&app) && ts_app_render_matched(&app));
  CHECK(app.bank[app.selected].recipe.seed == 44U);
  ts_app_dispose(&app);
  puts("rejected edit transaction tests passed");
  return 0;
}
