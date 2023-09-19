#ifndef __PICO_WS_SERVER_WEB_SOCKET_SERVER_H__
#define __PICO_WS_SERVER_WEB_SOCKET_SERVER_H__

#include <cstddef>
#include <memory>
#include <stdint.h>

class WebSocketServerInternal;

class WebSocketServer {
 public:
  typedef void (*ConnectCallback)(WebSocketServer& server, uint32_t conn_id);
  // Note: data can be treated as a null-terminated string if expecting TEXT messages.
  typedef void (*MessageCallback)(WebSocketServer& server, uint32_t conn_id, const void *data, size_t len);
  typedef void (*CloseCallback)(WebSocketServer& server, uint32_t conn_id);

  WebSocketServer(uint32_t max_connections = 1);
  ~WebSocketServer();

  // Atomic access, thread safe
  void setConnectCallback(ConnectCallback cb);
  void setMessageCallback(MessageCallback cb);
  void setCloseCallback(CloseCallback cb);
  void setCallbackExtra(void* arg);
  void* getCallbackExtra();

  bool startListening(uint16_t port);
  // Send a TEXT message, payload must be a null-terminated string
  bool sendMessage(uint32_t conn_id, const char* payload);
  // Send a BINARY message
  bool sendMessage(uint32_t conn_id, const void* payload, size_t payload_size);

  // Send a TEXT message to all connections, payload must be a null-terminated string
  bool broadcastMessage(const char* payload);
  // Send a BINARY message to all connections
  bool broadcastMessage(const void* payload, size_t payload_size);

  // Begin closing the specified connection.
  // Note: it is still possible for messages to be received on a closing connection,
  // but no further messages may be sent.
  bool close(uint32_t conn_id);

 private:
  std::unique_ptr<WebSocketServerInternal> internal;
  void* callback_extra = nullptr;
};

#endif
