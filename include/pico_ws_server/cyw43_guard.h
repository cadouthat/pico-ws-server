#ifndef __PICO_WS_SERVER_CYW43_GUARD_H__
#define __PICO_WS_SERVER_CYW43_GUARD_H__

#include "cyw43_config.h"

// RAII-style wrapper for cyw43 thread enter/exit.
class Cyw43Guard {
 public:
  Cyw43Guard() {
    cyw43_thread_enter();
  }
  ~Cyw43Guard() {
    cyw43_thread_exit();
  }
};

#endif
