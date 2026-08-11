#include "tapesister/ts_app.h"
#include "tapesister/ts_presentation.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct audio_context {
  ts_audition_mixer mixer;
} audio_context;
static void audio_callback(void *userdata, Uint8 *stream, int length) {
  audio_context *c = userdata;
  ts_audition_mix(&c->mixer, (float *)stream,
                  (size_t)length / (sizeof(float) * 2U));
}
static int ascii_key(SDL_Keycode k) {
  if (k >= SDLK_a && k <= SDLK_z)
    return 'A' + (int)(k - SDLK_a);
  if (k >= SDLK_0 && k <= SDLK_9)
    return '0' + (int)(k - SDLK_0);
  return (int)k;
}
static bool update_presentation(SDL_Window *window, SDL_Renderer *renderer,
                                ts_present_rect *presentation, int *window_w,
                                int *window_h, int *output_w, int *output_h) {
  SDL_GetWindowSize(window, window_w, window_h);
  if (SDL_GetRendererOutputSize(renderer, output_w, output_h) != 0)
    return false;
  return ts_present_fit(*output_w, *output_h, presentation);
}

static bool mouse_to_logical(SDL_Window *window, SDL_Renderer *renderer,
                             ts_present_rect *presentation, int window_x,
                             int window_y, int *logical_x, int *logical_y) {
  int window_w, window_h, output_w, output_h;
  if (!update_presentation(window, renderer, presentation, &window_w, &window_h,
                           &output_w, &output_h))
    return false;
  return ts_present_window_to_logical(presentation, window_w, window_h,
                                      output_w, output_h, window_x, window_y,
                                      logical_x, logical_y);
}
static void lock_note(SDL_AudioDeviceID dev, audio_context *a,
                      const ts_audition_source *s, int note) {
  if (dev)
    SDL_LockAudioDevice(dev);
  ts_audition_note_on(&a->mixer, s, (uint8_t)note);
  if (dev)
    SDL_UnlockAudioDevice(dev);
}
static void lock_off(SDL_AudioDeviceID dev, audio_context *a, int note) {
  if (dev)
    SDL_LockAudioDevice(dev);
  ts_audition_note_off(&a->mixer, (uint8_t)note);
  if (dev)
    SDL_UnlockAudioDevice(dev);
}
static void apply_mouse_result(SDL_AudioDeviceID device, audio_context *audio,
                               ts_app_state *app,
                               const ts_app_mouse_result result, char *error,
                               size_t error_capacity) {
  if (result.selected_recipe >= 0)
  {
    ts_app_ensure_rendered(app, (size_t)result.selected_recipe, error,
                           error_capacity);
    ts_app_request_render(app);
  }
  if (result.note_off >= 0)
    lock_off(device, audio, result.note_off);
  if (result.note_on >= 0)
    lock_note(device, audio, ts_app_preview_source(app) ? ts_app_preview_source(app) : &app->bank[app->selected].source, result.note_on);
}

