#include "client_connection.h"

#include <cstddef>
#include <string.h>
#include <queue>

#include "cyw43_config.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "web_socket_message.h"
#include "web_socket_server_internal.h"

void ClientConnection::popMessages() {
  while (!message_queue.empty()) {
    server.onMessage(this, message_queue.front().getPayload(), message_queue.front().getPayloadSize());
    message_queue.pop();
  }
}

bool ClientConnection::process(struct pbuf* pb) {
  bool keep_connection = true;
  if (is_upgraded) {
    keep_connection = ws_handler.process(pb);
  } else if (server.onUser(this, pb)) {
    keep_connection = false;
  } else {
    keep_connection = http_handler.process(pb, &is_upgraded);
    if (is_upgraded) {
      server.onUpgrade(this);
    }
  }

  return keep_connection;
}

bool ClientConnection::sendRaw(const void* data, size_t size) {
  cyw43_arch_lwip_check();

  if (!size) {
    return true;
  }

  // Note: unfortunately, we cannot easily determine whether ERR_MEM should be retryable here. It could be a
  // transient problem that would resolve over time, e.g. if the remote side acks some data and frees up send buffer.
  // Or, this payload (and the downstream structures needed) may be too large to ever fit in the LwIP pools.
  //
  // At best, we could return a "maybe retriable" status here, since we know some errors are permanent. However,
  // this would add complexity, and would still leave callers with the burden of non-convergent retries.
  return tcp_write(pcb, data, size, TCP_WRITE_FLAG_COPY) == ERR_OK;
}

bool ClientConnection::flushSend() {
  cyw43_arch_lwip_check();

  return tcp_output(pcb) == ERR_OK;
}

void ClientConnection::onClose() {
  server.onClose(this, is_upgraded);
}

bool ClientConnection::isClosing() {
  return http_handler.isClosing() || ws_handler.isClosing();
}

void ClientConnection::processWebSocketMessage(WebSocketMessage&& message) {
  message_queue.push(std::move(message));
}

bool ClientConnection::sendWebSocketMessage(const char* payload) {
  if (!is_upgraded) {
    return false;
  }

  return ws_handler.sendMessage(WebSocketMessage(WebSocketMessage::TEXT, payload, strlen(payload)));
}

bool ClientConnection::sendWebSocketMessage(const void* payload, size_t size) {
  if (!is_upgraded) {
    return false;
  }

  return ws_handler.sendMessage(WebSocketMessage(WebSocketMessage::BINARY, payload, size));
}

bool ClientConnection::close() {
  if (!is_upgraded) {
    return false;
  }

  return ws_handler.close();
}
