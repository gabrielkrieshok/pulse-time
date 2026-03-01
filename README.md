# Pulse Time

Feel the current time through vibration patterns on your Pebble smartwatch — no
need to look at your wrist. Pulse Time runs as a **background worker** behind
any watchface, so it's always available. Just tap your wrist and the watch
vibrates the current time.

**Tap your wrist, feel the time.**

## Why?

Checking your watch in a meeting, on a date, or while talking to someone can
feel rude. Pulse Time lets you discreetly know the time through vibration
patterns you can learn to read by feel. It works in the dark, under water, and
while your hands are full.

## How It Works

Pulse Time encodes the current time into a sequence of **long** and **short**
vibrations, separated by pauses. It automatically uses your Pebble's clock
format — 12-hour or 24-hour — based on your system setting. There are three
modes that trade off simplicity against precision.

Every mode works the same way:

1. You **tap your wrist** (a deliberate flick, not just walking motion)
2. The watch vibrates one or more **groups** of pulses, separated by longer pauses
3. You decode the groups to read the time

The first group always represents **hours**. Subsequent groups represent
**minutes** (or minute digits in Morse mode).

---

## Modes

### Terse (Default)

Inspired by the Apple Watch's Taptic Time. Gives the time to the nearest
**15 minutes** using the fewest possible vibrations. Great once you've learned
the pattern — you can tell the time in under 2 seconds.

**Encoding:**
- **Group 1 (Hours):** Long vibes for each 5-hour block, then short vibes
  for the remainder.
- **Group 2 (Quarter-hours):** Long vibes for each quarter past the hour
  (0-3). Omitted if the time is in the first quarter (`:00`-`:14`).

| Vibe   | Meaning            |
|--------|--------------------|
| Long   | 5 hours            |
| Short  | 1 hour (remainder) |
| *pause* |                   |
| Long   | 15 minutes         |

**Examples:**

| Time     | Group 1 (Hours)                     | Group 2 (Quarters)   | What you feel                              |
|----------|-------------------------------------|----------------------|--------------------------------------------|
| 3:00     | short short short                   | *(none)*             | `...`                                      |
| 5:10     | LONG                                | *(none)*             | `===`                                      |
| 7:20     | LONG short short                    | LONG                 | `=== . .` pause `===`                      |
| 9:35     | LONG short short short short        | LONG LONG            | `=== . . . .` pause `=== ===`              |
| 12:50    | LONG LONG short short               | LONG LONG LONG       | `=== === . .` pause `=== === ===`          |

### Digits

The most precise mode. Gives the time to the **exact minute** by encoding
the tens and ones place of each component separately.

**Encoding:**
- **Group 1 (Hours):** Long vibes for the tens digit, then short vibes for
  the ones digit. For hours 1-9 there are no long vibes.
- **Group 2 (Minutes):** Same pattern — long vibes for tens, short for ones.
  Omitted if the time is exactly on the hour (`:00`).

| Vibe   | Meaning               |
|--------|-----------------------|
| Long   | 10 hours / 10 minutes |
| Short  | 1 hour / 1 minute     |

**Examples:**

| Time   | Group 1 (Hours)       | Group 2 (Minutes)                     | What you feel                               |
|--------|-----------------------|---------------------------------------|---------------------------------------------|
| 3:00   | short short short     | *(none)*                              | `...`                                       |
| 3:05   | short short short     | short short short short short         | `...` pause `. . . . .`                     |
| 7:30   | short short short short short short short | LONG LONG LONG        | `. . . . . . .` pause `=== === ===`         |
| 12:47  | LONG short short      | LONG LONG LONG LONG short short short short short short short | `=== . .` pause `=== === === === . . . . . . .` |

### Morse Code

