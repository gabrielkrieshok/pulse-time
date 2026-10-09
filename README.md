# Pulse Time

Feel the current time through vibration patterns on your Pebble smartwatch — no
need to look at your wrist. Pulse Time runs as a **background worker** behind
any watchface, so it's always available. Just triple-tap your wrist and the
watch vibrates the current time.

**Triple-tap your wrist, feel the time.**

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

1. You **triple-tap your wrist** — three deliberate knocks, each within about
   0.8 seconds of the last (a single bump from walking or gesturing won't trigger it)
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
| 8     | `---..`  | long long long short short           |
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

1. Install Pulse Time from the [Rebble App Store](https://apps.rebble.io/en_US/application/6ac931717c7c2000098af597), or sideload the
   `.pbw` from the [latest release](https://github.com/gabrielkrieshok/pulse-time/releases/latest)
2. Open **Pulse Time** from the app launcher
3. Press **SELECT** (middle button) to start the background worker
4. The status indicator turns green and shows **RUNNING**
5. Press **BACK** to return to your watchface

That's it. The worker now runs in the background behind whatever watchface
you use. **Triple-tap your wrist** at any time to feel the time.

### Controls

The app opens to a menu. **UP/DOWN** move the highlight; **SELECT** acts on
the row.

| Row                | SELECT does                                          |
|--------------------|------------------------------------------------------|
| **Worker**         | Start or stop the background worker                  |
| **Mode**           | Cycle Terse &rarr; Digits &rarr; Morse               |
| **Vibe**           | Cycle Standard &rarr; Gentle &rarr; Strong &rarr; Learn |
| **Hourly chime**   | Toggle the on-the-hour buzz                          |
| **Test buzz**      | Play the current time now (worker must be running)   |
| **How to read it** | Points you to the guide in the phone settings        |

**BACK** exits the app; the worker keeps running.

When you change a setting, the watch gives a haptic confirmation so you
can tell it took without looking: one short pulse for a mode change, two
short pulses for a preset change, one long pulse for toggling the chime.

### Phone settings & pattern guide

In the Pebble phone app, open **Pulse Time** and tap the settings gear. The
page there explains how to read each mode and has a **pattern player**: pick a
time, see the long/short pulses drawn out, hear them with the real vibe
timings, or choose **Buzz it on my watch** to feel exactly that time. It also
sets mode, vibe and chime.

The page also lets you set **quiet hours** for the hourly chime (default 22:00
&ndash; 07:00 once switched on). The chime stays silent between those hours.

Notes: the page is hosted on GitHub Pages (`docs/index.html`), so it needs a
connection to load. "Buzz it on my watch" saves and closes the page, then
plays the time, starting the worker if it isn't running. When the watch app opens
it sends its current settings to the phone, so the page always opens on what the
watch actually has. A change saved while the watch app is closed is delivered the
next time it opens.

### Day-to-Day Use

Once configured, you never need to open the app again. From any watchface:

- **Triple-tap your wrist** — three deliberate knocks — and the time is
  vibrated in the mode you chose
- If you tap while a pattern is already playing, it's ignored (debounce)
- With the **hourly chime** on, the watch vibrates just the hour (in your
  current mode) at the top of every hour, so you can keep loose track of
  time without ever tapping
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
| **Inter gap** | Pause between groups (ms)                          |

| Preset       | Long  | Short | Intra gap | Inter gap | Character                          |
|--------------|-------|-------|-----------|-----------|------------------------------------|
| **Standard** | 400ms | 120ms | 100ms     | 500ms     | Balanced, easy to count            |
| **Gentle**   | 300ms | 80ms  | 120ms     | 600ms     | Subtler, easier on battery         |
| **Strong**   | 500ms | 150ms | 80ms      | 450ms     | Very distinct, harder to miss      |
| **Learn**    | 600ms | 200ms | 250ms     | 1200ms    | Slow and spacious, for learning    |

Start on **Learn** while the patterns are new, then move to Standard once
you can read them without thinking.

---

## Architecture

```
+------------------------------+
|   Foreground App (src/c/)    |  <- Open from launcher to configure
|                              |
|  Menu: worker, mode, vibe,   |
|  chime, test buzz, help      |
|  AppMessage <- phone page    |
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
|  (triple-tap, 800 ms apart)  |
|  Reads mode + settings from  |
|  persistent storage          |
|                              |
|  Encodes time as vibe groups |
|  Plays groups sequentially,  |
|  chained via AppTimer        |
|                              |
|  Optional hourly chime via   |
|  TickTimerService (HOUR_UNIT)|
+------------------------------+
```

**Communication:** The foreground app writes settings to Pebble's persistent
storage. When the user changes a setting, the app also sends an
`AppWorkerMessage` to tell the worker to reload immediately (rather than
waiting for the next tap).

**Playback engine:** Each mode encodes the time into 1-6 "vibe groups."
Each group is an array of alternating on/off durations fed to the Pebble
`vibes_enqueue_custom_pattern()` API. The worker plays the first group
immediately, then chains the remaining groups with an `AppTimer` set to the
group's duration plus the preset's inter-group gap, so the pauses are the
real millisecond values from the preset table.

**Hourly chime:** When enabled (and outside quiet hours), the worker subscribes to the tick timer at
`MINUTE_UNIT` and, whenever the `HOUR_UNIT` flag flips, plays only the hour
group(s) of the current mode. Off by default.

**Debouncing:** If a tap arrives while a pattern is still playing, it's
silently ignored.

## Project Structure

```
pulse-time/
├── package.json                    # Pebble project manifest
├── wscript                         # Build config (waf)
├── build.sh                        # Build helper (syncs headers, wraps pebble build)
├── docs/index.html                 # Hosted settings page + pattern player (GitHub Pages)
├── src/pkjs/index.js               # Phone-side glue: opens the page, relays settings
├── src/c/
│   ├── pulse_time.h                # Shared definitions (storage keys, presets, modes)
│   └── main.c                      # Foreground menu app
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
- **Use Test buzz** (in the app, or the pattern player on the phone settings page) to practice at
  known times until the patterns become second nature.
- **Use the Learn preset first.** Everything is slower and the gaps are
  wider, so individual vibes and group boundaries are easy to pick out.
  Switch to Standard once you stop having to count.
- **Turn on the hourly chime.** Feeling the hour once an hour, at a moment
  you can check against a clock, is painless repetition.
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
  threshold, and Pulse Time additionally requires three taps, each within 800 ms of the
  last. A deliberate triple knock triggers it reliably; a single bump from
  walking or gesturing does not.
- **Hourly chime and battery:** The chime keeps a once-a-minute tick
  subscription alive in the worker. That's cheap, but it's still more than
  nothing — leave it off if you're squeezing every hour out of a charge.
- **Midnight in 24-hour mode:** Hour 0 is **short, long** in Terse and Digits
  modes. Every real hour group puts its longs first, so that reversed pair can't
  be mistaken for one. Morse mode plays `-----`.
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

## Project links

- Rebble App Store: https://apps.rebble.io/en_US/application/6ac931717c7c2000098af597
- Code and issues: https://github.com/gabrielkrieshok/pulse-time
- Rebble developer docs: https://developer.rebble.io/

## License

MIT
