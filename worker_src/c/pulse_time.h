/**
 * Pulse Time — Shared Definitions
 *
 * Constants shared between the foreground app and background worker.
 * Both files include this header so storage keys and message types
 * stay in sync.
 */

#pragma once

#include <stdint.h>

// --- Persistent storage keys ---
#define STORAGE_KEY_VIBE_LONG    0x10
#define STORAGE_KEY_VIBE_SHORT   0x11
#define STORAGE_KEY_GAP_INTRA    0x12
#define STORAGE_KEY_GAP_INTER    0x13
#define STORAGE_KEY_PRESET       0x14
#define STORAGE_KEY_MODE         0x15

// --- AppWorkerMessage types ---
#define MSG_KEY_TRIGGER   0
#define MSG_KEY_SETTINGS  1

// --- Time-telling modes ---
typedef enum {
  MODE_TERSE  = 0,   // Apple-style: 5h groups + quarter-hours
  MODE_DIGITS = 1,   // Precise: 10h/1h groups + 10m/1m groups
  MODE_MORSE  = 2,   // Each digit in Morse code
  MODE_COUNT  = 3,
} PulseTimeMode;

// --- Vibe presets ---
typedef struct {
  const char *name;
  uint32_t vibe_long;    // ms — dash / "big" unit
  uint32_t vibe_short;   // ms — dot / "small" unit
  uint32_t gap_intra;    // ms — between vibes within a group
  uint32_t gap_inter;    // ms — between groups
} VibePreset;

// Preset table (defined identically in both compilation units
// to avoid linker issues with the separate worker build)
#define NUM_PRESETS 3

#define DEFINE_PRESETS \
  static const VibePreset s_presets[NUM_PRESETS] = { \
    { \
      .name       = "Standard", \
      .vibe_long  = 400, \
      .vibe_short = 120, \
      .gap_intra  = 100, \
      .gap_inter  = 500, \
    }, \
    { \
      .name       = "Gentle", \
      .vibe_long  = 300, \
      .vibe_short = 80, \
      .gap_intra  = 120, \
      .gap_inter  = 600, \
    }, \
    { \
      .name       = "Strong", \
      .vibe_long  = 500, \
      .vibe_short = 150, \
      .gap_intra  = 80, \
      .gap_inter  = 450, \
    }, \
  };

// Mode names for UI
static inline const char* mode_name(PulseTimeMode mode) {
  switch (mode) {
    case MODE_TERSE:  return "Terse";
    case MODE_DIGITS: return "Digits";
    case MODE_MORSE:  return "Morse";
    default:          return "?";
  }
}
