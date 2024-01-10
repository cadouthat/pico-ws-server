#ifndef __PICO_WS_SERVER_WEB_SOCKET_SERVER_H__
#define __PICO_WS_SERVER_WEB_SOCKET_SERVER_H__

#include <cstddef>
#include <memory>
#include <stdint.h>

class WebSocketServerInternal;

// Not multicore safe
class WebSocketServer {
 public:
  typedef void (*ConnectCallback)(WebSocketServer& server, uint32_t conn_id);
  // Note: data can be treated as a null-terminated string if expecting TEXT messages (an extra NULL byte is allocated)
  typedef void (*MessageCallback)(WebSocketServer& server, uint32_t conn_id, const void *data, size_t len);
  typedef void (*CloseCallback)(WebSocketServer& server, uint32_t conn_id);

  WebSocketServer(uint32_t max_connections = 1);
  ~WebSocketServer();

  // Warning: connect/close callbacks may be called from cyw43 ISR context, use caution with shared data
  // and avoid acquiring mutexes (ideally, only access data that can rely on cyw43 context for exclusion)
  void setConnectCallback(ConnectCallback cb);
  void setCloseCallback(CloseCallback cb);
  // Note: unlike connect/close, the message callback will not be called from an ISR, but still holds
  // the cyw43 context lock
  void setMessageCallback(MessageCallback cb);
  void setCallbackExtra(void* arg);
  void* getCallbackExtra();

  bool startListening(uint16_t port);
  // Must be called routinely to process incoming messages (triggers message callback)
  void popMessages();

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
