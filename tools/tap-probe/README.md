# Tap Probe

A tiny sideloadable watchapp for finding out why a triple-tap does, or does
not, reach Pulse Time. It is a separate Pebble project in this folder with its
own UUID, so it installs alongside Pulse Time without touching it.

## What it shows

```
APP taps 12  X+ 412ms      taps the foreground app got; last axis/direction and gap
app triple 2/3  fired 3    Pulse Time's counter (3 taps, each within 800 ms) on those taps
WORKER running, linked     this app's own background worker
wkr taps 12  fired 3       taps and fires the worker saw, over the path Pulse Time uses
RAW off (UP)               optional peak accelerometer magnitude in mG
tap 2 of 3                 the last event
```

The screen flashes and the watch gives one short buzz when the app-side
counter fires. Taps in the 600 ms after that buzz are ignored and labelled
`vibe echo ignored`, because the motor can register as a tap.

Buttons: **SELECT** starts or stops the worker, **UP** toggles the raw
readout, **DOWN** zeroes the counters.

## Build and install

```bash
cd tools/tap-probe
pebble build
pebble install --phone <phone IP>
pebble logs --phone <phone IP>      # same events with timestamps
```

## Reading the result

1. Open Tap Probe, press SELECT so WORKER says `running, linked`, then
   triple-tap the way you do for Pulse Time.
2. **APP taps climb but WORKER taps stay at 0.** The firmware delivers taps to
   the foreground app but not to a background worker on this watch. That is a
   firmware behaviour Pulse Time cannot work around with a window change.
3. **Both climb, both fire.** The tap path works. Check that Pulse Time's own
   worker is actually running (its Worker row, or Settings > Background App);
   only one worker can run at a time, and starting Tap Probe's worker stops
   Pulse Time's.
4. **Neither climbs even though the backlight lights.** The backlight flick and
   the tap service are not the same detector on this firmware, and a harder
   or sharper knock is needed. Turn RAW on to see the magnitude a knock reaches.
5. **Taps climb but the counter keeps resetting.** The gaps are over 800 ms.
   The gap readout tells you by how much.

When you are done, stop Tap Probe's worker (SELECT) and restart Pulse Time's.
