/**
 * Pulse Time — Background Worker
 *
 * Runs behind any watchface. Listens for accelerometer taps
 * and vibrates the current time in one of three modes:
 *
 *   TERSE:  5h groups + quarter-hours (Apple Watch-style)
 *   DIGITS: 10h/1h + 10m/1m (precise, every minute)
 *   MORSE:  Each digit of HH:MM in Morse code
 *
 * Settings (mode + vibe durations) are read from persistent storage,
 * written by the foreground app.
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
static int  s_ticks_remaining;

// ================================================================
//  Settings
// ================================================================

static uint32_t load_uint(uint32_t key, uint32_t fallback) {
  return persist_exists(key) ? (uint32_t)persist_read_int(key) : fallback;
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
// ================================================================

// TERSE: Apple Watch-style
//   Group 0: long × (hour/5), short × (hour%5)
//   Group 1: long × (minute/15)
// 12-hour format. Gives time to nearest 15 minutes.
static void encode_terse(int hour12, int minute) {
  s_num_groups = 0;

  // Hours
  VibeGroup *gh = &s_groups[s_num_groups];
  group_clear(gh);
  int fives = hour12 / 5;
  int remain_h = hour12 % 5;
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
// 12-hour format.
static void encode_digits(int hour12, int minute) {
  s_num_groups = 0;

  // Hours
  VibeGroup *gh = &s_groups[s_num_groups];
  group_clear(gh);
  int tens_h = hour12 / 10;
  int ones_h = hour12 % 10;
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
//   Digits extracted from 12-hour time, e.g. 3:05 → 3, 0, 5
//   Leading zero on hours suppressed: 3:05 is "3", "0", "5"
//   12:00 is "1", "2", "0", "0"
static void encode_morse(int hour12, int minute) {
  s_num_groups = 0;

  // Hour digits
  if (hour12 >= 10) {
    build_morse_digit(&s_groups[s_num_groups++], hour12 / 10);
  }
  build_morse_digit(&s_groups[s_num_groups++], hour12 % 10);

  // Minute digits (always 2 digits, preserve leading zero)
  build_morse_digit(&s_groups[s_num_groups++], minute / 10);
  build_morse_digit(&s_groups[s_num_groups++], minute % 10);
}

// ================================================================
//  Playback engine
// ================================================================

static void play_group(int idx) {
  if (idx >= s_num_groups || s_groups[idx].count == 0) {
    s_playing = false;
    tick_timer_service_unsubscribe();
    return;
  }
  VibeGroup *g = &s_groups[idx];
  VibePattern pattern = {
    .durations = g->segments,
    .num_segments = g->count,
  };
  vibes_enqueue_custom_pattern(pattern);
}

static void worker_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (!s_playing) {
    tick_timer_service_unsubscribe();
    return;
  }

  if (s_ticks_remaining > 0) {
    s_ticks_remaining--;
    return;
  }

  s_current_group++;
  if (s_current_group >= s_num_groups) {
    s_playing = false;
    tick_timer_service_unsubscribe();
    return;
  }

  if (s_groups[s_current_group].count == 0) {
    s_ticks_remaining = 0;
    return;
  }

  play_group(s_current_group);

  uint32_t dur = s_groups[s_current_group].total_ms;
  s_ticks_remaining = (dur / 1000) + 1;
}

static void vibe_current_time(void) {
  // Reload settings (user may have changed them)
  load_settings();

  // Cancel any in-progress playback
  vibes_cancel();
  s_playing = false;
  tick_timer_service_unsubscribe();

  // Get current time
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int hour;
  if (clock_is_24h_style()) {
    hour = t->tm_hour;  // 0–23
  } else {
    hour = t->tm_hour % 12;
    if (hour == 0) hour = 12;  // 1–12
  }
  int minute = t->tm_min;

  // Encode time for the selected mode
  switch (s_mode) {
    case MODE_TERSE:  encode_terse(hour, minute);  break;
    case MODE_DIGITS: encode_digits(hour, minute); break;
    case MODE_MORSE:  encode_morse(hour, minute);  break;
    default:          encode_terse(hour, minute);  break;
  }

  if (s_num_groups == 0) return;

  // Play first group
  s_current_group = 0;
  play_group(0);

  // Chain remaining groups via tick timer
  if (s_num_groups > 1) {
    s_playing = true;
    uint32_t dur = s_groups[0].total_ms;
    s_ticks_remaining = (dur / 1000) + 1;
    tick_timer_service_subscribe(SECOND_UNIT, worker_tick_handler);
  }
}

// ================================================================
//  Event handlers
// ================================================================

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
  vibes_cancel();
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  app_worker_message_unsubscribe();
}

int main(void) {
  prv_init();
  worker_event_loop();
  prv_deinit();
}
