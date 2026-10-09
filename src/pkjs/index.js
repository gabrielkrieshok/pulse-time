// Pulse Time — phone-side glue.
//
// The settings gear opens the hosted guide/config page. When the page closes
// it hands back JSON; we forward it to the watch app over AppMessage.
// Settings are also kept in localStorage so the page can start from the
// current values and so a change made while the watch app is closed is
// delivered the next time the app opens ('ready').

var CONFIG_URL = 'https://gabrielkrieshok.github.io/pulse-time/';

var DEFAULTS = { mode: 0, preset: 0, chime: 0 };

function load() {
  try {
    var s = JSON.parse(localStorage.getItem('pulse_settings'));
    if (s) return s;
  } catch (e) {}
  return { mode: DEFAULTS.mode, preset: DEFAULTS.preset, chime: DEFAULTS.chime };
}

function save(s) {
  localStorage.setItem('pulse_settings', JSON.stringify(s));
}

var pending = null;  // AppMessage payload waiting for the watch app to be open

function send(payload) {
  Pebble.sendAppMessage(payload,
    function () { pending = null; },
    function () { pending = payload; });
}

Pebble.addEventListener('ready', function () {
  if (pending) send(pending);
});

Pebble.addEventListener('showConfiguration', function () {
  var s = load();
  var url = CONFIG_URL + '?mode=' + s.mode + '&preset=' + s.preset + '&chime=' + s.chime;
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;  // cancelled
  var r;
  try { r = JSON.parse(decodeURIComponent(e.response)); } catch (err) { return; }

  var s = {
    mode:   r.mode   | 0,
    preset: r.preset | 0,
    chime:  r.chime ? 1 : 0
  };
  save(s);

  var payload = { MODE: s.mode, PRESET: s.preset, CHIME: s.chime };
  if (r.practice && typeof r.practice.hour === 'number') {
    payload.PRACTICE_HOUR = r.practice.hour;
    payload.PRACTICE_MINUTE = r.practice.minute;
  }
  send(payload);
});
