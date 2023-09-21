#include "http_handler.h"

#include <cstddef>
#include <stdint.h>
#include <string.h>

#include "lwip/pbuf.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#include "client_connection.h"
#include "debug.h"

#ifndef PICO_WS_SERVER_STATIC_HTML_HEX
#define PICO_WS_SERVER_STATIC_HTML_HEX "20"
#endif

namespace {

static constexpr auto MAX_REQUEST_SIZE = 4096;

static constexpr auto HEADER_BUF_SIZE = 64;
static constexpr const char EXPECTED_METHOD[] = "GET ";
static constexpr const char EXPECTED_PATH[] = "/ ";
static constexpr const char EXPECTED_PROTOCOL[] = "HTTP/1.1\r\n";
static constexpr const char EXPECTED_HEADER_UPGRADE[] = "Upgrade: websocket";
static constexpr const char EXPECTED_HEADER_CONNECTION_KEY[] = "Connection: ";
static constexpr const char EXPECTED_HEADER_CONNECTION_VALUE[] = "Upgrade";
static constexpr const char EXPECTED_HEADER_WS_VERSION[] = "Sec-WebSocket-Version: 13";
static constexpr const char EXPECTED_HEADER_NAME_WS_KEY[] = "Sec-WebSocket-Key: ";
static constexpr auto WS_KEY_BASE64_MAX = 24;
static constexpr const char WS_KEY_MAGIC[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static constexpr auto WS_KEY_COMBINED_BUFFER = WS_KEY_BASE64_MAX + sizeof(WS_KEY_MAGIC);
static constexpr auto SHA1_SIZE = 20;
static constexpr auto SHA1_BASE64_SIZE = 28;

static constexpr const char BAD_REQUEST_RESPONSE[] =
  "HTTP/1.1 400 Bad Request\r\n"
  "Connection: close\r\n"
  "Sec-WebSocket-Version: 13\r\n\r\n";

static constexpr const char NOT_FOUND_RESPONSE[] =
  "HTTP/1.1 404 Not Found\r\n"
  "Connection: close\r\n\r\n";

static constexpr const char BAD_METHOD_RESPONSE[] =
  "HTTP/1.1 405 Method Not Allowed\r\n"
  "Connection: close\r\n\r\n";

static constexpr const char HTML_RESPONSE_START[] =
  "HTTP/1.1 200 OK\r\n"
  "Connection: close\r\n"
  "Content-Type: text/html\r\n"
  "Content-Length: ";
static constexpr const char HTML_RESPONSE_END[] =
  "\r\n\r\n";

static constexpr const char UPGRADE_RESPONSE_START[] =
  "HTTP/1.1 101 Switching Protocols\r\n"
  "Upgrade: websocket\r\n"
  "Connection: upgrade\r\n";
static constexpr const char UPGRADE_RESPONSE_ACCEPT_PREFIX[] =
  "Sec-WebSocket-Accept: ";
static constexpr const char UPGRADE_RESPONSE_END[] =
  "\r\n\r\n";

void decode_hex(const char* hex, uint8_t* out) {
  size_t i_out = 0;
  char hex_byte[3] = {0};
  for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
    hex_byte[0] = hex[i];
    hex_byte[1] = hex[i + 1];
    out[i_out++] = strtol(hex_byte, nullptr, 16);
  }
}

} // namespace

bool HTTPHandler::process(struct pbuf* pb) {
  if (is_closing) {
    return false;
  }

  for (size_t i = 0; i < pb->tot_len; i++) {
    char c = pbuf_get_at(pb, i);
    bool sent_response;
    if (!process(c, &sent_response)) {
      if (!sent_response) {
        sendString(BAD_REQUEST_RESPONSE);
      }
      is_closing = true;
      return false;
    }
  }
  return true;
}

bool HTTPHandler::send(const void* data, size_t size) {
  return connection.sendRaw(data, size);
}

bool HTTPHandler::sendString(const char* s) {
  return send(s, strlen(s));
}

bool HTTPHandler::sendHTML() {
  if (!sendString(HTML_RESPONSE_START)) {
    return false;
  }
  uint8_t html_payload[sizeof(PICO_WS_SERVER_STATIC_HTML_HEX) / 2];
  decode_hex(PICO_WS_SERVER_STATIC_HTML_HEX, html_payload);
  char len_string[32];
  snprintf(len_string, sizeof(len_string), "%d", sizeof(html_payload));
  if (!sendString(len_string)) {
    return false;
  }
  if (!sendString(HTML_RESPONSE_END)) {
    return false;
  }
  if (!send(html_payload, sizeof(html_payload))) {
    return false;
  }
  connection.flushSend();
  return true;
}

