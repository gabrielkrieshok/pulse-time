/**
 * Pulse Time — Foreground App
 *
 * Control panel for the background worker.
 *   SELECT: start / stop the background worker
 *   UP:     cycle mode (Terse → Digits → Morse)
 *   DOWN:   cycle vibe preset (Standard → Gentle → Strong)
 *
 * Open this app once to configure, then close it.
 * The worker keeps running behind any watchface.
 * Tap your wrist to feel the time.
 */

#include <pebble.h>
#include "pulse_time.h"

DEFINE_PRESETS  // expands the preset table

// --- State ---
static int s_current_preset;
static PulseTimeMode s_current_mode;

// --- UI ---
static Window    *s_main_window;
static TextLayer *s_title_layer;
static TextLayer *s_status_layer;
static TextLayer *s_mode_layer;
static TextLayer *s_preset_layer;
static TextLayer *s_desc_layer;
static TextLayer *s_hint_layer;

static char s_status_buf[32];
static char s_mode_buf[32];
static char s_preset_buf[32];

// --- Helpers ---

static void save_settings(void) {
  const VibePreset *p = &s_presets[s_current_preset];
  persist_write_int(STORAGE_KEY_VIBE_LONG,  p->vibe_long);
  persist_write_int(STORAGE_KEY_VIBE_SHORT, p->vibe_short);
  persist_write_int(STORAGE_KEY_GAP_INTRA,  p->gap_intra);
  persist_write_int(STORAGE_KEY_GAP_INTER,  p->gap_inter);
  persist_write_int(STORAGE_KEY_PRESET,     s_current_preset);
  persist_write_int(STORAGE_KEY_MODE,       (int)s_current_mode);
}

static void notify_worker(uint16_t type) {
  if (!app_worker_is_running()) return;
  AppWorkerMessage msg = { .data0 = 0 };
  app_worker_send_message(type, &msg);
}

static const char* mode_description(PulseTimeMode mode) {
  switch (mode) {
    case MODE_TERSE:
      return "5h groups + quarters\n~15 min accuracy";
    case MODE_DIGITS:
      return "10h/1h + 10m/1m\nMinute precision";
    case MODE_MORSE:
      return "Each digit in\nMorse code";
    default:
      return "";
  }
}

static void update_ui(void) {
  // Worker status
  if (app_worker_is_running()) {
    snprintf(s_status_buf, sizeof(s_status_buf), "RUNNING");
    text_layer_set_text_color(s_status_layer, GColorGreen);
  } else {
    snprintf(s_status_buf, sizeof(s_status_buf), "STOPPED");
    text_layer_set_text_color(s_status_layer, GColorRed);
  }
  text_layer_set_text(s_status_layer, s_status_buf);

  // Mode
  snprintf(s_mode_buf, sizeof(s_mode_buf), "Mode: %s", mode_name(s_current_mode));
  text_layer_set_text(s_mode_layer, s_mode_buf);

  // Mode description
  text_layer_set_text(s_desc_layer, mode_description(s_current_mode));

  // Preset
  snprintf(s_preset_buf, sizeof(s_preset_buf), "Vibe: %s",
           s_presets[s_current_preset].name);
  text_layer_set_text(s_preset_layer, s_preset_buf);
}

// --- Button handlers ---

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (app_worker_is_running()) {
    app_worker_kill();
  } else {
    AppWorkerResult result = app_worker_launch();
    if (result != APP_WORKER_RESULT_SUCCESS &&
        result != APP_WORKER_RESULT_ALREADY_RUNNING) {
      snprintf(s_status_buf, sizeof(s_status_buf), "Error: %d", (int)result);
      text_layer_set_text(s_status_layer, s_status_buf);
      text_layer_set_text_color(s_status_layer, GColorYellow);
      return;
    }
  }
  update_ui();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_current_mode = (PulseTimeMode)((s_current_mode + 1) % MODE_COUNT);
  save_settings();
  notify_worker(MSG_KEY_SETTINGS);
  update_ui();
  vibes_short_pulse();  // feedback
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_current_preset = (s_current_preset + 1) % NUM_PRESETS;
  save_settings();
  notify_worker(MSG_KEY_SETTINGS);
  update_ui();
  vibes_double_pulse();  // feedback
}

// Long-press SELECT: trigger a test vibe via the worker
static void select_long_handler(ClickRecognizerRef recognizer, void *context) {
  notify_worker(MSG_KEY_TRIGGER);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long_handler, NULL);
}

// --- Window ---

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int w = bounds.size.w;

  int y = 4;

  // Title
  s_title_layer = text_layer_create(GRect(0, y, w, 28));
  text_layer_set_text(s_title_layer, "Pulse Time");
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_title_layer));
  y += 30;

  // Status
  s_status_layer = text_layer_create(GRect(0, y, w, 20));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(s_status_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_status_layer));
  y += 24;

  // Mode
  s_mode_layer = text_layer_create(GRect(0, y, w, 20));
  text_layer_set_text_alignment(s_mode_layer, GTextAlignmentCenter);
  text_layer_set_font(s_mode_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(s_mode_layer, GColorClear);
  text_layer_set_text_color(s_mode_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_mode_layer));
  y += 22;

  // Mode description
  s_desc_layer = text_layer_create(GRect(8, y, w - 16, 32));
  text_layer_set_text_alignment(s_desc_layer, GTextAlignmentCenter);
  text_layer_set_font(s_desc_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_background_color(s_desc_layer, GColorClear);
  text_layer_set_text_color(s_desc_layer, GColorDarkGray);
  layer_add_child(root, text_layer_get_layer(s_desc_layer));
  y += 34;

  // Preset
  s_preset_layer = text_layer_create(GRect(0, y, w, 20));
  text_layer_set_text_alignment(s_preset_layer, GTextAlignmentCenter);
  text_layer_set_font(s_preset_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(s_preset_layer, GColorClear);
  text_layer_set_text_color(s_preset_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_preset_layer));
  y += 24;

  // Hint
  s_hint_layer = text_layer_create(GRect(4, y, w - 8, 30));
  text_layer_set_text(s_hint_layer,
    "UP mode  SEL on/off  DN preset");
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer, GColorLightGray);
  layer_add_child(root, text_layer_get_layer(s_hint_layer));

  update_ui();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_mode_layer);
  text_layer_destroy(s_desc_layer);
  text_layer_destroy(s_preset_layer);
  text_layer_destroy(s_hint_layer);
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

  // Ensure settings are persisted (first run)
  save_settings();

  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load   = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
