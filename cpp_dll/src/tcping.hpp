#ifndef TCPING_HPP
#define TCPING_HPP

#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <type_traits>

namespace net {

asio::awaitable<std::chrono::steady_clock::duration>
async_tcping(std::string_view host, std::uint16_t port) {
  using namespace asio::experimental::awaitable_operators;

  auto executor = co_await asio::this_coro::executor;
  asio::ip::tcp::resolver resolver(executor);
  asio::ip::tcp::socket socket(executor);

  auto port_str = std::to_string(port);
  auto endpoint = *resolver.resolve(host, port_str).begin();
  auto start = std::chrono::steady_clock::now();
  co_await socket.async_connect(endpoint, asio::use_awaitable);
  auto end = std::chrono::steady_clock::now();
  std::error_code ec;
  socket.close(ec);
  co_return (end - start);
}

} // namespace net

#endif // TCPING_HPP
