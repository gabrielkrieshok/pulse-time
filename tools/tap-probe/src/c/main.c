/**
 * Tap Probe — a sideloadable diagnostic for Pulse Time's triple-tap trigger.
 *
 * Shows, live on the watch:
 *   APP     taps the foreground app receives from accel_tap_service, with the
 *           axis, direction and the gap since the previous tap
 *   WORKER  taps this app's own background worker receives over the same
 *           service. This is the path Pulse Time actually uses.
 *   TRIPLE  the Pulse Time counter (3 taps, each within 800 ms) run on both
 *           streams, with how many times each would have fired
 *   RAW     optional: peak accelerometer magnitude, to see how hard a knock is
 *
 * Buttons: SELECT starts or stops the worker. UP toggles the raw readout.
 *          DOWN zeroes the counters.
 *
 * Every event is also logged, so `pebble logs --phone <IP>` shows the same
 * information with timestamps.
 */

#include <pebble.h>

// Message types shared with worker_src/c/worker.c (keep both copies equal)
#define MSG_WORKER_ALIVE  0   // worker -> app: I'm running
#define MSG_WORKER_TAP    1   // worker -> app: tap seen (data0 = axis, data1 = direction > 0)
#define MSG_WORKER_FIRE   2   // worker -> app: triple-tap counter fired
#define MSG_APP_PING      3   // app -> worker: are you there?

// Same numbers as Pulse Time's worker
#define TAPS_REQUIRED        3
#define TAP_WINDOW_MS        800

#define IGNORE_AFTER_VIBE_MS 600   // the confirmation buzz can itself look like a tap
#define FLASH_MS             500

static Window    *s_window;
static TextLayer *s_text;
static char       s_buf[220];
static char       s_note[36] = "tap your wrist";

static int   s_app_taps, s_app_fires, s_app_resets;
static int   s_worker_taps, s_worker_fires;
static int   s_last_gap_ms;
static char  s_last_axis = '-', s_last_dir = ' ';
static int   s_count;
static bool  s_worker_linked;
static bool  s_raw_on;
static int   s_raw_peak_mg, s_raw_hold_mg, s_raw_batches;

static AppTimer *s_window_timer, *s_flash_timer;
static uint64_t  s_last_tap_ms, s_ignore_until_ms;

static uint64_t now_ms(void) {
  time_t s; uint16_t ms;
  s = time_ms(&s, &ms);
  return (uint64_t)s * 1000 + ms;
}

static void render(void) {
  const char *worker;
  if (!app_worker_is_running())  worker = "stopped (SELECT)";
  else if (s_worker_linked)      worker = "running, linked";
  else                           worker = "running";

  char raw[36];
  if (s_raw_on) {
    snprintf(raw, sizeof raw, "RAW %d mG (1s max %d)", s_raw_peak_mg, s_raw_hold_mg);
  } else {
    snprintf(raw, sizeof raw, "RAW off (UP)");
  }

  snprintf(s_buf, sizeof s_buf,
           "APP taps %d  %c%c %dms\n"
           "app triple %d/3  fired %d\n"
           "WORKER %s\n"
           "wkr taps %d  fired %d\n"
           "%s\n"
           "%s",
           s_app_taps, s_last_axis, s_last_dir, s_last_gap_ms,
           s_count, s_app_fires,
           worker,
           s_worker_taps, s_worker_fires,
           raw, s_note);
  text_layer_set_text(s_text, s_buf);
}

static void set_note(const char *n) {
  strncpy(s_note, n, sizeof s_note - 1);
  s_note[sizeof s_note - 1] = '\0';
}

// --- Flash the screen when the app-side counter fires ---

static void flash_off(void *data) {
  s_flash_timer = NULL;
  window_set_background_color(s_window, GColorWhite);
  text_layer_set_text_color(s_text, GColorBlack);
}

static void flash(void) {
  window_set_background_color(s_window, GColorBlack);
  text_layer_set_text_color(s_text, GColorWhite);
  if (s_flash_timer) app_timer_cancel(s_flash_timer);
  s_flash_timer = app_timer_register(FLASH_MS, flash_off, NULL);
}

// --- App-side tap stream, with the Pulse Time counter on top ---

static void window_timeout(void *data) {
  s_window_timer = NULL;
  s_app_resets++;
  s_count = 0;
  set_note("reset: >800ms gap");
  APP_LOG(APP_LOG_LEVEL_INFO, "triple: reset, no tap within %d ms", TAP_WINDOW_MS);
  render();
}

static void app_tap_handler(AccelAxisType axis, int32_t direction) {
  uint64_t now = now_ms();
  s_last_axis = (axis == ACCEL_AXIS_X) ? 'X' : (axis == ACCEL_AXIS_Y) ? 'Y' : 'Z';
  s_last_dir  = direction > 0 ? '+' : '-';

  if (now < s_ignore_until_ms) {
    set_note("vibe echo ignored");
    APP_LOG(APP_LOG_LEVEL_INFO, "app tap %c%c ignored (vibe echo)", s_last_axis, s_last_dir);
    render();
    return;
  }

  s_last_gap_ms = s_last_tap_ms ? (int)(now - s_last_tap_ms) : 0;
  s_last_tap_ms = now;
  s_app_taps++;

  if (s_window_timer) {
    app_timer_cancel(s_window_timer);
    s_window_timer = NULL;
  }
  s_count++;
  if (s_count >= TAPS_REQUIRED) {
    s_count = 0;
    s_app_fires++;
    set_note("app: WOULD FIRE");
    flash();
    vibes_short_pulse();
    s_ignore_until_ms = now + IGNORE_AFTER_VIBE_MS;
  } else {
    s_window_timer = app_timer_register(TAP_WINDOW_MS, window_timeout, NULL);
    set_note(s_count == 1 ? "tap 1 of 3" : "tap 2 of 3");
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "app tap #%d %c%c gap %d ms -> count %d",
          s_app_taps, s_last_axis, s_last_dir, s_last_gap_ms, s_count);
  render();
}

