#include "web_socket_server_internal.h"

#include <stdint.h>

#include "cyw43_config.h"
#include "lwip/tcp.h"

#include "client_connection.h"
#include "debug.h"
#include "pico_ws_server/cyw43_guard.h"

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

  bool keep_connection;
  if (pb) {
    keep_connection = connection->process(pb);
    tcp_recved(pcb, pb->tot_len);
    pbuf_free(pb);
  } else {
    keep_connection = false;
    DEBUG("client disconnected");
  }

  if (!keep_connection) {
    DEBUG("closing connection");

    tcp_arg(pcb, nullptr);
    connection->onClose();

    if (tcp_close(pcb) != ERR_OK) {
      tcp_abort(pcb);
      return ERR_ABRT;
    }
  }

  return ERR_OK;
}

err_t on_poll(void* arg, struct tcp_pcb* pcb) {
  cyw43_arch_lwip_check();

  if (!arg) {
    DEBUG("aborting inactive connection with null arg");
    tcp_abort(pcb);
    return ERR_ABRT;
  }

  ClientConnection* connection = (ClientConnection*)arg;
  if (connection->isClosing()) {
    tcp_arg(pcb, nullptr);
    connection->onClose();

    DEBUG("aborting inactive connection after close request");
    tcp_abort(pcb);
    return ERR_ABRT;
  }

  return ERR_OK;
}

void on_error(void* arg, err_t err) {
  cyw43_arch_lwip_check();

  DEBUG("connection errored %s: %d", arg ? "with arg" : "without arg", (int)err);

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
    DEBUG("aborting %s", arg ? "with arg" : "without arg");
    tcp_abort(new_pcb);
    return ERR_ABRT;
  }

  ClientConnection* connection = ((WebSocketServerInternal*)arg)->onConnect(new_pcb);
  if (!connection) {
    // Unable to accept connection
    DEBUG("aborting due to server rejection");
    tcp_abort(new_pcb);
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
  Cyw43Guard guard;

  struct tcp_pcb* temp_pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (!temp_pcb) {
    DEBUG("failed to create temp pcb");
    return NULL;
  }

  tcp_arg(temp_pcb, arg);

  if (tcp_bind(temp_pcb, IP_ADDR_ANY, port) != ERR_OK) {
    DEBUG("failed to bind");
    tcp_abort(temp_pcb);
    return NULL;
  }

  struct tcp_pcb* listen_pcb = tcp_listen(temp_pcb);
  if (!listen_pcb) {
    DEBUG("failed to create listen pcb");
    tcp_abort(temp_pcb);
  }

  // temp_pcb has already been freed
  return listen_pcb;
}

} // namespace

bool WebSocketServerInternal::startListening(uint16_t port) {
  Cyw43Guard guard;

  if (listen_pcb) {
    // Already listening
    return false;
  }

  listen_pcb = init_listen_pcb(port, this);
  if (listen_pcb) {
    tcp_accept(listen_pcb, on_connect);
  }

  return listen_pcb != nullptr;
}

void WebSocketServerInternal::popMessages() {
  Cyw43Guard guard;

  for (const auto& [_, conn] : connection_by_id) {
    conn->popMessages();
  }
}

bool WebSocketServerInternal::sendMessage(uint32_t conn_id, const char* payload) {
  Cyw43Guard guard;

  ClientConnection* connection = getConnectionById(conn_id);
  if (!connection) {
    DEBUG("connection not found");
    return false;
  }

  bool result = connection->sendWebSocketMessage(payload);

  return result;
}

bool WebSocketServerInternal::sendMessage(uint32_t conn_id, const void* payload, size_t payload_size) {
  Cyw43Guard guard;

  ClientConnection* connection = getConnectionById(conn_id);
  if (!connection) {
    DEBUG("connection not found");
    return false;
  }

  bool result = connection->sendWebSocketMessage(payload, payload_size);

  return result;
}

bool WebSocketServerInternal::broadcastMessage(const char* payload) {
  Cyw43Guard guard;

  if (connection_by_id.size() == 0) {
    DEBUG("connection map is empty");
    return false;
  }

  for (const auto& [_, connection] : connection_by_id) {
    connection->sendWebSocketMessage(payload);
  }

  return true;
}

bool WebSocketServerInternal::broadcastMessage(const void* payload, size_t payload_size) {
  Cyw43Guard guard;

  if (connection_by_id.size() == 0) {
    DEBUG("connection map is empty");
    return false;
  }

  for (const auto& [_, connection] : connection_by_id) {
    connection->sendWebSocketMessage(payload, payload_size);
  }

  return true;
}

bool WebSocketServerInternal::close(uint32_t conn_id) {
  Cyw43Guard guard;

  ClientConnection* connection = getConnectionById(conn_id);
  if (!connection) {
    DEBUG("connection not found");
    return false;
  }

  bool result = connection->close();

  return result;
}

ClientConnection* WebSocketServerInternal::onConnect(struct tcp_pcb* pcb) {
  cyw43_arch_lwip_check();

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
  cyw43_arch_lwip_check();

  if (connect_cb) {
    connect_cb(server, getConnectionId(connection));
  }
}

void WebSocketServerInternal::onClose(ClientConnection* connection, bool is_upgraded) {
  cyw43_arch_lwip_check();

  uint32_t conn_id = getConnectionId(connection);

  if (is_upgraded && close_cb) {
    close_cb(server, conn_id);
  }

  connection_by_id.erase(conn_id);
}

void WebSocketServerInternal::onMessage(ClientConnection* connection, const void* payload, size_t size) {
  cyw43_arch_lwip_check();

  if (message_cb) {
    message_cb(server, getConnectionId(connection), payload, size);
  }
}

uint32_t WebSocketServerInternal::getConnectionId(ClientConnection* connection) {
  // Use the connection instance address as a unique ID
  return (uint32_t)connection;
}

ClientConnection* WebSocketServerInternal::getConnectionById(uint32_t conn_id) {
  Cyw43Guard guard;

  auto iter = connection_by_id.find(conn_id);
  if (iter == connection_by_id.end()) {
    return nullptr;
  }

  return iter->second.get();
}
