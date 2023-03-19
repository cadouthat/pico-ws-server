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

  // Note: methods below must be called from lwIP context. It is safe to call them from callback handlers
  bool startListening(uint16_t port);
  // Send a TEXT message, payload must be a null-terminated string
  bool sendMessage(uint32_t conn_id, const char* payload);
  // Send a BINARY message
  bool sendMessage(uint32_t conn_id, const void* payload, size_t payload_size);

 private:
  std::unique_ptr<WebSocketServerInternal> internal;
  void* callback_extra = nullptr;
};

#endif
