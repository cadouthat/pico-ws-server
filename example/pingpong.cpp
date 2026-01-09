// ---------------------------------------------------------------------------
// Minimal ping/pong demo for pico-ws-server
//
// What it does:
// - Connects to Wi‑Fi (STA mode) using WIFI_SSID / WIFI_PASSWORD (set in build)
// - Starts a WebSocket server on port 8088
// - Sends a PING every 5 seconds
// - Logs PONGs and resets a simple missed counter
// - Closes the connection after 3 consecutive missed PONGs or failed PING sends
//
// How to use:
// 1) Define WIFI_SSID and WIFI_PASSWORD (e.g., via CMake or compiler flags).
// 2) Build and flash to a Pico W.
// 3) Connect a WebSocket client to ws://<device-ip>:8088/ (any path).
// 4) Watch the serial console for PING/PONG logs and close events.
//
// This is intentionally minimal: one client at a time, no buffering, no reconnection.
// ---------------------------------------------------------------------------

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "pico_ws_server/web_socket_server.h"

// Simple ping/pong demo: send a ping every 5s, log pongs, and close after 3 misses.
// This is intentionally minimal—no reconnection or buffering.

static constexpr uint32_t PING_INTERVAL_MS = 5000;
static constexpr uint8_t MAX_MISSED_PONGS = 3;

static absolute_time_t next_ping_deadline;
static uint8_t pending_pings = 0;
static uint8_t missed_pongs = 0;
static uint32_t last_conn_id = 0;

static void reset_ping_state() {
  pending_pings = 0;
  missed_pongs = 0;
  next_ping_deadline = make_timeout_time_ms(PING_INTERVAL_MS);
}

static void on_connect(WebSocketServer& server, uint32_t conn_id) {
  last_conn_id = conn_id;
  reset_ping_state();
  printf("WS connected (id=%u)\n", conn_id);
}

static void on_disconnect(WebSocketServer& server, uint32_t conn_id) {
  printf("WS closed (id=%u)\n", conn_id);
  if (conn_id == last_conn_id) {
    last_conn_id = 0;
    reset_ping_state();
  }
}

static void on_message(WebSocketServer& server, uint32_t conn_id, const void* data, size_t len) {
  (void)server;
  printf("WS message (%u bytes): %.*s\n", (unsigned)len, (int)len, (const char*)data);
}

static void on_pong(WebSocketServer& server, uint32_t conn_id, const void* data, size_t len) {
  (void)server;
  (void)data;
  (void)len;
  if (conn_id != last_conn_id) {
    return;
  }
  reset_ping_state();
  printf("WS PONG (id=%u)\n", conn_id);
}

static void maybe_send_ping(WebSocketServer& server) {
  if (last_conn_id == 0) {
    return;
  }

  absolute_time_t now = get_absolute_time();
  if (absolute_time_diff_us(now, next_ping_deadline) > 0) {
    return; // not time yet
  }

  if (pending_pings > 0) {
    ++missed_pongs;
    if (missed_pongs > MAX_MISSED_PONGS) {
      printf("WS closing after %u missed pongs (id=%u)\n", missed_pongs, last_conn_id);
      server.close(last_conn_id);
      last_conn_id = 0;
      reset_ping_state();
      return;
    }
  }

  if (server.sendPing(last_conn_id, nullptr, 0)) {
    ++pending_pings;
    printf("WS sent PING (pending=%u, missed=%u)\n", pending_pings, missed_pongs);
  } else {
    ++missed_pongs;
    printf("WS PING send failed (missed=%u)\n", missed_pongs);
  }

  next_ping_deadline = delayed_by_ms(now, PING_INTERVAL_MS);
}

int main() {
  stdio_init_all();

  if (cyw43_arch_init() != 0) {
    printf("cyw43_arch_init failed\n");
    while (1) tight_loop_contents();
  }

  cyw43_arch_enable_sta_mode();

  printf("Connecting to Wi-Fi...\n");
  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
    printf("failed to connect.\n");
    while (1) tight_loop_contents();
  }
  printf("Connected.\n");

  WebSocketServer server;
  server.setConnectCallback(on_connect);
  server.setCloseCallback(on_disconnect);
  server.setMessageCallback(on_message);
  server.setPongCallback(on_pong);
  server.setTcpNoDelay(true);

  reset_ping_state();

  bool server_ok = server.startListening(8088);
  if (!server_ok) {
    printf("Failed to start WebSocket server\n");
    while (1) tight_loop_contents();
  }
  printf("WebSocket server started\n");

  while (1) {
    cyw43_arch_poll();
    maybe_send_ping(server);
  }
}