// --- Worker-side tap stream ---

static void worker_message_handler(uint16_t type, AppWorkerMessage *msg) {
  switch (type) {
    case MSG_WORKER_ALIVE:
      s_worker_linked = true;
      APP_LOG(APP_LOG_LEVEL_INFO, "worker: alive");
      break;
    case MSG_WORKER_TAP:
      s_worker_linked = true;
      s_worker_taps++;
      APP_LOG(APP_LOG_LEVEL_INFO, "worker tap #%d axis %d dir %s",
              s_worker_taps, msg->data0, msg->data1 ? "+" : "-");
      break;
    case MSG_WORKER_FIRE:
      s_worker_fires++;
      set_note("worker: WOULD FIRE");
      APP_LOG(APP_LOG_LEVEL_INFO, "worker: triple-tap fired (#%d)", s_worker_fires);
      break;
  }
  render();
}

static void ping_worker(void) {
  if (!app_worker_is_running()) return;
  AppWorkerMessage m = { .data0 = 0, .data1 = 0, .data2 = 0 };
  app_worker_send_message(MSG_APP_PING, &m);
}

// --- Optional raw accelerometer readout ---

static uint32_t isqrt(uint32_t n) {
  uint32_t r = 0, bit = 1u << 30;
  while (bit > n) bit >>= 2;
  while (bit) {
    if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; }
    else r >>= 1;
    bit >>= 2;
  }
  return r;
}

static void accel_data_handler(AccelData *data, uint32_t num_samples) {
  int32_t peak_sq = 0;
  for (uint32_t i = 0; i < num_samples; i++) {
    if (data[i].did_vibrate) continue;
    int32_t m = (int32_t)data[i].x * data[i].x + (int32_t)data[i].y * data[i].y +
                (int32_t)data[i].z * data[i].z;
    if (m > peak_sq) peak_sq = m;
  }
  s_raw_peak_mg = (int)isqrt((uint32_t)peak_sq);
  if (++s_raw_batches >= 5) {   // 5 batches x 200 ms = a 1 s hold
    s_raw_batches = 0;
    s_raw_hold_mg = 0;
  }
  if (s_raw_peak_mg > s_raw_hold_mg) s_raw_hold_mg = s_raw_peak_mg;
  render();
}

static void set_raw(bool on) {
  if (on == s_raw_on) return;
  s_raw_on = on;
  if (on) {
    accel_data_service_subscribe(5, accel_data_handler);
    accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
  } else {
    accel_data_service_unsubscribe();
    s_raw_peak_mg = s_raw_hold_mg = s_raw_batches = 0;
  }
  render();
}

// --- Buttons ---

static void select_click(ClickRecognizerRef rec, void *ctx) {
  if (app_worker_is_running()) {
    app_worker_kill();
    s_worker_linked = false;
    set_note("worker stopped");
  } else {
    AppWorkerResult r = app_worker_launch();
    if (r == APP_WORKER_RESULT_SUCCESS || r == APP_WORKER_RESULT_ALREADY_RUNNING) {
      set_note("worker starting...");
    } else if (r == APP_WORKER_RESULT_DIFFERENT_APP) {
      set_note("another worker runs");
    } else {
      set_note("worker launch failed");
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "worker launch result %d", (int)r);
  }
  render();
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
  set_raw(!s_raw_on);
}

static void down_click(ClickRecognizerRef rec, void *ctx) {
  s_app_taps = s_app_fires = s_app_resets = 0;
  s_worker_taps = s_worker_fires = 0;
  s_count = 0;
  s_last_gap_ms = 0;
  s_last_tap_ms = 0;
  s_last_axis = '-'; s_last_dir = ' ';
  if (s_window_timer) { app_timer_cancel(s_window_timer); s_window_timer = NULL; }
  set_note("counters zeroed");
  render();
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

// --- Window ---

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int inset = PBL_IF_ROUND_ELSE(20, 4);
  s_text = text_layer_create(GRect(inset, PBL_IF_ROUND_ELSE(22, 2),
                                   b.size.w - 2 * inset, b.size.h - 4));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_background_color(s_text, GColorClear);
  text_layer_set_text_color(s_text, GColorBlack);
  text_layer_set_text_alignment(s_text,
      PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  layer_add_child(root, text_layer_get_layer(s_text));
  render();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_text);
  s_text = NULL;
}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load, .unload = window_unload,
  });
  window_stack_push(s_window, true);

  accel_tap_service_subscribe(app_tap_handler);
  app_worker_message_subscribe(worker_message_handler);
  ping_worker();
}

static void deinit(void) {
  if (s_window_timer) app_timer_cancel(s_window_timer);
  if (s_flash_timer)  app_timer_cancel(s_flash_timer);
  set_raw(false);
  accel_tap_service_unsubscribe();
  app_worker_message_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
