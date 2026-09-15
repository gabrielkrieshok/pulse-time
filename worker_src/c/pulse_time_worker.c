/**
 * Pulse Time — Background Worker
 *
 * Runs behind any watchface. Listens for a double wrist-tap
 * and vibrates the current time in one of three modes:
 *
 *   TERSE:  5h groups + quarter-hours (Apple Watch-style)
 *   DIGITS: 10h/1h + 10m/1m (precise, every minute)
 *   MORSE:  Each digit of HH:MM in Morse code
 *
 * Optionally vibrates the hour at the top of every hour (chime).
 *
 * Settings (mode, vibe timing, chime) are read from persistent
 * storage, written by the foreground app.
 */

#include <pebble_worker.h>
#include "pulse_time.h"

// pebble_worker.h omits the Vibes API, but the symbols exist in libpebble.
// Declare them here so the worker can drive the vibration motor directly.
typedef struct {
  const uint32_t *durations;
  uint32_t num_segments;
} VibePattern;

void vibes_cancel(void);
void vibes_enqueue_custom_pattern(VibePattern pattern);

DEFINE_PRESETS  // expands the preset table

// --- Settings ---
static uint32_t s_vibe_long;
static uint32_t s_vibe_short;
static uint32_t s_gap_intra;
static uint32_t s_gap_inter;
static PulseTimeMode s_mode;
static bool s_chime_enabled;

// --- Double-tap detection ---
#define DOUBLE_TAP_WINDOW_MS 800  // second tap must arrive within this window
static AppTimer *s_tap_timer;
static bool s_awaiting_second_tap;

// --- Vibe group playback ---
#define MAX_GROUP_SEGMENTS 32
#define MAX_GROUPS 6  // morse can have 4 digits; terse/digits use 2

typedef struct {
  uint32_t segments[MAX_GROUP_SEGMENTS];
  int      count;
  uint32_t total_ms;
} VibeGroup;

static VibeGroup s_groups[MAX_GROUPS];
static int  s_num_groups;
static int  s_current_group;
static bool s_playing;
static AppTimer *s_play_timer;   // chains groups with real ms gaps

// ================================================================
//  Settings
// ================================================================

static uint32_t load_uint(uint32_t key, uint32_t fallback) {
  return persist_exists(key) ? (uint32_t)persist_read_int(key) : fallback;
}

static void chime_tick_handler(struct tm *tick_time, TimeUnits units_changed);

static void apply_chime_subscription(void) {
  // MINUTE_UNIT keeps the worker cheap; we act only when HOUR_UNIT flips.
  if (s_chime_enabled) {
    tick_timer_service_subscribe(MINUTE_UNIT, chime_tick_handler);
  } else {
    tick_timer_service_unsubscribe();
  }
}

static void load_settings(void) {
  int preset_idx = (int)load_uint(STORAGE_KEY_PRESET, 0);
  if (preset_idx < 0 || preset_idx >= NUM_PRESETS) preset_idx = 0;

  s_vibe_long  = load_uint(STORAGE_KEY_VIBE_LONG,  s_presets[preset_idx].vibe_long);
  s_vibe_short = load_uint(STORAGE_KEY_VIBE_SHORT,  s_presets[preset_idx].vibe_short);
  s_gap_intra  = load_uint(STORAGE_KEY_GAP_INTRA,   s_presets[preset_idx].gap_intra);
  s_gap_inter  = load_uint(STORAGE_KEY_GAP_INTER,   s_presets[preset_idx].gap_inter);

  int mode = (int)load_uint(STORAGE_KEY_MODE, 0);
  s_mode = (mode >= 0 && mode < MODE_COUNT) ? (PulseTimeMode)mode : MODE_TERSE;

  s_chime_enabled = load_uint(STORAGE_KEY_CHIME, 0) != 0;
  apply_chime_subscription();
}

// ================================================================
//  Group building helpers
// ================================================================

// Append a single vibe to a group
static void group_append(VibeGroup *g, uint32_t on_ms) {
  if (g->count >= MAX_GROUP_SEGMENTS - 1) return;
  // If there's already content, add intra gap first
  if (g->count > 0) {
    g->segments[g->count++] = s_gap_intra;
    g->total_ms += s_gap_intra;
  }
  g->segments[g->count++] = on_ms;
  g->total_ms += on_ms;
}

