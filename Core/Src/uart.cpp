#include "main.h"
#include "task.hpp"
#include "uart.hpp"
#include <usart.h>

namespace uart {
namespace {

struct Channel {
  constexpr static int size = 128;
  uint8_t mem[size];
  int head = 0;
  int tail = 0;

  void push(char c, const IrqGuard&) volatile {
    int next_tail = (tail + 1) % size;
    if (next_tail != head) {
      mem[tail] = static_cast<uint8_t>(c);
      tail = next_tail;
    }
  }

  char pop(const IrqGuard&) volatile {
    if (head == tail) {
      return 0;
    } else {
      return mem[std::exchange(head, (head + 1) % size)];
    }
  }
};

volatile Channel tx;
volatile Channel rx;
volatile bool tx_in_flight = false;
bool use_irq = false;

constexpr UART_HandleTypeDef* the_uart = &huart6;
constexpr int timeout_ms = 200;

char recv_char() {
  if (use_irq) {
    while (true) {
      poll_peripherals();
      auto no_irq = IrqGuard();
      auto next = rx.pop(no_irq);
      if (next) {
        return next;
      }
    }
  } else {
    uint8_t c = 0;
    poll_peripherals();
    HAL_UART_Receive(the_uart, &c, 1, timeout_ms);
    return static_cast<char>(c);
  }
}

} // namespace

std::string_view recv(std::span<char> buf) {
  char* const begin = buf.data();
  char* const end = begin + buf.size();
  char* it = begin;

  while (it != end) {
    char c = recv_char();
    if (c == 0) {
      continue;
    }
    send(std::string_view(&c, &c+1));
    if (c == '\r') {
      send("\n");
      break;
    }
    *it++ = c;
  }

  return std::string_view(begin, it);
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* u) {
  static volatile char next_byte = 0;
  if (next_byte) {
    rx.push(next_byte, IrqGuard());
  }
  HAL_UART_Receive_IT(u, (uint8_t*) &next_byte, 1);
}


void send(std::string_view line) {
  if (use_irq) {
    auto no_irq = IrqGuard();
    for (char c: line) {
      tx.push(c, no_irq);
    }
    // Kickstart tx if needed
    if (!tx_in_flight) {
      tx_in_flight = true;
      HAL_UART_TxCpltCallback(the_uart);
    }
  } else {
    HAL_UART_Transmit(the_uart, (const uint8_t*) line.data(), line.size(), timeout_ms);
  }
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* u) {
  static volatile char next_byte;
  auto no_irq = IrqGuard();
  next_byte = tx.pop(no_irq);
  if (next_byte) {
    HAL_UART_Transmit_IT(u, (const uint8_t*) &next_byte, 1);
  } else {
    tx_in_flight = false;
  }
}

void sendln(std::string_view line) {
  send(line);
  send("\r\n");
}


void toggle_irq_mode() {
  use_irq ^= true;
  if (use_irq) {
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    HAL_UART_RxCpltCallback(the_uart); // Kickstart rx
  } else {
    HAL_UART_AbortReceive(the_uart);
    HAL_NVIC_DisableIRQ(USART6_IRQn);
    tx_in_flight = false;
  }
  send("Async mode is now ");
  sendln(use_irq ? "ON" : "OFF");
}

} // namespace uart
