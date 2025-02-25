#include "http_handler.h"

#include <cstddef>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "lwip/pbuf.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#include "client_connection.h"
#include "debug.h"
#include "http_request.h"

// generated at build time -- see CMakeLists.txt
#include <static_html_hex.h>

namespace {

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
  "Content-Encoding: gzip\r\n"
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

} // namespace

bool HTTPHandler::process(struct pbuf* pb, bool *is_upgraded) {
  if (is_closing) {
    return false;
  }

  HTTPRequest request;
  const auto result = request.process(pb);
  switch (result) {
    case HTTPRequest::BAD_REQUEST:
      sendString(BAD_REQUEST_RESPONSE);
      is_closing = true;
      return false;
    case HTTPRequest::BAD_METHOD:
      sendString(BAD_METHOD_RESPONSE);
      is_closing = true;
      return false;
    case HTTPRequest::NOT_FOUND:
      sendString(NOT_FOUND_RESPONSE);
      is_closing = true;
      return false;
    case HTTPRequest::ATTEMPT_UPGRADE: {
      bool sent_response;
      if (!attemptUpgrade(request, &sent_response)) {
        if (!sent_response) {
          sendString(BAD_REQUEST_RESPONSE);
        }
        is_closing = true;
        return false;
      }
      *is_upgraded = true;
      return true;
    }
  }
  return false;
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
  char len_string[32];
  snprintf(len_string, sizeof(len_string), "%d", static_html_gz_len);
  if (!sendString(len_string)) {
    return false;
  }
  if (!sendString(HTML_RESPONSE_END)) {
    return false;
  }
  if (!send(static_html_gz, static_html_gz_len)) {
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

bool HTTPHandler::attemptUpgrade(const HTTPRequest& request, bool* sent_response) {
  DEBUG("%s %s %s '%s'",
    has_upgrade_header ? "has_upgrade_header" : "",
    has_connection_header ? "has_connection_header" : "",
    has_ws_version_header ? "has_ws_version_header" : "",
    ws_key_header_value);
  if (!request.has_upgrade_header && !request.has_connection_header && !request.has_ws_version_header) {
    // Not a WebSocket request, serve static HTML and close connection
    if (!sendHTML()) {
      DEBUG("failed to send HTML response");
    }
    *sent_response = true;
    return false;
  }

  if (!request.has_upgrade_header || !request.has_connection_header || !request.has_ws_version_header) {
    return false;
  }
  if (!request.ws_key_header_value[0] || strlen(request.ws_key_header_value) > WS_KEY_BASE64_MAX) {
    return false;
  }

  char combined_key[WS_KEY_COMBINED_BUFFER] = {0};
  strcat(combined_key, request.ws_key_header_value);
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
