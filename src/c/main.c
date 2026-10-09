/**
 * Pulse Time — Foreground App
 *
 * Menu-driven control panel for the background worker.
 *   Worker       start / stop the background worker
 *   Mode         cycle Terse -> Digits -> Morse
 *   Vibe         cycle Standard -> Gentle -> Strong -> Learn
 *   Hourly chime toggle the on-the-hour hour buzz
 *   Test buzz    play the current time now
 *   How to read  pointer to the phone-side guide
 *
 * The phone settings page (src/pkjs + docs/) sends the same settings over
 * AppMessage, plus an optional "practice time" to buzz.
 *
 * Open this app once to configure, then close it.
 * The worker keeps running behind any watchface.
 */

#include <pebble.h>
#include "pulse_time.h"

DEFINE_PRESETS  // expands the preset table

// --- Menu rows ---
enum {
  ROW_WORKER = 0,
  ROW_MODE,
  ROW_PRESET,
  ROW_CHIME,
  ROW_TEST,
  ROW_HELP,
  ROW_COUNT,
};

// --- State ---
static int s_current_preset;
static PulseTimeMode s_current_mode;
static bool s_chime_enabled;

// --- UI ---
static Window    *s_main_window;
static MenuLayer *s_menu_layer;
static Window    *s_help_window;
static TextLayer *s_help_text;

// --- Helpers ---

static void save_settings(void) {
  const VibePreset *p = &s_presets[s_current_preset];
  persist_write_int(STORAGE_KEY_VIBE_LONG,  p->vibe_long);
  persist_write_int(STORAGE_KEY_VIBE_SHORT, p->vibe_short);
  persist_write_int(STORAGE_KEY_GAP_INTRA,  p->gap_intra);
  persist_write_int(STORAGE_KEY_GAP_INTER,  p->gap_inter);
  persist_write_int(STORAGE_KEY_PRESET,     s_current_preset);
  persist_write_int(STORAGE_KEY_MODE,       (int)s_current_mode);
  persist_write_int(STORAGE_KEY_CHIME,      s_chime_enabled ? 1 : 0);
}

static void notify_worker(uint16_t type, uint16_t data0, uint16_t data1) {
  if (!app_worker_is_running()) return;
  AppWorkerMessage msg = { .data0 = data0, .data1 = data1, .data2 = 0 };
  app_worker_send_message(type, &msg);
}

static void settings_changed(void) {
  save_settings();
  notify_worker(MSG_KEY_SETTINGS, 0, 0);
  if (s_menu_layer) menu_layer_reload_data(s_menu_layer);
}

// --- Menu ---

static uint16_t menu_get_num_rows(MenuLayer *layer, uint16_t section, void *ctx) {
  return ROW_COUNT;
}

static void menu_draw_row(GContext *gctx, const Layer *cell_layer,
                          MenuIndex *idx, void *ctx) {
  const char *title = "";
  const char *subtitle = "";

  switch (idx->row) {
    case ROW_WORKER:
      title = "Worker";
      subtitle = app_worker_is_running() ? "Running" : "Stopped";
      break;
    case ROW_MODE:
      title = "Mode";
      subtitle = mode_name(s_current_mode);
      break;
    case ROW_PRESET:
      title = "Vibe";
      subtitle = s_presets[s_current_preset].name;
      break;
    case ROW_CHIME:
      title = "Hourly chime";
      subtitle = s_chime_enabled ? "On" : "Off";
      break;
    case ROW_TEST:
      title = "Test buzz";
      subtitle = app_worker_is_running() ? "Play the time now"
                                         : "Turn worker on first";
      break;
    case ROW_HELP:
      title = "How to read it";
      subtitle = "See phone settings";
      break;
  }
  menu_cell_basic_draw(gctx, cell_layer, title, subtitle, NULL);
}

static void help_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_help_text = text_layer_create(
      GRect(8, PBL_IF_ROUND_ELSE(24, 4), bounds.size.w - 16, bounds.size.h - 8));
  text_layer_set_font(s_help_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_help_text,
                                PBL_IF_ROUND_ELSE(GTextAlignmentCenter,
                                                  GTextAlignmentLeft));
  text_layer_set_text(s_help_text,
    "Double-tap your wrist to feel the time.\n\n"
    "A step-by-step guide with a pattern player lives in the Pebble phone "
    "app: open Pulse Time and tap the settings gear.");
  layer_add_child(root, text_layer_get_layer(s_help_text));
}

