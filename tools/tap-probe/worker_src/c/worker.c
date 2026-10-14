/**
 * Tap Probe — background worker.
 *
 * Subscribes to the same accel_tap_service Pulse Time's worker uses, runs the
 * same triple-tap counter (3 taps, each within 800 ms), and reports every tap
 * and every fire to the foreground app. It does not vibrate, so nothing here
 * can echo back into the tap service.
 */

#include <pebble_worker.h>

// Message types shared with src/c/main.c (keep both copies equal)
#define MSG_WORKER_ALIVE  0
#define MSG_WORKER_TAP    1
#define MSG_WORKER_FIRE   2
#define MSG_APP_PING      3

#define TAPS_REQUIRED  3
#define TAP_WINDOW_MS  800

static AppTimer *s_timer;
static int       s_count;

static void send(uint16_t type, uint16_t d0, uint16_t d1) {
  AppWorkerMessage m = { .data0 = d0, .data1 = d1, .data2 = 0 };
  app_worker_send_message(type, &m);   // dropped silently if the app is closed
}

static void timeout(void *data) {
  s_timer = NULL;
  s_count = 0;
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  send(MSG_WORKER_TAP, (uint16_t)axis, direction > 0 ? 1 : 0);

  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  s_count++;
  if (s_count >= TAPS_REQUIRED) {
    s_count = 0;
    send(MSG_WORKER_FIRE, 0, 0);
  } else {
    s_timer = app_timer_register(TAP_WINDOW_MS, timeout, NULL);
  }
}

static void message_handler(uint16_t type, AppWorkerMessage *msg) {
  if (type == MSG_APP_PING) send(MSG_WORKER_ALIVE, 0, 0);
}

int main(void) {
  accel_tap_service_subscribe(tap_handler);
  app_worker_message_subscribe(message_handler);
  send(MSG_WORKER_ALIVE, 0, 0);
  worker_event_loop();
  if (s_timer) app_timer_cancel(s_timer);
  accel_tap_service_unsubscribe();
  app_worker_message_unsubscribe();
}
