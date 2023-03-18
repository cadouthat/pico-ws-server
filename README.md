# pico-ws-server
WebSockets server implementation for Raspberry Pi Pico W on top of raw lwIP

Note: this does not aim to be a general-purpose HTTP server, and only includes minimal HTTP handling to support the WebSocket handshake.

It does support serving a single, static HTML blob for all non-websocket HTTP GET requests. This is defined at compile time by setting PICO_WS_SERVER_STATIC_HTML in cmake.