// Append N identical vibes
static void group_append_n(VibeGroup *g, int n, uint32_t on_ms) {
  for (int i = 0; i < n; i++) {
    group_append(g, on_ms);
  }
}

static void group_clear(VibeGroup *g) {
  g->count = 0;
  g->total_ms = 0;
}

// ================================================================
//  Morse code table for digits 0–9
//  Each digit has exactly 5 elements (dots and dashes).
//  false = dot (short), true = dash (long)
// ================================================================

static const bool MORSE_DIGITS[10][5] = {
  { true,  true,  true,  true,  true  },  // 0: -----
  { false, true,  true,  true,  true  },  // 1: .----
  { false, false, true,  true,  true  },  // 2: ..---
  { false, false, false, true,  true  },  // 3: ...--
  { false, false, false, false, true  },  // 4: ....-
  { false, false, false, false, false },  // 5: .....
  { true,  false, false, false, false },  // 6: -....
  { true,  true,  false, false, false },  // 7: --...
  { true,  true,  true,  false, false },  // 8: ---..
  { true,  true,  true,  true,  false },  // 9: ----.
};

// Build a morse group for a single digit
static void build_morse_digit(VibeGroup *g, int digit) {
  group_clear(g);
  if (digit < 0 || digit > 9) return;
  for (int i = 0; i < 5; i++) {
    group_append(g, MORSE_DIGITS[digit][i] ? s_vibe_long : s_vibe_short);
  }
}

// ================================================================
//  Mode encoders
//  Each fills s_groups / s_num_groups. Hours come first, so a
//  "hours only" playback is just the first hour group(s).
// ================================================================

// TERSE: Apple Watch-style
//   Group 0: long × (hour/5), short × (hour%5)
//   Group 1: long × (minute/15)
// Gives time to nearest 15 minutes.
static void encode_terse(int hour, int minute) {
  s_num_groups = 0;

  // Hours
  VibeGroup *gh = &s_groups[s_num_groups];
  group_clear(gh);
  int fives = hour / 5;
  int remain_h = hour % 5;
  group_append_n(gh, fives, s_vibe_long);
  group_append_n(gh, remain_h, s_vibe_short);
  if (gh->count > 0) s_num_groups++;

  // Quarter-hours
  int quarters = minute / 15;
  if (quarters > 0) {
    VibeGroup *gm = &s_groups[s_num_groups];
    group_clear(gm);
    group_append_n(gm, quarters, s_vibe_long);
    s_num_groups++;
  }
}

// DIGITS: Precise, every minute
//   Group 0: long × (hour/10), short × (hour%10)
//   Group 1: long × (minute/10), short × (minute%10)
static void encode_digits(int hour, int minute) {
  s_num_groups = 0;

  // Hours
  VibeGroup *gh = &s_groups[s_num_groups];
  group_clear(gh);
  int tens_h = hour / 10;
  int ones_h = hour % 10;
  group_append_n(gh, tens_h, s_vibe_long);
  group_append_n(gh, ones_h, s_vibe_short);
  if (gh->count > 0) s_num_groups++;

  // Minutes
  int tens_m = minute / 10;
  int ones_m = minute % 10;
  if (tens_m > 0 || ones_m > 0) {
    VibeGroup *gm = &s_groups[s_num_groups];
    group_clear(gm);
    group_append_n(gm, tens_m, s_vibe_long);
    group_append_n(gm, ones_m, s_vibe_short);
    if (gm->count > 0) s_num_groups++;
  }
}

// MORSE: Each digit of time as Morse code
//   e.g. 3:05 → 3, 0, 5   (leading hour zero suppressed)
//   12:00 → 1, 2, 0, 0
static void encode_morse(int hour, int minute) {
  s_num_groups = 0;

  // Hour digits
  if (hour >= 10) {
    build_morse_digit(&s_groups[s_num_groups++], hour / 10);
  }
  build_morse_digit(&s_groups[s_num_groups++], hour % 10);

  // Minute digits (always 2 digits, preserve leading zero)
  build_morse_digit(&s_groups[s_num_groups++], minute / 10);
  build_morse_digit(&s_groups[s_num_groups++], minute % 10);
}

// Number of leading groups that encode the hour, for the current mode.
static int hour_group_count(int hour) {
  switch (s_mode) {
    case MODE_MORSE: return (hour >= 10) ? 2 : 1;
    default:         return (hour > 0) ? 1 : 0;
  }
}

