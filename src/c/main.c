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
static bool s_quiet_enabled;
static int  s_quiet_start;
static int  s_quiet_end;

// A practice time waiting for a freshly launched worker to come up
static AppTimer *s_practice_timer;
static uint16_t s_practice_hour, s_practice_minute;

// --- UI ---
static Window    *s_main_window;
static MenuLayer *s_menu_layer;
static Window    *s_help_window;
static ScrollLayer *s_help_scroll;
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
  persist_write_int(STORAGE_KEY_QUIET,      s_quiet_enabled ? 1 : 0);
  persist_write_int(STORAGE_KEY_QUIET_START, s_quiet_start);
  persist_write_int(STORAGE_KEY_QUIET_END,   s_quiet_end);
}

static void notify_worker(uint16_t type, uint16_t data0, uint16_t data1) {
  if (!app_worker_is_running()) return;
  AppWorkerMessage msg = { .data0 = data0, .data1 = data1, .data2 = 0 };
  app_worker_send_message(type, &msg);
}

// Tell the phone what the watch currently has, so the settings page opens on
// the real values instead of whatever the phone last saved.
static void send_settings_to_phone(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int32(out, MESSAGE_KEY_MODE,   (int32_t)s_current_mode);
  dict_write_int32(out, MESSAGE_KEY_PRESET, s_current_preset);
  dict_write_int32(out, MESSAGE_KEY_CHIME,  s_chime_enabled ? 1 : 0);
  dict_write_int32(out, MESSAGE_KEY_QUIET,       s_quiet_enabled ? 1 : 0);
  dict_write_int32(out, MESSAGE_KEY_QUIET_START, s_quiet_start);
  dict_write_int32(out, MESSAGE_KEY_QUIET_END,   s_quiet_end);
  app_message_outbox_send();
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

static char s_chime_subtitle[24];

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
      if (!s_chime_enabled) {
        subtitle = "Off";
      } else if (s_quiet_enabled) {
        snprintf(s_chime_subtitle, sizeof(s_chime_subtitle),
                 "On, quiet %02d-%02d", s_quiet_start, s_quiet_end);
        subtitle = s_chime_subtitle;
      } else {
        subtitle = "On";
      }
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
  int pad_x = PBL_IF_ROUND_ELSE(18, 8);

  // UP/DOWN scroll the text; it is taller than the screen.
  s_help_scroll = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(s_help_scroll, window);

  GRect text_frame = GRect(pad_x, PBL_IF_ROUND_ELSE(24, 4),
                           bounds.size.w - 2 * pad_x, 2000);
  s_help_text = text_layer_create(text_frame);
  text_layer_set_font(s_help_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_help_text,
                                PBL_IF_ROUND_ELSE(GTextAlignmentCenter,
                                                  GTextAlignmentLeft));
  text_layer_set_text(s_help_text,
    "Triple-tap your wrist to feel the time.\n\n"
    "Long buzz = big unit, short buzz = small unit.\n\n"
    "Terse: hours first (long = 5, short = 1), pause, then long = 15 min.\n\n"
    "Digits: hours, then minutes. Long = tens, short = ones.\n\n"
    "Morse: every digit is 5 buzzes (long = dash, short = dot).\n\n"
    "For a full guide and a pattern player, open Pulse Time in the Pebble "
    "phone app and tap the settings gear.");

  // Size the text layer to its content, then tell the scroll layer how far to scroll.
  GSize content = text_layer_get_content_size(s_help_text);
  text_frame.size.h = content.h + 4;
  layer_set_frame(text_layer_get_layer(s_help_text), text_frame);
  scroll_layer_set_content_size(s_help_scroll,
      GSize(bounds.size.w, text_frame.origin.y + text_frame.size.h +
                           PBL_IF_ROUND_ELSE(32, 12)));

  scroll_layer_add_child(s_help_scroll, text_layer_get_layer(s_help_text));
  layer_add_child(root, scroll_layer_get_layer(s_help_scroll));
}

static void help_window_unload(Window *window) {
  text_layer_destroy(s_help_text);
  scroll_layer_destroy(s_help_scroll);
  s_help_text = NULL;
  s_help_scroll = NULL;
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

static void practice_timer_fired(void *data) {
  s_practice_timer = NULL;
  if (app_worker_is_running()) {
    notify_worker(MSG_KEY_PLAY_TIME, s_practice_hour, s_practice_minute);
  } else {
    vibes_double_pulse();   // worker would not start; nothing to play
  }
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool changed = false;

  // The phone page just opened and wants the watch's current settings.
  if (dict_find(iter, MESSAGE_KEY_SYNC)) {
    send_settings_to_phone();
    return;
  }

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
  t = dict_find(iter, MESSAGE_KEY_QUIET);
  if (t) {
    s_quiet_enabled = t->value->int32 != 0;
    changed = true;
  }
  t = dict_find(iter, MESSAGE_KEY_QUIET_START);
  if (t && t->value->int32 >= 0 && t->value->int32 < 24) {
    s_quiet_start = (int)t->value->int32;
    changed = true;
  }
  t = dict_find(iter, MESSAGE_KEY_QUIET_END);
  if (t && t->value->int32 >= 0 && t->value->int32 < 24) {
    s_quiet_end = (int)t->value->int32;
    changed = true;
  }
  if (changed) settings_changed();

  // Optional "practice" request: buzz this time in the chosen mode/preset.
  Tuple *ph = dict_find(iter, MESSAGE_KEY_PRACTICE_HOUR);
  Tuple *pm = dict_find(iter, MESSAGE_KEY_PRACTICE_MINUTE);
  if (ph && pm) {
    uint16_t h = (uint16_t)ph->value->int32;
    uint16_t m = (uint16_t)pm->value->int32;
    if (app_worker_is_running()) {
      notify_worker(MSG_KEY_PLAY_TIME, h, m);
    } else {
      // The worker starts asynchronously; give it a moment before messaging it.
      s_practice_hour = h;
      s_practice_minute = m;
      if (app_worker_launch() == APP_WORKER_RESULT_SUCCESS) {
        if (s_practice_timer) app_timer_cancel(s_practice_timer);
        s_practice_timer = app_timer_register(800, practice_timer_fired, NULL);
      } else {
        vibes_double_pulse();
      }
    }
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
  s_quiet_enabled = persist_read_int(STORAGE_KEY_QUIET) != 0;
  s_quiet_start = persist_exists(STORAGE_KEY_QUIET_START) ?
                  (int)persist_read_int(STORAGE_KEY_QUIET_START) % 24 : QUIET_START_DEFAULT;
  s_quiet_end   = persist_exists(STORAGE_KEY_QUIET_END) ?
                  (int)persist_read_int(STORAGE_KEY_QUIET_END) % 24 : QUIET_END_DEFAULT;

  // Ensure settings are persisted (first run)
  save_settings();

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 96);

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
  if (s_practice_timer) app_timer_cancel(s_practice_timer);
  window_destroy(s_help_window);
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