bool HTTPHandler::sendUpgradeResponse(uint8_t* key_accept, size_t key_accept_len) {
  if (!sendString(UPGRADE_RESPONSE_START)) {
    return false;
  }
  if (!sendString(UPGRADE_RESPONSE_ACCEPT_PREFIX)) {
    return false;
  }
  if (!send(key_accept, key_accept_len)) {
    return false;
  }
  if (!sendString(UPGRADE_RESPONSE_END)) {
    return false;
  }
  connection.flushSend();
  return true;
}

bool HTTPHandler::attemptUpgrade(bool* sent_response) {
  DEBUG("%s %s %s '%s'",
    has_upgrade_header ? "has_upgrade_header" : "",
    has_connection_header ? "has_connection_header" : "",
    has_ws_version_header ? "has_ws_version_header" : "",
    ws_key_header_value);
  if (!has_upgrade_header && !has_connection_header && !has_ws_version_header) {
    // Not a WebSocket request, serve static HTML and close connection
    sendHTML();
    *sent_response = true;
    return false;
  }

  if (!has_upgrade_header || !has_connection_header || !has_ws_version_header) {
    return false;
  }
  if (!ws_key_header_value[0] || strlen(ws_key_header_value) > WS_KEY_BASE64_MAX) {
    return false;
  }

  char combined_key[WS_KEY_COMBINED_BUFFER] = {0};
  strcat(combined_key, ws_key_header_value);
  strcat(combined_key, WS_KEY_MAGIC);

  uint8_t sha1[SHA1_SIZE];
  if (mbedtls_sha1_ret((uint8_t*)combined_key, strlen(combined_key), sha1) != 0) {
    return false;
  }

  size_t sha1_base64_len;
  uint8_t sha1_base64[SHA1_BASE64_SIZE + 1];
  if (mbedtls_base64_encode(sha1_base64, sizeof(sha1_base64), &sha1_base64_len, sha1, SHA1_SIZE) != 0) {
    return false;
  }

  return sendUpgradeResponse(sha1_base64, sha1_base64_len);
}

bool HTTPHandler::matchAndThen(char c, const char* expected, RequestPart next_part) {
    if (c != expected[current_index++]) {
      current_index = 0;
      return false;
    }
    if (!expected[current_index]) {
      current_part = next_part;
      current_index = 0;
    }
    return true;
}

bool HTTPHandler::processHeader(const char* header) {
  if (!strcmp(header, EXPECTED_HEADER_UPGRADE)) {
    has_upgrade_header = true;
  }
  if (!strncmp(header, EXPECTED_HEADER_CONNECTION_KEY, strlen(EXPECTED_HEADER_CONNECTION_KEY))) {
    if (strstr(header + strlen(EXPECTED_HEADER_CONNECTION_KEY), EXPECTED_HEADER_CONNECTION_VALUE)) {
      has_connection_header = true;
    }
  }
  if (!strcmp(header, EXPECTED_HEADER_WS_VERSION)) {
    has_ws_version_header = true;
  }
  if (!strncmp(header, EXPECTED_HEADER_NAME_WS_KEY, strlen(EXPECTED_HEADER_NAME_WS_KEY))) {
    strcpy(ws_key_header_value, header + strlen(EXPECTED_HEADER_NAME_WS_KEY));
  }
  return true;
}

bool HTTPHandler::process(char c, bool *sent_response) {
  if (++request_bytes > MAX_REQUEST_SIZE) {
    return false;
  }

  switch (current_part) {
  case METHOD:
    if (!matchAndThen(c, EXPECTED_METHOD, PATH)) {
      sendString(BAD_METHOD_RESPONSE);
      *sent_response = true;
      return false;
    }
    return true;

  case PATH:
    if (!matchAndThen(c, EXPECTED_PATH, PROTOCOL)) {
      sendString(NOT_FOUND_RESPONSE);
      *sent_response = true;
      return false;
    }
    return true;

  case PROTOCOL:
    return matchAndThen(c, EXPECTED_PROTOCOL, HEADER);

  case HEADER:
    if (c == '\r') {
      current_part = LINE_DELIM;
      current_index = 0;
    } else if (current_index < HEADER_BUF_SIZE - 1) {
      current_header[current_index++] = c;
      current_header[current_index] = 0;
    }
    return true;

  case LINE_DELIM: {
    if (c != '\n') {
      return false;
    }
    if (!current_header[0]) {
      is_upgraded = attemptUpgrade(sent_response);
      return is_upgraded;
    }
    bool ok = processHeader(current_header);
    current_part = HEADER;
    current_index = 0;
    current_header[0] = 0;
    return ok;
  }

  default:
    return false;
  }
}
