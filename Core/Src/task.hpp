#pragma once
#include "usart.h"

// Try to not go long without calling this
void poll_peripherals();


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