Each digit of the time is vibrated as its
[Morse code](https://en.wikipedia.org/wiki/Morse_code) representation.
Long vibe = dash (`-`), short vibe = dot (`.`). Leading hour zeros are
suppressed (3:05 is three groups, not four).

Each digit is always exactly **5 elements** (dots and dashes), which makes
the rhythm predictable.

| Digit | Morse    | Pattern                              |
|-------|----------|--------------------------------------|
| 0     | `-----`  | long long long long long             |
| 1     | `.----`  | short long long long long            |
| 2     | `..---`  | short short long long long           |
| 3     | `...--`  | short short short long long          |
| 4     | `....-`  | short short short short long         |
| 5     | `.....`  | short short short short short        |
| 6     | `-....`  | long short short short short         |
| 7     | `--...`  | long long short short short          |
| 8     | `---..'  | long long long short short           |
| 9     | `----.`  | long long long long short            |

**Examples:**

| Time   | Digits        | Groups                                                   |
|--------|---------------|----------------------------------------------------------|
| 3:05   | 3, 0, 5       | `...--` pause `-----` pause `.....`                      |
| 7:20   | 7, 2, 0       | `--...` pause `..---` pause `-----`                      |
| 12:07  | 1, 2, 0, 7    | `.----` pause `..---` pause `-----` pause `--...`        |
| 12:59  | 1, 2, 5, 9    | `.----` pause `..---` pause `.....` pause `----.`        |

---

## Using Pulse Time

### First-Time Setup

1. Install the `.pbw` file on your Pebble
2. Open **Pulse Time** from the app launcher
3. Press **SELECT** (middle button) to start the background worker
4. The status indicator turns green and shows **RUNNING**
5. Press **BACK** to return to your watchface

That's it. The worker now runs in the background behind whatever watchface
you use. **Tap your wrist** at any time to feel the time.

### Controls

| Button              | Action                                         |
|---------------------|-------------------------------------------------|
| **SELECT** (press)  | Start or stop the background worker             |
| **SELECT** (hold)   | Trigger a test vibe (useful while configuring)   |
| **UP**              | Cycle mode: Terse &rarr; Digits &rarr; Morse    |
| **DOWN**            | Cycle preset: Standard &rarr; Gentle &rarr; Strong |
| **BACK**            | Exit the app (worker keeps running)              |

When you press UP or DOWN, the watch gives a short haptic confirmation
(single pulse for mode change, double pulse for preset change) so you
can tell the setting changed without looking.

### Day-to-Day Use

Once configured, you never need to open the app again. From any watchface:

- **Tap your wrist** — a deliberate flick or knock — and the time is
  vibrated in the mode you chose
- If you tap while a pattern is already playing, it's ignored (debounce)
- Settings persist across reboots; the worker restarts automatically

You can find the worker under **Settings &rarr; Background App** on your
Pebble to confirm it's running or to stop it.

---

## Presets

Presets control the vibration timing. Each preset defines four values:

| Parameter     | What it controls                                   |
|---------------|----------------------------------------------------|
| **Long vibe** | Duration of a long/dash vibration (ms)              |
| **Short vibe**| Duration of a short/dot vibration (ms)              |
| **Intra gap** | Pause between vibrations *within* a group (ms)      |
| **Inter gap** | Pause between groups (implemented via tick timer)    |

| Preset     | Long  | Short | Intra gap | Inter gap | Character                     |
|------------|-------|-------|-----------|-----------|-------------------------------|
| **Standard** | 400ms | 120ms | 100ms    | 500ms     | Balanced, easy to count       |
| **Gentle**   | 300ms | 80ms  | 120ms    | 600ms     | Subtler, easier on battery    |
| **Strong**   | 500ms | 150ms | 80ms     | 450ms     | Very distinct, harder to miss |

The inter-group pause uses the Pebble's `SECOND_UNIT` tick timer, so it's
roughly 1 second in practice (the minimum resolution available to workers).

---

## Architecture

```
+------------------------------+
|   Foreground App (src/c/)    |  <- Open from launcher to configure
|                              |
|  SELECT = start/stop worker  |
|  UP     = cycle mode         |
|  DOWN   = cycle preset       |
|  HOLD SELECT = test vibe     |
|                              |
|  Persists settings to        |
|  Persistent Storage API      |
+----------+-------------------+
           | AppWorkerMessage
           v
+------------------------------+
|  Background Worker           |  <- Runs behind any watchface
|  (worker_src/c/)             |
|                              |
|  Listens for accel taps      |
|  Reads mode + settings from  |
|  persistent storage          |
|                              |
|  Encodes time as vibe groups |
|  Plays groups sequentially,  |
|  chained via TickTimerService |
+------------------------------+
```

**Communication:** The foreground app writes settings to Pebble's persistent
storage. When the user changes a setting, the app also sends an
`AppWorkerMessage` to tell the worker to reload immediately (rather than
waiting for the next tap).

**Playback engine:** Each mode encodes the time into 1-6 "vibe groups."
Each group is an array of alternating on/off durations fed to the Pebble
`vibes_enqueue_custom_pattern()` API. The worker plays the first group
immediately, then uses a `SECOND_UNIT` tick timer to chain remaining groups
with pauses in between.

**Debouncing:** If a tap arrives while a pattern is still playing, it's
silently ignored.

## Project Structure

```
pulse-time/
├── package.json                    # Pebble project manifest
├── wscript                         # Build config (waf)
├── build.sh                        # Build helper (syncs headers, wraps pebble build)
├── src/c/
│   ├── pulse_time.h                # Shared definitions (storage keys, presets, modes)
│   └── main.c                      # Foreground control-panel app
└── worker_src/c/
    ├── pulse_time.h                # Copy of shared header (see note below)
    └── pulse_time_worker.c         # Background worker (tap -> encode -> vibe)
```

**Why is the header duplicated?** The Pebble SDK compiles `src/c/` and
`worker_src/c/` as completely separate compilation units with separate include
paths. There's no way to share a header across both without duplicating it.
The `build.sh` script automatically syncs the header before each build so
they never drift out of sync.

## Building

Requires the [Pebble SDK](https://developer.rebble.io/developer.pebble.com/sdk/index.html)
(available via [Rebble](https://rebble.io)).

```bash
# Build only
./build.sh

# Build and install to emulator
./build.sh install            # defaults to basalt
./build.sh install chalk      # specify platform

# Build and install to phone
./build.sh phone 192.168.4.198

# Build, install, and tail logs
./build.sh logs 192.168.1.42
```

Or use the Pebble SDK directly:

```bash
pebble build
pebble install --emulator basalt
pebble install --phone 192.168.1.42
```

## Tips for Learning the Patterns

- **Start with Terse mode.** It uses the fewest vibes and is the fastest to
  interpret. Most of the time you just need "it's about 3:30" rather than
  "it's 3:27."
- **Count the longs first, then the shorts.** Longs are the "big" units
  (5 hours in Terse, 10s digit in Digits), shorts are the remainder.
- **Use the test vibe** (long-press SELECT in the app) to practice at
  known times until the patterns become second nature.
- **Try Gentle preset first.** The wider intra-gap (120ms) makes individual
  vibes easier to distinguish while you're learning.
- **Morse mode** is the hardest to learn but the most precise at
  minute-level. It helps if you already know Morse code for digits.

## Notes

- **Battery:** Occasional taps (a few per hour) have negligible impact.
  The vibration motor draws significant current while active, so avoid
  automated or rapid repeated vibrations.
- **One worker at a time:** Pebble only allows a single background worker.
  If another app's worker is running, you'll be prompted to choose which
  to keep.
- **Tap sensitivity:** The accelerometer tap detection has a built-in
  threshold. A deliberate wrist tap or knock triggers it reliably; normal
  walking and gesturing generally do not.
- **Group chaining:** The worker uses `SECOND_UNIT` tick timer to chain
  vibe groups, giving roughly 1-second granularity on inter-group pauses.
  This is a pragmatic tradeoff for the worker's limited memory budget
  (10.5 kB).
- **Clock format:** Pulse Time follows your Pebble's system clock setting.
  In 12-hour mode, midnight and noon are both represented as 12. In 24-hour
  mode, hours range from 0 to 23.
- **Settings persist:** Mode, preset, and vibe timing are saved to
  persistent storage and survive reboots. The worker re-reads settings
  from storage on every tap, so changes made in the foreground app take
  effect immediately.

## Compatibility

All Pebble hardware platforms: Aplite (original Pebble), Basalt (Pebble
Time), Chalk (Pebble Time Round), Diorite (Pebble 2), and Emery
(Pebble Time 2).

Works with original Pebble watches via [Rebble](https://rebble.io) and
the Pebble 2 Duo / Pebble Time 2 from Core Devices.

## License

MIT
