// Pulse Time — phone-side glue.
//
// The settings gear opens the hosted guide/config page. When the page closes
// it hands back JSON; we forward it to the watch app over AppMessage.
//
// Settings live on the watch (the source of truth) and are mirrored in
// localStorage so the page can open on the current values:
//   - when the watch app starts ('ready') we ask it for its settings (SYNC)
//   - a change made on the page while the watch app is closed is kept as
//     `pending` and delivered the next time the app opens.

var CONFIG_URL = 'https://gabrielkrieshok.github.io/pulse-time/';

var DEFAULTS = { mode: 0, preset: 0, chime: 0, quiet: 0, qstart: 22, qend: 7 };

function load() {
  var s = {};
  try { s = JSON.parse(localStorage.getItem('pulse_settings')) || {}; } catch (e) {}
  var out = {};
  for (var k in DEFAULTS) out[k] = (typeof s[k] === 'number') ? s[k] : DEFAULTS[k];
  return out;
}

function save(s) {
  localStorage.setItem('pulse_settings', JSON.stringify(s));
}

function toPayload(s) {
  return {
    MODE: s.mode, PRESET: s.preset, CHIME: s.chime,
    QUIET: s.quiet, QUIET_START: s.qstart, QUIET_END: s.qend
  };
}

var pending = null;  // AppMessage payload waiting for the watch app to be open

// `retry` is what to re-send next time the watch app opens if this fails
// (null = nothing; a practice buzz is never replayed late).
function send(payload, retry) {
  Pebble.sendAppMessage(payload,
    function () { if (retry) pending = null; },
    function () { if (retry) pending = retry; });
}

Pebble.addEventListener('ready', function () {
  if (pending) {
    send(pending, pending);           // phone-side change wins
  } else {
    send({ SYNC: 1 }, null);          // otherwise adopt the watch's settings
  }
});

// The watch replied to SYNC with its current settings.
Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload || {};
  if (typeof p.MODE === 'undefined') return;
  var s = load();
  s.mode = p.MODE | 0;
  s.preset = p.PRESET | 0;
  s.chime = p.CHIME ? 1 : 0;
  s.quiet = p.QUIET ? 1 : 0;
  if (typeof p.QUIET_START === 'number') s.qstart = p.QUIET_START;
  if (typeof p.QUIET_END === 'number') s.qend = p.QUIET_END;
  save(s);
});

Pebble.addEventListener('showConfiguration', function () {
  var s = load();
  var url = CONFIG_URL + '?mode=' + s.mode + '&preset=' + s.preset +
            '&chime=' + s.chime + '&quiet=' + s.quiet +
            '&qs=' + s.qstart + '&qe=' + s.qend;
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;  // cancelled
  var r;
  try { r = JSON.parse(decodeURIComponent(e.response)); } catch (err) { return; }

  var s = {
    mode:   r.mode   | 0,
    preset: r.preset | 0,
    chime:  r.chime ? 1 : 0,
    quiet:  r.quiet ? 1 : 0,
    qstart: ((r.qstart | 0) % 24 + 24) % 24,
    qend:   ((r.qend   | 0) % 24 + 24) % 24
  };
  save(s);

  var payload = toPayload(s);
  if (r.practice && typeof r.practice.hour === 'number') {
    payload.PRACTICE_HOUR = r.practice.hour;
    payload.PRACTICE_MINUTE = r.practice.minute;
  }
  send(payload, toPayload(s));
});
