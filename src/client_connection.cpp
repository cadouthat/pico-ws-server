#include "client_connection.h"

#include <cstddef>
#include <string.h>

#include "cyw43_config.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "web_socket_message.h"
#include "web_socket_server_internal.h"

bool ClientConnection::process(struct pbuf* pb) {
  bool result;
  if (http_handler.isUpgraded()) {
    result = ws_handler.process(pb);
  } else {
    result = http_handler.process(pb);
    if (http_handler.isUpgraded()) {
      server.onUpgrade(this);
    }
  }

  return result;
}

bool ClientConnection::sendRaw(const void* data, size_t size) {
  cyw43_arch_lwip_check();

  if (!size) {
    return true;
  }

  return tcp_write(pcb, data, size, TCP_WRITE_FLAG_COPY) == ERR_OK;
}

void ClientConnection::onClose() {
  server.onClose(this, http_handler.isUpgraded());
}

bool ClientConnection::isClosing() {
  return http_handler.isClosing() || ws_handler.isClosing();
}

void ClientConnection::processWebSocketMessage(const void* payload, size_t size) {
  server.onMessage(this, payload, size);
}

bool ClientConnection::sendWebSocketMessage(const char* payload) {
  if (!http_handler.isUpgraded()) {
    return false;
  }

  return ws_handler.sendMessage(WebSocketMessage(WebSocketMessage::TEXT, payload, strlen(payload)));
}

bool ClientConnection::sendWebSocketMessage(const void* payload, size_t size) {
  if (!http_handler.isUpgraded()) {
    return false;
  }

  return ws_handler.sendMessage(WebSocketMessage(WebSocketMessage::BINARY, payload, size));
}
