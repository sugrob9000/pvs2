#include "main.h"
#include "uart.hpp"
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

using std::optional, std::nullopt;
using std::string_view;

namespace {

constexpr auto max_int = std::numeric_limits<int16_t>::max();

optional<int16_t> parse_num(string_view str) {
  if (str.empty() || str.length() > 5) {
    return nullopt;
  }
  int16_t result = 0;
  for (char c: str) {
    if (result > max_int / 10) {
      return nullopt;
    }
    result *= 10;
    if (c < '0' || c > '9') {
      return nullopt;
    }
    int nc = c - '0';
    if (max_int - nc < result) {
      return nullopt;
    }
    result += nc;
  }
  return result;
}

struct Expr {
  int16_t lhs;
  int16_t rhs;
  char op;
  optional<int16_t> calculate() const;
};

optional<Expr> parse_expr(string_view expr) {
  if (expr.empty() || expr.back() != '=') {
    return nullopt;
  }
  auto op_idx = expr.find_first_of("+-*/");
  if (op_idx == expr.npos) {
    return nullopt;
  }
  auto lhs = parse_num(expr.substr(0, op_idx));
  auto rhs = parse_num(expr.substr(op_idx + 1, expr.length() - op_idx - 2));
  if (!lhs || !rhs) {
    return nullopt;
  }
  return Expr {
    .lhs = *lhs,
    .rhs = *rhs,
    .op = expr[op_idx],
  };
}

optional<int16_t> Expr::calculate() const {
  switch (op) {
    case '+': return (max_int - rhs < lhs) ? nullopt : optional(rhs + lhs);
    case '-': return lhs - rhs;
    case '*': return (max_int / rhs < lhs) ? nullopt : optional(rhs * lhs);
    case '/': return rhs == 0 ? nullopt : optional(lhs / rhs);
    default: return nullopt;
  }
}

string_view fmt_num(int16_t num, std::span<char> buf) {
  if (buf.empty()) {
    return "";
  }
  bool negative = false;
  if (num < 0) {
    negative = true;
    num = -num;
  }

  auto it = buf.rbegin();
  auto end = buf.rend();

  do {
    *it++ = '0' + num % 10;
    num /= 10;
  } while (num != 0 && it != end);

  if (negative && it != end) {
    *it++ = '-';
  }

  return string_view(&*(it - 1), buf.data() + buf.size());
}

bool button_pressed() {
  return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == GPIO_PIN_RESET;
}

bool toggle_request = false;

} // namespace

extern "C" void main() {
  {
    extern "C" void init_all();
    init_all();
  }

  char buf[128];
  while (true) {
    if (std::exchange(toggle_request, false)) {
      uart::toggle_irq_mode();
    }
    string_view line = uart::recv(buf);
    if (auto expr = parse_expr(line)) {
      if (auto answer = expr->calculate()) {
        uart::sendln(fmt_num(*answer, buf));
        continue;
      }
    }
    uart::sendln("error");
  }
}


void poll_peripherals() {
  static uint32_t button_pressed_since = 0;
  if (button_pressed()) {
    if (button_pressed_since == 0) {
      button_pressed_since = HAL_GetTick();
    }
  } else {
    if (button_pressed_since != 0) {
      uint32_t time_pressed = HAL_GetTick() - button_pressed_since;
      if (time_pressed > 250) {
        toggle_request = true;
      }
    }
    button_pressed_since = 0;
  }
}