// ================================================================
//  Playback engine
//  Groups are chained with an AppTimer so the inter-group gap is
//  the real preset value (ms), not the ~1 s tick-timer granularity.
// ================================================================

static void stop_playback(void) {
  if (s_play_timer) {
    app_timer_cancel(s_play_timer);
    s_play_timer = NULL;
  }
  vibes_cancel();
  s_playing = false;
}

static void play_timer_handler(void *data);

static void play_group(int idx) {
  VibeGroup *g = &s_groups[idx];
  VibePattern pattern = {
    .durations = g->segments,
    .num_segments = g->count,
  };
  vibes_enqueue_custom_pattern(pattern);

  bool last = (idx + 1 >= s_num_groups);
  uint32_t wait_ms = g->total_ms + (last ? 0 : s_gap_inter);
  s_play_timer = app_timer_register(wait_ms, play_timer_handler, NULL);
}

static void play_timer_handler(void *data) {
  s_play_timer = NULL;
  if (!s_playing) return;

  s_current_group++;
  // Skip any empty groups
  while (s_current_group < s_num_groups && s_groups[s_current_group].count == 0) {
    s_current_group++;
  }
  if (s_current_group >= s_num_groups) {
    s_playing = false;   // done; taps are accepted again
    return;
  }
  play_group(s_current_group);
}

static void start_playback(void) {
  if (s_num_groups == 0) return;
  s_current_group = 0;
  while (s_current_group < s_num_groups && s_groups[s_current_group].count == 0) {
    s_current_group++;
  }
  if (s_current_group >= s_num_groups) return;
  s_playing = true;
  play_group(s_current_group);
}

static void get_hour_minute(int *hour, int *minute) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  if (clock_is_24h_style()) {
    *hour = t->tm_hour;          // 0–23
  } else {
    *hour = t->tm_hour % 12;
    if (*hour == 0) *hour = 12;  // 1–12
  }
  *minute = t->tm_min;
}

static void encode_time(int hour, int minute) {
  switch (s_mode) {
    case MODE_TERSE:  encode_terse(hour, minute);  break;
    case MODE_DIGITS: encode_digits(hour, minute); break;
    case MODE_MORSE:  encode_morse(hour, minute);  break;
    default:          encode_terse(hour, minute);  break;
  }
}

// Double-tap: vibrate the full current time
static void vibe_current_time(void) {
  load_settings();   // user may have changed them
  stop_playback();

  int hour, minute;
  get_hour_minute(&hour, &minute);
  encode_time(hour, minute);
  start_playback();
}

// Hourly chime: vibrate just the hour, in the current mode
static void vibe_current_hour(void) {
  stop_playback();

  int hour, minute;
  get_hour_minute(&hour, &minute);
  encode_time(hour, 0);
  s_num_groups = hour_group_count(hour);
  start_playback();
}

// ================================================================
//  Event handlers
// ================================================================

static void chime_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (!s_chime_enabled) return;
  if (!(units_changed & HOUR_UNIT)) return;
  if (s_playing) return;
  vibe_current_hour();
}

static void tap_timeout_handler(void *data) {
  // Window expired without a second tap — reset
  s_tap_timer = NULL;
  s_awaiting_second_tap = false;
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_playing) return;  // ignore taps during playback

  if (!s_awaiting_second_tap) {
    // First tap — start the window
    s_awaiting_second_tap = true;
    s_tap_timer = app_timer_register(DOUBLE_TAP_WINDOW_MS,
                                     tap_timeout_handler, NULL);
  } else {
    // Second tap — cancel the timer and fire
    if (s_tap_timer) {
      app_timer_cancel(s_tap_timer);
      s_tap_timer = NULL;
    }
    s_awaiting_second_tap = false;
    vibe_current_time();
  }
}

static void worker_message_handler(uint16_t type, AppWorkerMessage *msg) {
  if (type == MSG_KEY_TRIGGER) {
    vibe_current_time();
  } else if (type == MSG_KEY_SETTINGS) {
    load_settings();
  }
}

// ================================================================
//  Lifecycle
// ================================================================

static void prv_init(void) {
  load_settings();
  accel_tap_service_subscribe(accel_tap_handler);
  app_worker_message_subscribe(worker_message_handler);
}

static void prv_deinit(void) {
  stop_playback();
  if (s_tap_timer) app_timer_cancel(s_tap_timer);
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  app_worker_message_unsubscribe();
}

int main(void) {
  prv_init();
  worker_event_loop();
  prv_deinit();
}
