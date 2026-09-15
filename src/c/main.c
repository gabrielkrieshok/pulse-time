/**
 * Pulse Time — Foreground App
 *
 * Control panel for the background worker.
 *   SELECT:       start / stop the background worker
 *   SELECT (hold): test vibe (plays the current time)
 *   UP:           cycle mode (Terse → Digits → Morse)
 *   UP (hold):    toggle hourly chime
 *   DOWN:         cycle vibe preset (Standard → Gentle → Strong → Learn)
 *
 * Open this app once to configure, then close it.
 * The worker keeps running behind any watchface.
 * Double-tap your wrist to feel the time.
 */

#include <pebble.h>
#include "pulse_time.h"

DEFINE_PRESETS  // expands the preset table

// --- State ---
static int s_current_preset;
static PulseTimeMode s_current_mode;
static bool s_chime_enabled;

// --- UI ---
static Window    *s_main_window;
static TextLayer *s_title_layer;
static TextLayer *s_status_layer;
static TextLayer *s_mode_layer;
static TextLayer *s_desc_layer;
static TextLayer *s_preset_layer;
static TextLayer *s_chime_layer;
static TextLayer *s_hint_layer;

static char s_status_buf[32];
static char s_mode_buf[32];
static char s_preset_buf[32];
static char s_chime_buf[32];

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

static void notify_worker(uint16_t type) {
  if (!app_worker_is_running()) return;
  AppWorkerMessage msg = { .data0 = 0 };
  app_worker_send_message(type, &msg);
}

static const char* mode_description(PulseTimeMode mode) {
  switch (mode) {
    case MODE_TERSE:  return "5h + 1h, then quarters";
    case MODE_DIGITS: return "10s + 1s digits, to the minute";
    case MODE_MORSE:  return "Each digit in Morse";
    default:          return "";
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
  text_layer_set_text(s_desc_layer, mode_description(s_current_mode));

  // Preset
  snprintf(s_preset_buf, sizeof(s_preset_buf), "Vibe: %s",
           s_presets[s_current_preset].name);
  text_layer_set_text(s_preset_layer, s_preset_buf);

  // Chime
  snprintf(s_chime_buf, sizeof(s_chime_buf), "Hourly chime: %s",
           s_chime_enabled ? "On" : "Off");
  text_layer_set_text(s_chime_layer, s_chime_buf);
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
  vibes_short_pulse();  // feedback: one pulse = mode changed
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_current_preset = (s_current_preset + 1) % NUM_PRESETS;
  save_settings();
  notify_worker(MSG_KEY_SETTINGS);
  update_ui();
  vibes_double_pulse();  // feedback: two pulses = preset changed
}

// Long-press UP: toggle hourly chime
static void up_long_handler(ClickRecognizerRef recognizer, void *context) {
  s_chime_enabled = !s_chime_enabled;
  save_settings();
  notify_worker(MSG_KEY_SETTINGS);
  update_ui();
  vibes_long_pulse();  // feedback: one long pulse = chime toggled
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
  window_long_click_subscribe(BUTTON_ID_UP, 700, up_long_handler, NULL);
}

// --- Window ---

static TextLayer* add_text_layer(Layer *root, GRect frame, const char *font_key,
                                 GColor color) {
  TextLayer *tl = text_layer_create(frame);
  text_layer_set_text_alignment(tl, GTextAlignmentCenter);
  text_layer_set_font(tl, fonts_get_system_font(font_key));
  text_layer_set_background_color(tl, GColorClear);
  text_layer_set_text_color(tl, color);
  layer_add_child(root, text_layer_get_layer(tl));
  return tl;
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int w = bounds.size.w;
  int y = 4;

  s_title_layer = add_text_layer(root, GRect(0, y, w, 26),
                                 FONT_KEY_GOTHIC_24_BOLD, GColorWhite);
  text_layer_set_text(s_title_layer, "Pulse Time");
  y += 26;

  s_status_layer = add_text_layer(root, GRect(0, y, w, 20),
                                  FONT_KEY_GOTHIC_18_BOLD, GColorWhite);
  y += 20;

  s_mode_layer = add_text_layer(root, GRect(0, y, w, 20),
                                FONT_KEY_GOTHIC_18, GColorWhite);
  y += 20;

  s_desc_layer = add_text_layer(root, GRect(8, y, w - 16, 16),
                                FONT_KEY_GOTHIC_14, GColorLightGray);
  y += 18;

  s_preset_layer = add_text_layer(root, GRect(0, y, w, 20),
                                  FONT_KEY_GOTHIC_18, GColorWhite);
  y += 20;

  s_chime_layer = add_text_layer(root, GRect(0, y, w, 20),
                                 FONT_KEY_GOTHIC_18, GColorWhite);
  y += 20;

  s_hint_layer = add_text_layer(root, GRect(4, y, w - 8, 30),
                                FONT_KEY_GOTHIC_14, GColorLightGray);
  text_layer_set_text(s_hint_layer,
    "UP mode  SEL on/off  DN vibe\nhold UP chime  hold SEL test");

  update_ui();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_mode_layer);
  text_layer_destroy(s_desc_layer);
  text_layer_destroy(s_preset_layer);
  text_layer_destroy(s_chime_layer);
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
  s_chime_enabled = persist_read_int(STORAGE_KEY_CHIME) != 0;

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
