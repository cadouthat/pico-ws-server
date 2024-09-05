# pico-ws-server
WebSockets server implementation for Raspberry Pi Pico W on top of raw lwIP

## Building
Users of this library must provide a CMake library named `lwipopts_provider` which provides the `lwipopts.h` header for lwIP configuration.

Users must also link this library with an implementation of `pico_cyw43_arch` (e.g. `pico_cyw43_arch_lwip_poll`).

Warning: the `pico_cyw43_arch` implementation must allow standard library functions (including `malloc`/`free`) to be called from network workers. Since `pico_cyw43_arch_lwip_threadsafe_background` executes workers within ISRs, it is typically not safe unless you have added a critical section
wrapper around `malloc` and friends.

## Security
This server does not currently support HTTPS/WSS

## HTTP Notes
This does not aim to be a general-purpose HTTP server, and only includes minimal HTTP support for the WebSocket handshake. Only `GET / HTTP/1.1` requests are allowed, all other requests will return status `400`/`404`/`405`.

Valid requests which do not request a WebSocket upgrade will be served a static HTML blob. This is defined at compile time via CMake, using the specified file ([see example](example/CMakeLists.txt)). Define `STATIC_HTML_PATH` to point at the directory containing the static HTML, and `STATIC_HTML_FILENAME` as the static HTML filename. Changes to the HTML file will get added at compile time.

## Performance Notes
No benchmarking has been done, but this server is expected to have a small memory footprint and low response latency. However, there is likely room for performance improvement when it comes to processing large payloads.

Some low hanging fruit for future improvement:
* Process payload bytes in bulk
* Provide interface to consume chunked messages, without copying into a continuous buffer
