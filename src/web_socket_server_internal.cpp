#include "web_socket_server_internal.h"

#include <stdint.h>

#include "cyw43_config.h"
#include "lwip/tcp.h"

#include "client_connection.h"
#include "debug.h"

namespace {

constexpr auto POLL_TIMER_COARSE = 10; // around 5 seconds

err_t on_recv(void* arg, struct tcp_pcb* pcb, struct pbuf* pb, err_t err) {
  cyw43_arch_lwip_check();

  if (!arg) {
    if (pb) {
      DEBUG("pbuf with null arg");
      pbuf_free(pb);
    }
    return ERR_OK;
  }

  ClientConnection* connection = (ClientConnection*)arg;

  bool ok = false;
  if (pb) {
    ok = connection->process(pb);
    tcp_recved(pcb, pb->tot_len);
  }

  if (!ok) {
    tcp_arg(pcb, nullptr);
    connection->onClose();
    tcp_close(pcb);
    if (pb) {
      if (tcp_output(pcb) != ERR_OK) {
        DEBUG("tcp_output failed");
      }
      DEBUG("closing connection");
    } else {
      DEBUG("client closed connection");
    }
  }

  if (pb) {
    pbuf_free(pb);
  }
  return ERR_OK;
}

err_t on_poll(void* arg, struct tcp_pcb* pcb) {
  cyw43_arch_lwip_check();

  if (!arg) {
    tcp_abort(pcb);
    DEBUG("aborting inactive connection with null arg");
    return ERR_ABRT;
  }

  ClientConnection* connection = (ClientConnection*)arg;
  if (connection->isClosing()) {
    tcp_arg(pcb, nullptr);
    connection->onClose();
    tcp_abort(pcb);
    DEBUG("aborting inactive connection after close request");
    return ERR_ABRT;
  }

  return ERR_OK;
}

void on_error(void* arg, err_t err) {
  cyw43_arch_lwip_check();

  DEBUG("connection errored %s", arg ? "with arg" : "without arg");

  if (arg) {
    ((ClientConnection*)arg)->onClose();
  }
}

err_t on_connect(void* arg, struct tcp_pcb* new_pcb, err_t err) {
  cyw43_arch_lwip_check();

  if (!new_pcb) {
    // Connection error
    DEBUG("null pcb");
    return ERR_ARG;
  }
  if (!arg || err != ERR_OK) {
    // Unexpected error
    tcp_abort(new_pcb);
    DEBUG("aborting %s", arg ? "with arg" : "without arg");
    return ERR_ABRT;
  }

  ClientConnection* connection = ((WebSocketServerInternal*)arg)->onConnect(new_pcb);
  if (!connection) {
    // Unable to accept connection
    tcp_abort(new_pcb);
    DEBUG("aborting due to server rejection");
    return ERR_ABRT;
  }

  tcp_arg(new_pcb, connection);
  tcp_err(new_pcb, on_error);
  tcp_recv(new_pcb, on_recv);
  tcp_poll(new_pcb, on_poll, POLL_TIMER_COARSE);

  DEBUG("success");

  return ERR_OK;
}

struct tcp_pcb* init_listen_pcb(uint16_t port, void* arg) {
  cyw43_arch_lwip_check();

  struct tcp_pcb* temp_pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (!temp_pcb) {
    DEBUG("failed to create temp pcb");
    return NULL;
  }

  tcp_arg(temp_pcb, arg);

  if (tcp_bind(temp_pcb, IP_ADDR_ANY, port) != ERR_OK) {
    tcp_abort(temp_pcb);
    DEBUG("failed to bind");
    return NULL;
  }

  struct tcp_pcb* listen_pcb = tcp_listen(temp_pcb);
  if (!listen_pcb) {
    tcp_abort(temp_pcb);
    DEBUG("failed to create listen pcb");
  }

  // temp_pcb has already been freed
  return listen_pcb;
}

} // namespace

bool WebSocketServerInternal::startListening(uint16_t port) {
  if (listen_pcb) {
    // Already listening
    return false;
  }

  cyw43_thread_enter();

  listen_pcb = init_listen_pcb(port, this);
  if (listen_pcb) {
    tcp_accept(listen_pcb, on_connect);
  }

  cyw43_thread_exit();

  return listen_pcb != nullptr;
}

bool WebSocketServerInternal::sendMessage(uint32_t conn_id, const char* payload) {
  ClientConnection* connection = getConnectionById(conn_id);
  if (!connection) {
    DEBUG("connection not found");
    return false;
  }

  cyw43_thread_enter();

  bool result = connection->sendWebSocketMessage(payload);

  cyw43_thread_exit();
  return result;
}

bool WebSocketServerInternal::sendMessage(uint32_t conn_id, const void* payload, size_t payload_size) {
  ClientConnection* connection = getConnectionById(conn_id);
  if (!connection) {
    DEBUG("connection not found");
    return false;
  }

  cyw43_thread_enter();

  bool result = connection->sendWebSocketMessage(payload, payload_size);

  cyw43_thread_exit();
  return result;
}

bool WebSocketServerInternal::broadcastMessage(const char* payload) {
  if (connection_by_id.size() == 0) {
    DEBUG("connection map is empty");
    return false;
  }

  cyw43_thread_enter();
  
  for (const auto& [_, connection] : connection_by_id) {
    connection->sendWebSocketMessage(payload);
  }

  cyw43_thread_exit();
  return true;
}

bool WebSocketServerInternal::broadcastMessage(const void* payload, size_t payload_size) {
  if (connection_by_id.size() == 0) {
    DEBUG("connection map is empty");
    return false;
  }

  cyw43_thread_enter();
  
  for (const auto& [_, connection] : connection_by_id) {
    connection->sendWebSocketMessage(payload, payload_size);
  }

  cyw43_thread_exit();
  return true;
}

bool WebSocketServerInternal::close(uint32_t conn_id) {
  ClientConnection* connection = getConnectionById(conn_id);
  if (!connection) {
    DEBUG("connection not found");
    return false;
  }

  cyw43_thread_enter();

  bool result = connection->close();

  cyw43_thread_exit();
  return result;
}

ClientConnection* WebSocketServerInternal::onConnect(struct tcp_pcb* pcb) {
  if (connection_by_id.size() >= max_connections) {
    return nullptr;
  }

  auto connection = std::make_unique<ClientConnection>(*this, pcb);
  ClientConnection* connection_ptr = connection.get();
  uint32_t conn_id = getConnectionId(connection_ptr);

  connection_by_id[conn_id] = std::move(connection);

  return connection_ptr;
}

void WebSocketServerInternal::onUpgrade(ClientConnection* connection) {
  if (connect_cb) {
    connect_cb(server, getConnectionId(connection));
  }
}

void WebSocketServerInternal::onClose(ClientConnection* connection, bool is_upgraded) {
  uint32_t conn_id = getConnectionId(connection);

  if (is_upgraded && close_cb) {
    close_cb(server, conn_id);
  }

  connection_by_id.erase(conn_id);
}

void WebSocketServerInternal::onMessage(ClientConnection* connection, const void* payload, size_t size) {
  if (message_cb) {
    message_cb(server, getConnectionId(connection), payload, size);
  }
}

uint32_t WebSocketServerInternal::getConnectionId(ClientConnection* connection) {
  // Use the connection instance address as a unique ID
  return (uint32_t)connection;
}

ClientConnection* WebSocketServerInternal::getConnectionById(uint32_t conn_id) {
  auto iter = connection_by_id.find(conn_id);
  if (iter == connection_by_id.end()) {
    return nullptr;
  }

  return iter->second.get();
}
