#pragma once
#include <span>
#include <string_view>

namespace uart {

std::string_view recv(std::span<char> buf);
void send(std::string_view);
void sendln(std::string_view);

void toggle_irq_mode();

} // namespace uart
