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
  }
  if (result.note_off >= 0)
    lock_off(device, audio, result.note_off);
  if (result.note_on >= 0)
    lock_note(device, audio, ts_app_preview_source(app), result.note_on);
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
  enum { MODAL_NONE, MODAL_SAVE, MODAL_LOAD, MODAL_BAKE, MODAL_PARENT, MODAL_PARAMETER, MODAL_SELECT } modal=MODAL_NONE;
  ts_text_edit modal_edit={0}; char modal_error[160]={0};
  ts_file_browser browser={0};bool browser_open=false;char remembered_directory[TS_PATH_MAX_BYTES+1U]=".";
  bool overwrite_confirm=false;
  bool discard_confirm=false;
  size_t pending_selection=0;
  ts_parameter_id modal_parameter=TS_P_SOURCE; int drag_parameter=-1;ts_owned_recipe drag_before={0};
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
        if(drag_parameter>=0&&k==SDLK_ESCAPE){free((void*)app.bank[app.selected].recipe.name);app.bank[app.selected].recipe=drag_before.value;drag_before.name=NULL;ts_owned_recipe_destroy(&drag_before);drag_parameter=-1;ts_app_request_render(&app);}
        else if(modal!=MODAL_NONE){ts_text_edit*edit=browser_open?&browser.filename:&modal_edit;if(k==SDLK_ESCAPE){if(browser_open){ts_file_browser_close(&browser);browser_open=false;}ts_text_edit_destroy(&modal_edit);modal=MODAL_NONE;modal_error[0]=0;overwrite_confirm=false;discard_confirm=false;SDL_StopTextInput();}else if(browser_open&&k==SDLK_TAB)ts_file_browser_toggle_focus(&browser);else if(browser_open&&browser.focus==TS_BROWSER_FOCUS_LIST&&(k==SDLK_UP||k==SDLK_DOWN||k==SDLK_PAGEUP||k==SDLK_PAGEDOWN||k==SDLK_HOME||k==SDLK_END)){ts_browser_key key=k==SDLK_UP?TS_BROWSER_KEY_UP:(k==SDLK_DOWN?TS_BROWSER_KEY_DOWN:(k==SDLK_PAGEUP?TS_BROWSER_KEY_PAGE_UP:(k==SDLK_PAGEDOWN?TS_BROWSER_KEY_PAGE_DOWN:(k==SDLK_HOME?TS_BROWSER_KEY_HOME:TS_BROWSER_KEY_END))));ts_file_browser_move(&browser,key);}else if(browser_open&&browser.focus==TS_BROWSER_FOCUS_FILENAME&&k==SDLK_HOME)ts_text_edit_home(edit);else if(browser_open&&browser.focus==TS_BROWSER_FOCUS_FILENAME&&k==SDLK_END)ts_text_edit_end(edit);else if((!browser_open||browser.focus==TS_BROWSER_FOCUS_FILENAME)&&k==SDLK_BACKSPACE){overwrite_confirm=false;discard_confirm=false;ts_text_edit_backspace(edit);}else if((!browser_open||browser.focus==TS_BROWSER_FOCUS_FILENAME)&&k==SDLK_DELETE){overwrite_confirm=false;discard_confirm=false;ts_text_edit_delete(edit);}else if((!browser_open||browser.focus==TS_BROWSER_FOCUS_FILENAME)&&k==SDLK_LEFT)ts_text_edit_left(edit);else if((!browser_open||browser.focus==TS_BROWSER_FOCUS_FILENAME)&&k==SDLK_RIGHT)ts_text_edit_right(edit);else if(k==SDLK_RETURN&&!e.key.repeat){bool ok=false;ts_io_error io={0};char chosen[1100],second[1100];if(browser_open&&browser.selected<browser.count){ts_browser_entry*entry=&browser.entries[browser.selected];if(entry->directory){ts_file_browser_enter(&browser,browser.selected);continue;}if(strcmp(browser.filename.text,entry->name)!=0){ts_file_browser_enter(&browser,browser.selected);continue;}}if(browser_open&&!ts_file_browser_result(&browser,chosen,sizeof chosen,second,sizeof second)){snprintf(modal_error,sizeof modal_error,"CHOOSE A FILE NAME");continue;}if(modal==MODAL_SAVE)ok=ts_app_save_recipe_confirmed(&app,chosen,overwrite_confirm,&io);else if(modal==MODAL_LOAD){if(ts_app_session_requires_discard(&app)&&!discard_confirm){discard_confirm=true;snprintf(modal_error,sizeof modal_error,"UNSAVED/PARENT - ENTER AGAIN TO DISCARD");continue;}ok=ts_app_load_recipe(&app,chosen,&io);}else if(modal==MODAL_BAKE)ok=ts_app_bake_confirmed(&app,chosen,second,overwrite_confirm,&io);else if(modal==MODAL_PARENT)ok=ts_app_update_parent(&app,true);else if(modal==MODAL_PARAMETER)ok=ts_app_set_parameter_text(&app,modal_parameter,modal_edit.text,modal_error,sizeof modal_error);else if(modal==MODAL_SELECT)ok=ts_app_select_recipe_confirmed(&app,pending_selection,true);if(ok){if(browser_open){snprintf(remembered_directory,sizeof remembered_directory,"%s",browser.directory);ts_file_browser_close(&browser);browser_open=false;}ts_text_edit_destroy(&modal_edit);modal=MODAL_NONE;modal_error[0]=0;overwrite_confirm=false;discard_confirm=false;SDL_StopTextInput();}else if(io.status==TS_IO_EXISTS&&(modal==MODAL_SAVE||modal==MODAL_BAKE)){overwrite_confirm=true;snprintf(modal_error,sizeof modal_error,"EXISTS - ENTER AGAIN TO REPLACE");}else if(!modal_error[0])snprintf(modal_error,sizeof modal_error,"%s",io.message[0]?io.message:"ACTION FAILED");}}
        else if (k == SDLK_ESCAPE)
          running = false;
        else if((modifiers&KMOD_CTRL)&&(k==SDLK_s||k==SDLK_o||k==SDLK_b)&&!e.key.repeat){if(drag_parameter>=0){if(!ts_recipe_fields_equal(&drag_before.value,&app.bank[app.selected].recipe))ts_recipe_history_commit(&app.history,&drag_before.value);ts_owned_recipe_destroy(&drag_before);drag_parameter=-1;}modal=k==SDLK_o?MODAL_LOAD:(k==SDLK_b?MODAL_BAKE:MODAL_SAVE);overwrite_confirm=false;browser_open=ts_file_browser_open(&browser,k==SDLK_o?TS_BROWSER_LOAD:(k==SDLK_b?TS_BROWSER_BAKE:TS_BROWSER_SAVE),remembered_directory,"");SDL_StartTextInput();}
        else if(k==SDLK_TAB){ts_app_page_move(&app,(modifiers&KMOD_SHIFT)?-1:1);}
        else if(k==SDLK_PAGEUP&&!e.key.repeat){ts_app_page_move(&app,-1);}
        else if(k==SDLK_PAGEDOWN&&!e.key.repeat){ts_app_page_move(&app,1);}
        else if(k==SDLK_UP){ts_app_focus_move(&app,-1);}
        else if(k==SDLK_DOWN){ts_app_focus_move(&app,1);}
        else if(k==SDLK_RETURN&&!e.key.repeat&&app.focused_parameter>=0){const ts_parameter_desc*d=ts_parameter_by_id((ts_parameter_id)app.focused_parameter);if(d->type==TS_PARAM_ENUM||d->type==TS_PARAM_BOOLEAN)ts_app_adjust_parameter(&app,d->id,1,true);else{char initial[128];if(d->id==TS_P_NAME)snprintf(initial,sizeof initial,"%s",app.bank[app.selected].recipe.name);else{double value=0;ts_parameter_get_number(d->id,&app.bank[app.selected].recipe,&value);snprintf(initial,sizeof initial,"%.9g",value);}modal=MODAL_PARAMETER;modal_parameter=d->id;ts_text_edit_init(&modal_edit,d->id==TS_P_NAME?TS_RECIPE_NAME_MAX_BYTES+1U:128U,initial);SDL_StartTextInput();}}
        else if((k==SDLK_LEFT||k==SDLK_RIGHT)&&app.focused_parameter>=0){double step=k==SDLK_RIGHT?1:-1;if(modifiers&KMOD_SHIFT)step*=10;if(modifiers&KMOD_CTRL)step*=.1;ts_app_adjust_parameter(&app,(ts_parameter_id)app.focused_parameter,step,true);}
        else if((modifiers&KMOD_CTRL)&&k==SDLK_z&&!e.key.repeat){if(modifiers&KMOD_SHIFT)ts_app_redo(&app);else ts_app_undo(&app);}
        else if((modifiers&KMOD_CTRL)&&k==SDLK_y&&!e.key.repeat){ts_app_redo(&app);}
        else if((modifiers&KMOD_CTRL)&&k==SDLK_p&&!e.key.repeat){if(modifiers&KMOD_SHIFT){modal=MODAL_PARENT;ts_text_edit_init(&modal_edit,2,"");SDL_StartTextInput();}else ts_app_commit_parent(&app);}
        else if (k == SDLK_F1 && app.selected > 0) {
          pending_selection=app.selected-1;if(ts_app_session_requires_discard(&app)){modal=MODAL_SELECT;ts_text_edit_init(&modal_edit,2,"");SDL_StartTextInput();}else ts_app_select_recipe_confirmed(&app,pending_selection,true);
        } else if (k == SDLK_F2 && app.selected + 1 < app.bank_count) {
          pending_selection=app.selected+1;if(ts_app_session_requires_discard(&app)){modal=MODAL_SELECT;ts_text_edit_init(&modal_edit,2,"");SDL_StartTextInput();}else ts_app_select_recipe_confirmed(&app,pending_selection,true);
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
          lock_note(device, &audio, ts_app_preview_source(&app),
                    app.bank[app.selected].recipe.root_midi_note);
        else if ((modifiers & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) == 0) {
          int note;
          if (ts_app_key_press(&app, ascii_key(k), e.key.repeat != 0, &note))
            lock_note(device, &audio, ts_app_preview_source(&app), note);
        }
      } else if(e.type==SDL_TEXTINPUT&&modal!=MODAL_NONE){if(!browser_open||browser.focus==TS_BROWSER_FOCUS_FILENAME){overwrite_confirm=false;discard_confirm=false;ts_text_edit_insert(browser_open?&browser.filename:&modal_edit,e.text.text);}}
      else if(modal!=MODAL_NONE&&(e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP||e.type==SDL_MOUSEMOTION||e.type==SDL_MOUSEWHEEL)){
        if(browser_open&&e.type==SDL_MOUSEWHEEL)ts_file_browser_wheel(&browser,-e.wheel.y);
        else if(browser_open&&e.type==SDL_MOUSEMOTION){int lx,ly;if(mouse_to_logical(window,renderer,&presentation,e.motion.x,e.motion.y,&lx,&ly))ts_file_browser_mouse_motion(&browser,ly);}
        else if(browser_open&&e.type==SDL_MOUSEBUTTONUP)ts_file_browser_mouse_release(&browser);
        else if(browser_open&&e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT){int lx,ly;if(mouse_to_logical(window,renderer,&presentation,e.button.x,e.button.y,&lx,&ly)&&!ts_file_browser_mouse_press(&browser,lx,ly,e.button.clicks)&&ly>=260&&ly<286){if(lx<180)ts_file_browser_home(&browser);else if(lx<280)ts_file_browser_root(&browser);else if(lx<390)ts_file_browser_parent(&browser);else ts_file_browser_mkdir(&browser,browser.filename.text);}}
      }
      else if (e.type == SDL_KEYUP) {
        int note;
        ts_app_key_release(&app, ascii_key(e.key.keysym.sym), &note);
        if (note >= 0)
          lock_off(device, &audio, note);
      } else if (e.type == SDL_MOUSEBUTTONDOWN &&
                 e.button.button == SDL_BUTTON_LEFT) {
        int lx, ly;
        if (mouse_to_logical(window, renderer, &presentation, e.button.x,e.button.y,&lx,&ly)) {
          int tab=ts_ui_tab_hit(lx,ly),parameter=ts_ui_parameter_hit(lx,ly,app.page);
          ts_ui_action action=ts_ui_action_hit(lx,ly);
          if(action!=TS_UI_ACTION_NONE){if(drag_parameter>=0){if(!ts_recipe_fields_equal(&drag_before.value,&app.bank[app.selected].recipe))ts_recipe_history_commit(&app.history,&drag_before.value);ts_owned_recipe_destroy(&drag_before);drag_parameter=-1;}if(action==TS_UI_COMMIT_PARENT)ts_app_commit_parent(&app);else if(action==TS_UI_MODE){ts_app_toggle_mode(&app,false);if(device)SDL_LockAudioDevice(device);audio.mixer.mode=app.mode;if(device)SDL_UnlockAudioDevice(device);}else{modal=action==TS_UI_UPDATE_PARENT?MODAL_PARENT:(action==TS_UI_SAVE?MODAL_SAVE:(action==TS_UI_LOAD?MODAL_LOAD:MODAL_BAKE));if(modal==MODAL_PARENT)ts_text_edit_init(&modal_edit,2,"");else browser_open=ts_file_browser_open(&browser,modal==MODAL_LOAD?TS_BROWSER_LOAD:(modal==MODAL_BAKE?TS_BROWSER_BAKE:TS_BROWSER_SAVE),remembered_directory,"");SDL_StartTextInput();}}
          else if(tab>=0)ts_app_set_page(&app,(ts_parameter_page)tab);
          else if(parameter>=0){app.focused_parameter=parameter;double steps=1;if(lx<610&&lx>=578)steps=lx<594?-1:1;
            else if(lx>=416&&lx<=576){const ts_parameter_desc*d=ts_parameter_by_id((ts_parameter_id)parameter);double old;ts_parameter_get_number((ts_parameter_id)parameter,&app.bank[app.selected].recipe,&old);double target=ts_parameter_from_position(d,ts_ui_slider_position(lx));steps=(target-old)/d->fine_step;drag_parameter=parameter;ts_owned_recipe_copy(&drag_before,&app.bank[app.selected].recipe);}
            ts_app_adjust_parameter(&app,(ts_parameter_id)parameter,steps,drag_parameter<0);
          } else {int recipe=ts_ui_recipe_hit(lx,ly,app.bank_count);if(recipe>=0&&(size_t)recipe!=app.selected){if(ts_app_session_requires_discard(&app)){pending_selection=(size_t)recipe;modal=MODAL_SELECT;ts_text_edit_init(&modal_edit,2,"");SDL_StartTextInput();}else ts_app_select_recipe_confirmed(&app,(size_t)recipe,true);}else apply_mouse_result(device, &audio, &app,ts_app_mouse_press(&app,lx,ly),error,sizeof error);}}
      } else if (e.type == SDL_MOUSEBUTTONUP &&
                 e.button.button == SDL_BUTTON_LEFT) {
        if(drag_parameter>=0){if(!ts_recipe_fields_equal(&drag_before.value,&app.bank[app.selected].recipe))ts_recipe_history_commit(&app.history,&drag_before.value);ts_owned_recipe_destroy(&drag_before);drag_parameter=-1;}
        apply_mouse_result(device, &audio, &app, ts_app_mouse_release(&app),
                           error, sizeof error);
      } else if(e.type==SDL_MOUSEMOTION&&drag_parameter>=0){int lx,ly;if(mouse_to_logical(window,renderer,&presentation,e.motion.x,e.motion.y,&lx,&ly)){const ts_parameter_desc*d=ts_parameter_by_id((ts_parameter_id)drag_parameter);double old;ts_parameter_get_number(d->id,&app.bank[app.selected].recipe,&old);double target=ts_parameter_from_position(d,ts_ui_slider_position(lx));ts_app_adjust_parameter(&app,d->id,(target-old)/d->fine_step,false);}}
      else if(e.type==SDL_MOUSEWHEEL&&app.focused_parameter>=0){int wx,wy,lx,ly;SDL_GetMouseState(&wx,&wy);int parameter=-1;if(mouse_to_logical(window,renderer,&presentation,wx,wy,&lx,&ly))parameter=ts_ui_parameter_hit(lx,ly,app.page);ts_app_adjust_parameter(&app,(ts_parameter_id)(parameter>=0?parameter:app.focused_parameter),e.wheel.y>0?1:-1,true);}
      else if (e.type == SDL_MOUSEMOTION && app.mouse_note >= 0) {
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
        if(drag_parameter>=0){if(!ts_recipe_fields_equal(&drag_before.value,&app.bank[app.selected].recipe))ts_recipe_history_commit(&app.history,&drag_before.value);ts_owned_recipe_destroy(&drag_before);drag_parameter=-1;}
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
    model.baked=ts_app_baked(&app);if(modal!=MODAL_NONE){model.modal_title=modal==MODAL_SAVE?"SAVE RECIPE":(modal==MODAL_LOAD?"LOAD RECIPE":(modal==MODAL_BAKE?"BAKE SAMPLE":(modal==MODAL_PARAMETER?"EDIT PARAMETER":(modal==MODAL_SELECT?"DISCARD SESSION AND SELECT?":"UPDATE PARENT?"))));model.modal_text=modal_edit.text;model.modal_error=modal_error[0]?modal_error:NULL;model.browser=browser_open?&browser:NULL;}
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
  if(modal!=MODAL_NONE){SDL_StopTextInput();ts_text_edit_destroy(&modal_edit);}
  if(browser_open)ts_file_browser_close(&browser);
  ts_owned_recipe_destroy(&drag_before);
  free(rgba);
  free(fb);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  ts_app_dispose(&app);
  return result;
}
