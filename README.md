# pico-ws-server
WebSockets server implementation for Raspberry Pi Pico W on top of raw lwIP

## Building
Users of this library must provide a CMake library named `lwipopts_provider` which provides the `lwipopts.h` header for lwIP configuration.

Users must also link this library with an implementation of `pico_cyw43_arch` (e.g. `pico_cyw43_arch_lwip_poll`).

Warning: the `pico_cyw43_arch` implementation must allow standard library functions (including `malloc`/`free`) to be called from network workers. Since `pico_cyw43_arch_lwip_threadsafe_background` executes workers within ISRs, it is typically not safe unless you have added a critical section
wrapper around `malloc` and friends.

## WebSocket Features

### PING/PONG Support (RFC 6455)
The server implements WebSocket PING (opcode 0x9) and PONG (opcode 0xA) control frames per RFC 6455:
- **Automatic PONG responses**: The server automatically replies to client PING frames with PONG frames, echoing the payload
- **Application PING API**: Applications can send PING frames using `sendPing(conn_id, payload, size)` to monitor client liveness
- **PONG notifications**: Register a callback with `setPongCallback()` to receive notifications when PONG frames arrive

See the [pingpong example](example/pingpong.cpp) for a complete demonstration of heartbeat/liveness tracking.

## Security
This server does not currently support HTTPS/WSS

## HTTP Notes
This does not aim to be a general-purpose HTTP server, and only includes minimal HTTP support for the WebSocket handshake. Only `GET / HTTP/1.1` requests are allowed, all other requests will return status `400`/`404`/`405`.

Valid requests which do not request a WebSocket upgrade will be served a static HTML blob. This is defined at compile time via CMake, using the specified file ([see example](example/CMakeLists.txt)). Define `STATIC_HTML_PATH` to point at the directory containing the static HTML, and `STATIC_HTML_FILENAME` as the static HTML filename. Changes to the HTML file will get added at compile time.

**Recent Enhancement**: The server now supports **chunked transfer encoding** for serving larger static HTML files, enabling efficient delivery of content that exceeds single-packet buffers.

### Updating Static HTML
To regenerate the static HTML header from your source HTML file, use the provided script:
```bash
cd example/static_html_generator
./update_static_html.sh
```
This compresses the HTML with gzip and generates a C header file with the embedded content. The script requires `gzip` and `xxd` to be installed. The script runs on macOS, Linux, and Windows with WSL.

## API Reference

### Server Initialization
- **`WebSocketServer(uint32_t max_connections = 1)`**  
  Constructor. Creates a WebSocket server instance supporting up to `max_connections` simultaneous connections.

- **`bool startListening(uint16_t port)`**  
  Starts the server listening on the specified port. Returns `true` on success.

### Message Processing
- **`void popMessages()`**  
  Must be called routinely in your main loop to process incoming messages. This triggers registered message callbacks for any queued messages.

### Callbacks
All callbacks receive a `WebSocketServer&` reference and `conn_id` to identify the connection. Use `setCallbackExtra()` to pass custom application state.

#### Connection Lifecycle
- **`void setConnectCallback(ConnectCallback cb)`**  
  Callback signature: `void callback(WebSocketServer& server, uint32_t conn_id)`  
  Called when a new WebSocket connection is established.  
  ⚠️ **Warning**: May be called from ISR context. Use caution with shared data and avoid acquiring mutexes.

- **`void setCloseCallback(CloseCallback cb)`**  
  Callback signature: `void callback(WebSocketServer& server, uint32_t conn_id)`  
  Called when a connection closes.  
  ⚠️ **Warning**: May be called from ISR context. Use caution with shared data and avoid acquiring mutexes.

#### Message Handling
- **`void setMessageCallback(MessageCallback cb)`**  
  Callback signature: `void callback(WebSocketServer& server, uint32_t conn_id, const void* data, size_t len)`  
  Called when a complete message is received. The `data` pointer includes an extra null terminator for TEXT messages, allowing safe treatment as a C string.  
  **Context**: Not called from ISR, but holds the cyw43 context lock.

#### PING/PONG Monitoring
- **`void setPongCallback(PongCallback cb)`**  
  Callback signature: `void callback(WebSocketServer& server, uint32_t conn_id, const void* data, size_t len)`  
  Called when a PONG frame is received (typically in response to `sendPing()`). If no callback is registered, PONG frames are silently ignored.  
  **Context**: Same as message callback (cyw43 lock held, not ISR).

#### Custom Application State
- **`void setCallbackExtra(void* arg)`**  
  Store a pointer to custom application data. Retrieve it in callbacks using `getCallbackExtra()`.

- **`void* getCallbackExtra()`**  
  Retrieve the custom application data pointer set with `setCallbackExtra()`.

### Sending Messages

#### Unicast (Single Connection)
- **`bool sendMessage(uint32_t conn_id, const char* payload)`**  
  Send a TEXT message. `payload` must be a null-terminated string. Returns `true` on success.

- **`bool sendMessage(uint32_t conn_id, const void* payload, size_t payload_size)`**  
  Send a BINARY message with explicit size. Returns `true` on success.

- **`bool sendPing(uint32_t conn_id, const void* payload = nullptr, size_t payload_size = 0)`**  
  Send a PING control frame with optional payload (up to 125 bytes per RFC 6455). Client should respond with a PONG frame echoing the payload. Returns `true` on success.

#### Broadcast (All Connections)
- **`bool broadcastMessage(const char* payload)`**  
  Send a TEXT message to all connected clients. `payload` must be a null-terminated string. Returns `true` on success.

- **`bool broadcastMessage(const void* payload, size_t payload_size)`**  
  Send a BINARY message to all connected clients with explicit size. Returns `true` on success.

### Connection Management
- **`bool close(uint32_t conn_id)`**  
  Begin graceful shutdown of the specified connection. Messages may still be received on a closing connection, but no further messages can be sent. Returns `true` on success.

### TCP Options
- **`void setTcpNoDelay(bool enabled)`**  
  Enable/disable TCP_NODELAY to control Nagle's algorithm. When enabled (`true`), small packets are sent immediately for lower latency. When disabled (`false`, default), the TCP stack may buffer small writes to reduce network overhead.  
  Call this before `startListening()` or after connections are established.

## Performance Notes
No benchmarking has been done, but this server is expected to have a small memory footprint and low response latency. However, there is likely room for performance improvement when it comes to processing large payloads.

Some low hanging fruit for future improvement:
* Process payload bytes in bulk
* Provide interface to consume chunked messages, without copying into a continuous buffer
