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

## Performance Notes
No benchmarking has been done, but this server is expected to have a small memory footprint and low response latency. However, there is likely room for performance improvement when it comes to processing large payloads.

Some low hanging fruit for future improvement:
* Process payload bytes in bulk
* Provide interface to consume chunked messages, without copying into a continuous buffer