int main(int argc, char **argv) {
  ts_cli_options options;
  char error[256] = {0};
  if (!ts_cli_parse(argc, argv, &options, error, sizeof error)) {
    fprintf(stderr, "TapeSister: %s\n", error);
    return 2;
  }
  if (options.help) {
    puts("tapesister [--recipe PATH] [--palette-file PATH] [--palette "
         "default|dark] [--resource-dir PATH] [--smoke-test]");
    return 0;
  }
  char factory[1024];
  if (!ts_app_find_factory(options.resource_dir, argv[0], factory,
                           sizeof factory, error, sizeof error)) {
    fprintf(stderr, "TapeSister: %s\n", error);
    return 3;
  }
  ts_app_state app;
  if (!ts_app_load_bank(&app, factory, options.recipe_path, error,
                        sizeof error)) {
    fprintf(stderr, "TapeSister: %s\n", error);
    return 4;
  }
  char startup_message[192];
  snprintf(startup_message, sizeof startup_message, "%s", app.status);
  if (!ts_app_ensure_rendered(&app, app.selected, error, sizeof error)) {
    fprintf(stderr, "TapeSister: %s\n", error);
    ts_app_dispose(&app);
    return 5;
  }
  ts_app_request_render(&app);
  if (options.smoke_test)
    for (size_t i = 0; i < app.bank_count; i++)
      if (!ts_app_ensure_rendered(&app, i, error, sizeof error)) {
        fprintf(stderr, "TapeSister: %s\n", error);
        ts_app_dispose(&app);
        return 5;
      }
  ts_palette palette;
  ts_palette_builtin(&palette, options.palette_name);
  if (options.palette_file &&
      !ts_palette_load_file(options.palette_file, &palette, error,
                            sizeof error))
    snprintf(startup_message, sizeof startup_message,
             "PALETTE FALLBACK: %.150s", error);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    fprintf(stderr, "TapeSister SDL: %s\n", SDL_GetError());
    ts_app_dispose(&app);
    return 6;
  }
  const bool audio_subsystem = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
  Uint32 flags = SDL_WINDOW_RESIZABLE |
                 (options.smoke_test ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
  SDL_Window *window =
      SDL_CreateWindow("TapeSister", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1264, 800, flags);
  if (!window) {
    fprintf(stderr, "TapeSister window: %s\n", SDL_GetError());
    SDL_Quit();
    ts_app_dispose(&app);
    return 7;
  }
  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_Texture *texture =
      renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, TS_SCREEN_WIDTH,
                                   TS_SCREEN_HEIGHT)
               : NULL;
  ts_framebuffer *fb = calloc(1, sizeof(*fb));
  uint32_t *rgba = malloc(TS_SCREEN_WIDTH * TS_SCREEN_HEIGHT * sizeof(*rgba));
  if (!renderer || !texture || !fb || !rgba) {
    fprintf(stderr, "TapeSister UI allocation failed\n");
    free(fb);
    free(rgba);
    if (texture)
      SDL_DestroyTexture(texture);
    if (renderer)
      SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    ts_app_dispose(&app);
    return 8;
  }
  audio_context audio;
  ts_audition_init(&audio.mixer, 48000);
  SDL_AudioSpec want = {0}, have = {0};
  want.freq = 48000;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 512;
  want.callback = audio_callback;
  want.userdata = &audio;
  SDL_AudioDeviceID device =
      audio_subsystem ? SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                            SDL_AUDIO_ALLOW_FREQUENCY_CHANGE)
                      : 0;
  if (device && have.format == AUDIO_F32SYS && have.channels == 2) {
    ts_audition_init(&audio.mixer, (uint32_t)have.freq);
    SDL_PauseAudioDevice(device, 0);
    const char *driver = SDL_GetCurrentAudioDriver();
    snprintf(app.status, sizeof app.status, "AUDIO %.100s %d HZ FLOAT STEREO",
             driver ? driver : "UNKNOWN", have.freq);
  } else {
    if (device) {
      SDL_CloseAudioDevice(device);
      device = 0;
    }
    snprintf(app.status, sizeof app.status, "AUDIO UNAVAILABLE: %.150s",
             SDL_GetError());
  }
  if (options.smoke_test) {
    float smoke_audio[128];
    if (device)
      SDL_LockAudioDevice(device);
    ts_audition_note_on(&audio.mixer, &app.bank[0].source,
                        app.bank[0].recipe.root_midi_note);
    for (int i = 0; i < 3; i++)
      ts_audition_mix(&audio.mixer, smoke_audio, 64);
    if (device)
      SDL_UnlockAudioDevice(device);
  }
  bool running = true;
  int result = 0;
  ts_present_rect presentation = {0};
  unsigned frames = 0;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = false;
      else if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        const SDL_Keymod modifiers = (SDL_Keymod)e.key.keysym.mod;
        if (k == SDLK_ESCAPE)
          running = false;
        else if(k==SDLK_TAB){ts_app_focus_move(&app,(modifiers&KMOD_SHIFT)?-1:1);}
        else if(k==SDLK_PAGEUP&&!e.key.repeat){ts_app_set_page(&app,(ts_parameter_page)((app.page+5)%6));}
        else if(k==SDLK_PAGEDOWN&&!e.key.repeat){ts_app_set_page(&app,(ts_parameter_page)((app.page+1)%6));}
        else if(k==SDLK_UP){ts_app_focus_move(&app,-1);}
        else if(k==SDLK_DOWN){ts_app_focus_move(&app,1);}
        else if((k==SDLK_LEFT||k==SDLK_RIGHT)&&app.focused_parameter>=0){double step=k==SDLK_RIGHT?1:-1;if(modifiers&KMOD_SHIFT)step*=10;if(modifiers&KMOD_CTRL)step*=.1;ts_app_adjust_parameter(&app,(ts_parameter_id)app.focused_parameter,step,true);}
        else if((modifiers&KMOD_CTRL)&&k==SDLK_z&&!e.key.repeat){if(modifiers&KMOD_SHIFT)ts_app_redo(&app);else ts_app_undo(&app);}
        else if((modifiers&KMOD_CTRL)&&k==SDLK_y&&!e.key.repeat){ts_app_redo(&app);}
        else if((modifiers&KMOD_CTRL)&&k==SDLK_p&&!e.key.repeat){if(modifiers&KMOD_SHIFT)ts_app_update_parent(&app,true);else ts_app_commit_parent(&app);}
        else if (k == SDLK_F1 && app.selected > 0) {
          app.selected--;
          ts_app_ensure_rendered(&app, app.selected, error, sizeof error);
          ts_app_request_render(&app);
        } else if (k == SDLK_F2 && app.selected + 1 < app.bank_count) {
          app.selected++;
          ts_app_ensure_rendered(&app, app.selected, error, sizeof error);
          ts_app_request_render(&app);
        } else if (k == SDLK_LEFTBRACKET && app.base_octave > -1)
          app.base_octave--;
        else if (k == SDLK_RIGHTBRACKET && app.base_octave < 9)
          app.base_octave++;
        else if (k == SDLK_g && (modifiers & KMOD_CTRL) != 0 &&
                 ts_app_toggle_mode(&app, e.key.repeat != 0)) {
          if (device)
            SDL_LockAudioDevice(device);
          audio.mixer.mode = app.mode;
          if (device)
            SDL_UnlockAudioDevice(device);
        } else if (k == SDLK_SPACE) {
          if (device)
            SDL_LockAudioDevice(device);
          ts_audition_stop_all(&audio.mixer);
          if (device)
            SDL_UnlockAudioDevice(device);
        } else if (k == SDLK_RETURN && !e.key.repeat)
          lock_note(device, &audio, ts_app_preview_source(&app) ? ts_app_preview_source(&app) : &app.bank[app.selected].source,
                    app.bank[app.selected].recipe.root_midi_note);
        else if ((modifiers & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) == 0) {
          int note;
          if (ts_app_key_press(&app, ascii_key(k), e.key.repeat != 0, &note))
            lock_note(device, &audio, &app.bank[app.selected].source, note);
        }
      } else if (e.type == SDL_KEYUP) {
        int note;
        ts_app_key_release(&app, ascii_key(e.key.keysym.sym), &note);
        if (note >= 0)
          lock_off(device, &audio, note);
      } else if (e.type == SDL_MOUSEBUTTONDOWN &&
                 e.button.button == SDL_BUTTON_LEFT) {
        int lx, ly;
        if (mouse_to_logical(window, renderer, &presentation, e.button.x,e.button.y,&lx,&ly)) {
          int tab=ts_ui_tab_hit(lx,ly),parameter=ts_ui_parameter_hit(lx,ly,app.page);
          if(tab>=0)ts_app_set_page(&app,(ts_parameter_page)tab);
          else if(parameter>=0){app.focused_parameter=parameter;double steps=1;if(lx<610&&lx>=578)steps=lx<594?-1:1;
            else if(lx>=416&&lx<=576){const ts_parameter_desc*d=ts_parameter_by_id((ts_parameter_id)parameter);double old;ts_parameter_get_number((ts_parameter_id)parameter,&app.bank[app.selected].recipe,&old);double target=ts_parameter_from_position(d,ts_ui_slider_position(lx));steps=(target-old)/d->fine_step;}
            ts_app_adjust_parameter(&app,(ts_parameter_id)parameter,steps,true);
          } else apply_mouse_result(device, &audio, &app,
                             ts_app_mouse_press(&app, lx, ly), error,
                             sizeof error);}
      } else if (e.type == SDL_MOUSEBUTTONUP &&
                 e.button.button == SDL_BUTTON_LEFT) {
        apply_mouse_result(device, &audio, &app, ts_app_mouse_release(&app),
                           error, sizeof error);
      } else if (e.type == SDL_MOUSEMOTION && app.mouse_note >= 0) {
        int lx, ly;
        ts_app_mouse_result mouse_result;
        if (mouse_to_logical(window, renderer, &presentation, e.motion.x,
                             e.motion.y, &lx, &ly))
          mouse_result = ts_app_mouse_move(&app, lx, ly);
        else
          mouse_result = ts_app_mouse_release(&app);
        apply_mouse_result(device, &audio, &app, mouse_result, error,
                           sizeof error);
      } else if (e.type == SDL_WINDOWEVENT &&
                 e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        apply_mouse_result(device, &audio, &app, ts_app_focus_lost(&app), error,
                           sizeof error);
      }
    }
    ts_app_poll_render(&app);
    ts_ui_model model = {0};
    model.recipe_count = app.bank_count;
    model.selected_recipe = app.selected;
    model.base_octave = app.base_octave;
    model.mode = app.mode;
    model.audio_status = app.status;
    model.message = startup_message[0] ? startup_message : NULL;
    if (device)
      SDL_LockAudioDevice(device);
    model.active_voices = ts_audition_active_voices(&audio.mixer);
    if (device)
      SDL_UnlockAudioDevice(device);
    model.overload = ts_app_update_overload(
        &app, ts_audition_overload_generation(&audio.mixer), SDL_GetTicks());
    model.page=app.page;model.focused_parameter=app.focused_parameter;model.dirty=ts_app_dirty(&app);
    model.parent_present=app.has_parent;model.parent_match=app.has_parent&&ts_recipe_fields_equal(&app.parent.value,&app.bank[app.selected].recipe);
    model.rendering=ts_app_rendering(&app);model.render_error=ts_app_render_failed(&app);model.playback_position=ts_audition_cursor_position(&audio.mixer);
    for (size_t i = 0; i < app.bank_count; i++) {
      model.recipes[i] = &app.bank[i].recipe;
      model.renders[i] = &app.bank[i].render;
    }
    if(app.previews.current)model.renders[app.selected]=&app.previews.current->render;
    memcpy(model.pressed, app.key_down, sizeof(model.pressed));
    ts_ui_draw(fb, &model);
    size_t nonblank = 0;
    for (size_t i = 0; i < TS_SCREEN_WIDTH * TS_SCREEN_HEIGHT; i++) {
      if (fb->pixels[i] >= TS_PALETTE_SIZE) {
        result = 9;
        running = false;
        break;
      }
      if (fb->pixels[i] != 0)
        nonblank++;
      rgba[i] = palette.rgba[fb->pixels[i]];
    }
    if (options.smoke_test && nonblank < 1000) {
      result = 9;
      running = false;
    }
    SDL_UpdateTexture(texture, NULL, rgba, TS_SCREEN_WIDTH * 4);
    int window_w, window_h, output_w, output_h;
    if (!update_presentation(window, renderer, &presentation, &window_w,
                             &window_h, &output_w, &output_h)) {
      result = 9;
      running = false;
    }
    SDL_Rect destination = {presentation.x, presentation.y, presentation.w,
                            presentation.h};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &destination);
    SDL_RenderPresent(renderer);
    if (options.smoke_test && ++frames >= 4)
      running = false;
    SDL_Delay(1);
  }
  if (device) {
    SDL_LockAudioDevice(device);
    ts_audition_discard_all(&audio.mixer);
    SDL_UnlockAudioDevice(device);
    SDL_CloseAudioDevice(device);
  }
  free(rgba);
  free(fb);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  ts_app_dispose(&app);
  return result;
}
