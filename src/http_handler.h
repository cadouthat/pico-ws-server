#ifndef __HTTP_HANDLER_H__
#define __HTTP_HANDLER_H__

#include <cstddef>
#include <stdint.h>

#include "lwip/pbuf.h"

class ClientConnection;
class HTTPRequest;

// Only access from lwIP context
class HTTPHandler {
 public:
  HTTPHandler(ClientConnection& connection) : connection(connection) {}

  bool process(struct pbuf* pb, bool *is_upgraded);
  bool isClosing() { return is_closing; }

 private:
  ClientConnection& connection;
  bool is_closing = false;

  bool send(const void* data, size_t size);
  bool sendString(const char* s);
  bool sendHTML();
  bool sendUpgradeResponse(uint8_t* key_accept, size_t key_accept_len);
  bool attemptUpgrade(const HTTPRequest& request, bool* sent_html);
};

#endif