static void help_window_unload(Window *window) {
  text_layer_destroy(s_help_text);
  s_help_text = NULL;
}

static void menu_select(MenuLayer *layer, MenuIndex *idx, void *ctx) {
  switch (idx->row) {
    case ROW_WORKER:
      if (app_worker_is_running()) {
        app_worker_kill();
      } else {
        AppWorkerResult result = app_worker_launch();
        if (result != APP_WORKER_RESULT_SUCCESS &&
            result != APP_WORKER_RESULT_ALREADY_RUNNING) {
          vibes_double_pulse();   // could not start (e.g. another worker owns the slot)
        }
      }
      menu_layer_reload_data(s_menu_layer);
      break;
    case ROW_MODE:
      s_current_mode = (PulseTimeMode)((s_current_mode + 1) % MODE_COUNT);
      settings_changed();
      vibes_short_pulse();    // one pulse = mode changed
      break;
    case ROW_PRESET:
      s_current_preset = (s_current_preset + 1) % NUM_PRESETS;
      settings_changed();
      vibes_double_pulse();   // two pulses = preset changed
      break;
    case ROW_CHIME:
      s_chime_enabled = !s_chime_enabled;
      settings_changed();
      vibes_long_pulse();     // one long pulse = chime toggled
      break;
    case ROW_TEST:
      if (app_worker_is_running()) {
        notify_worker(MSG_KEY_TRIGGER, 0, 0);
      } else {
        vibes_double_pulse();
      }
      break;
    case ROW_HELP:
      window_stack_push(s_help_window, true);
      break;
  }
}

// --- AppMessage (from the phone settings page) ---

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool changed = false;

  Tuple *t = dict_find(iter, MESSAGE_KEY_MODE);
  if (t && t->value->int32 >= 0 && t->value->int32 < MODE_COUNT) {
    s_current_mode = (PulseTimeMode)t->value->int32;
    changed = true;
  }
  t = dict_find(iter, MESSAGE_KEY_PRESET);
  if (t && t->value->int32 >= 0 && t->value->int32 < NUM_PRESETS) {
    s_current_preset = (int)t->value->int32;
    changed = true;
  }
  t = dict_find(iter, MESSAGE_KEY_CHIME);
  if (t) {
    s_chime_enabled = t->value->int32 != 0;
    changed = true;
  }
  if (changed) settings_changed();

  // Optional "practice" request: buzz this time in the chosen mode/preset.
  Tuple *ph = dict_find(iter, MESSAGE_KEY_PRACTICE_HOUR);
  Tuple *pm = dict_find(iter, MESSAGE_KEY_PRACTICE_MINUTE);
  if (ph && pm) {
    if (!app_worker_is_running()) app_worker_launch();
    // A just-launched worker may not be listening yet; the user can retry
    // from the page. Normally the worker is already running.
    notify_worker(MSG_KEY_PLAY_TIME,
                  (uint16_t)ph->value->int32, (uint16_t)pm->value->int32);
  }
}

// --- Window ---

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = menu_get_num_rows,
    .draw_row     = menu_draw_row,
    .select_click = menu_select,
  });
  menu_layer_set_highlight_colors(s_menu_layer,
      PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack), GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void main_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
}

// --- Lifecycle ---

static void init(void) {
  // Load saved state
  s_current_preset = (int)persist_read_int(STORAGE_KEY_PRESET);
  if (s_current_preset < 0 || s_current_preset >= NUM_PRESETS) {
    s_current_preset = 0;
  }
  int mode = (int)persist_read_int(STORAGE_KEY_MODE);
  s_current_mode = (mode >= 0 && mode < MODE_COUNT) ?
                   (PulseTimeMode)mode : MODE_TERSE;
  s_chime_enabled = persist_read_int(STORAGE_KEY_CHIME) != 0;

  // Ensure settings are persisted (first run)
  save_settings();

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 32);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load   = main_window_load,
    .unload = main_window_unload,
  });

  s_help_window = window_create();
  window_set_window_handlers(s_help_window, (WindowHandlers) {
    .load   = help_window_load,
    .unload = help_window_unload,
  });

  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  window_destroy(s_help_window);
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
