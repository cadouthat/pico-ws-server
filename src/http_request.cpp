#include "http_request.h"

#include <string.h>

#include "lwip/pbuf.h"

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

} // namespace

HTTPRequest::ProcessResult HTTPRequest::process(struct pbuf* pb) {
  request_bytes = 0;
  for (size_t i = 0; i < pb->tot_len; ++i) {
    char c = pbuf_get_at(pb, i);
    bool sent_response;
    const auto result = process(c);
    switch (result) {
      case CONTINUE:
        continue;
      default:
        return result;
    }
  }
  return BAD_REQUEST;
}

bool HTTPRequest::matchAndThen(char c, const char* expected, RequestPart next_part) {
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

void HTTPRequest::processHeader(const char* header) {
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
}

HTTPRequest::ProcessResult HTTPRequest::process(char c) {
    if (++request_bytes > MAX_REQUEST_SIZE) {
      return BAD_REQUEST;
    }
  
    switch (current_part) {
    case METHOD:
      if (!matchAndThen(c, EXPECTED_METHOD, PATH)) {
        return BAD_METHOD;
      }
      return CONTINUE;
  
    case PATH:
      if (!matchAndThen(c, EXPECTED_PATH, PROTOCOL)) {
        return NOT_FOUND;
      }
      return CONTINUE;
  
    case PROTOCOL:
      if (!matchAndThen(c, EXPECTED_PROTOCOL, HEADER)) {
        return BAD_REQUEST;
      }
      return CONTINUE;
  
    case HEADER:
      if (c == '\r') {
        current_part = LINE_DELIM;
        current_index = 0;
      } else if (current_index < HEADER_BUF_SIZE - 1) {
        current_header[current_index++] = c;
        current_header[current_index] = 0;
      }
      return CONTINUE;
  
    case LINE_DELIM:
      if (c != '\n') {
        return BAD_REQUEST;
      }
      if (!current_header[0]) {
        return ATTEMPT_UPGRADE;
      }
      processHeader(current_header);
      current_part = HEADER;
      current_index = 0;
      current_header[0] = 0;
      return CONTINUE;
    }

    return BAD_REQUEST;
}