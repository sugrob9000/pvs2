#pragma once
#include "usart.h"
#include <span>
#include <string_view>

namespace uart {
std::string_view recv(std::span<char> buf);
void send(std::string_view);
void sendln(std::string_view);
void toggle_irq_mode();
} // namespace uart

// Try to not go long without calling this
void poll_peripherals();

// RAII guard for enabling/disabling interrupts
// XXX: should save & restore CPSR instead of using a counter
struct IrqGuard {
  static inline int depth = 0;

  IrqGuard() {
    if (depth++ == 0) {
      __disable_irq();
    }
  }

  ~IrqGuard() {
    if (--depth == 0) {
      __enable_irq();
    }
  }

  IrqGuard(const IrqGuard&) = delete;
  IrqGuard(IrqGuard&&) = delete;
  IrqGuard& operator=(const IrqGuard&) = delete;
  IrqGuard& operator=(IrqGuard&&) = delete;
};
