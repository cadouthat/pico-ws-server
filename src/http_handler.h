#ifndef __HTTP_HANDLER_H__
#define __HTTP_HANDLER_H__

#include <cstddef>
#include <stdint.h>

#include "lwip/pbuf.h"

class ClientConnection;

class HTTPHandler {
 public:
  HTTPHandler(ClientConnection& connection) : connection(connection) {}

  bool process(struct pbuf* pb);
  bool isUpgraded() { return is_upgraded; }
  bool isClosing() { return is_closing; }

 private:
  static constexpr auto HEADER_BUF_SIZE = 64;

  enum RequestPart {
    METHOD,
    PATH,
    PROTOCOL,
    HEADER,
    LINE_DELIM,
  };

  ClientConnection& connection;
  bool is_upgraded = false;
  bool is_closing = false;

  size_t request_bytes = 0;
  RequestPart current_part = METHOD;
  size_t current_index = 0;

  char current_header[HEADER_BUF_SIZE] = {0};

  bool has_upgrade_header = false;
  bool has_connection_header = false;
  bool has_ws_version_header = false;
  char ws_key_header_value[HEADER_BUF_SIZE] = {0};

  bool send(const void* data, size_t size);
  bool sendString(const char* s);
  bool sendHTML();
  bool sendUpgradeResponse(uint8_t* key_accept, size_t key_accept_len);
  bool attemptUpgrade(bool* sent_html);

  bool matchAndThen(char c, const char* expected, RequestPart next_part);
  bool processHeader(const char* header);
  bool process(char c, bool* sent_html);
};

#endif
