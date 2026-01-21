#ifndef PING_HPP
#define PING_HPP

#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <concepts>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>

#include "icmp_header.hpp"
#include "ipv4_header.hpp"
#include "ipv6_header.hpp"

namespace net {
struct use_ipv4_t {};
struct use_ipv6_t {};

inline constexpr use_ipv4_t use_ipv4;
inline constexpr use_ipv6_t use_ipv6;

template <class IPType> struct ip_token_to_header {};

template <> struct ip_token_to_header<use_ipv4_t> {
  using type = ipv4_header;
};

template <> struct ip_token_to_header<use_ipv6_t> {
  using type = ipv6_header;
};

template <class IPType>
using ip_token_to_header_t = typename ip_token_to_header<IPType>::type;

template <class IPType>
inline constexpr bool is_ip_token_v =
    std::is_same_v<IPType, use_ipv4_t> || std::is_same_v<IPType, use_ipv6_t>;

template <class HeaderType>
  requires std::is_same_v<HeaderType, ipv4_header> ||
           std::is_same_v<HeaderType, ipv6_header>
struct icmp_compose {
  HeaderType ip_hd{};
  icmp_header icmp_hd{};
  std::size_t length{0};
  std::chrono::steady_clock::duration elapsed{};
};

inline std::uint16_t get_identifier() noexcept {
#if defined(ASIO_WINDOWS)
  return static_cast<unsigned short>(::GetCurrentProcessId());
#else
  return static_cast<unsigned short>(::getpid());
#endif
}

template <class IPType>
  requires is_ip_token_v<IPType>
class pinger {
public:
  using ip_type = IPType;
  using ip_header_type = ip_token_to_header_t<ip_type>;
  using icmp_compose_type = icmp_compose<ip_header_type>;

  pinger(asio::any_io_executor &exec) : executor(exec) {}
  pinger(const pinger &) = delete;
  pinger(pinger &&) = default;
  ~pinger() = default;

  pinger &operator=(const pinger &) = delete;
  pinger &operator=(pinger &&) = default;

  static asio::ip::icmp get_icmp_protocol() noexcept {
    if constexpr (is_v4) {
      return asio::ip::icmp::v4();
    } else {
      return asio::ip::icmp::v6();
    }
  }

  template <class DurationRepType, class DurationPeriodType>
  asio::awaitable<std::vector<icmp_compose_type>>
  async_ping(std::string_view dest, int count, int ttl,
             const std::chrono::duration<DurationRepType, DurationPeriodType>
                 &timeout) {
    if (count <= 0 || ttl <= 0) {
      co_return std::vector<icmp_compose_type>{};
    }

    asio::ip::icmp::endpoint destination{
        *resolver.resolve(get_icmp_protocol(), dest, "").begin()};
    asio::ip::unicast::hops hops(ttl);

    socket.set_option(hops);

    std::vector<icmp_compose_type> composes;
    for (int sequence_number = 0; sequence_number < count; ++sequence_number) {
      co_await ping_send_pack(destination, sequence_number);

      auto res = co_await ping_receive_pack(sequence_number, timeout);
      if (std::holds_alternative<icmp_compose_type>(res)) {
        composes.emplace_back(*std::get_if<icmp_compose_type>(&res));
      } else {
        composes.push_back({});
      }
    }
    co_return composes;
  }

protected:
  asio::awaitable<void>
  ping_send_pack(const asio::ip::icmp::endpoint &destination,
                 int sequence_number) {
    icmp_header echo_request;
    if constexpr (is_v4) {
      echo_request.type(std::to_underlying(icmp_header::ipv4::echo_request));
    } else {
      echo_request.type(std::to_underlying(icmp_header::ipv6::echo_request));
    }
    echo_request.code(0);
    echo_request.identifier(get_identifier());
    echo_request.sequence_number(sequence_number);
    compute_checksum(echo_request, body.begin(), body.end());

    // Encode the request packet.
    asio::streambuf request_buffer;
    std::ostream os(&request_buffer);
    os << echo_request << body;

    co_await socket.async_send_to(request_buffer.data(), destination,
                                  asio::use_awaitable);
    co_return;
  }

  template <class DurationRepType, class DurationPeriodType,
            std::size_t RecursionLimit = 256>
  asio::awaitable<std::variant<std::monostate, icmp_compose_type>>
  ping_receive_pack(
      int sequence_number,
      const std::chrono::duration<DurationRepType, DurationPeriodType> &timeout,
      std::size_t recursion_time = 0) {
    using namespace asio::experimental::awaitable_operators;

    asio::streambuf reply_buffer;
    asio::steady_timer timer(executor);
    asio::ip::icmp::endpoint sender;

    timer.expires_after(timeout);
    auto time_sent = std::chrono::steady_clock::now();
    auto value =
        co_await (socket.async_receive_from(reply_buffer.prepare(65536), sender,
                                            asio::use_awaitable) ||
                  timer.async_wait(asio::use_awaitable));
    auto now = std::chrono::steady_clock::now();
    auto value_ptr = std::get_if<std::size_t>(&value);
    if (!value_ptr) {
      co_return std::monostate{};
    }

    std::size_t length = *value_ptr;

    reply_buffer.commit(length);
    std::istream is(&reply_buffer);
    ip_header_type ip_hdr;
    icmp_header icmp_hdr;

    if (!is) {
      co_return std::monostate{};
    }
    if constexpr (is_v4) {
      is >> ip_hdr >> icmp_hdr;
    } else {
      is >> icmp_hdr;
      // populate ipv6 header source address from the sender endpoint
      ip_hdr.set_source_address(sender.address().to_v6());
    }

    if (icmp_hdr.type() ==
        (is_v4 ? std::to_underlying(icmp_header::ipv4::time_exceeded)
               : std::to_underlying(icmp_header::ipv6::time_exceeded))) {
      auto elapsed = now - time_sent;
      co_return icmp_compose_type{std::move(ip_hdr), std::move(icmp_hdr),
                                  length, std::move(elapsed)};
    } else if (icmp_hdr.type() ==
               (is_v4 ? std::to_underlying(icmp_header::ipv4::echo_reply)
                      : std::to_underlying(icmp_header::ipv6::echo_reply))) {
      if (icmp_hdr.identifier() != get_identifier()) {
        if (recursion_time < RecursionLimit) {
          co_return co_await ping_receive_pack(sequence_number, timeout,
                                               recursion_time + 1);
        } else {
          co_return std::monostate{};
        }
      }
      if (icmp_hdr.sequence_number() == sequence_number) {
        auto elapsed = now - time_sent;
        co_return icmp_compose_type{std::move(ip_hdr), std::move(icmp_hdr),
                                    length, std::move(elapsed)};
      } else if (recursion_time < RecursionLimit) {
        co_return co_await ping_receive_pack(sequence_number, timeout,
                                             recursion_time + 1);
      } else {
        co_return std::monostate{};
      }
    } else {
      co_return std::monostate{};
    }
  }

private:
  inline static const std::string body{"\"Hello!\" from Asio ping."};
  inline static constexpr bool is_v4 = std::is_same_v<ip_type, use_ipv4_t>;

  asio::any_io_executor &executor;
  asio::ip::icmp::resolver resolver{executor};

  asio::ip::icmp::socket socket{executor, get_icmp_protocol()};
};

template <class IPType, class DurationRepType, class DurationPeriodType>
  requires is_ip_token_v<std::remove_cvref_t<IPType>>
inline asio::awaitable<std::vector<
    icmp_compose<ip_token_to_header_t<std::remove_cvref_t<IPType>>>>>
async_ping(
    std::string_view dest, int count, int ttl,
    const std::chrono::duration<DurationRepType, DurationPeriodType> &timeout,
    IPType &&type) {
  auto executor = co_await asio::this_coro::executor;
  pinger<std::remove_cvref_t<IPType>> local_pinger{executor};
  co_return co_await local_pinger.async_ping(dest, count, ttl, timeout);
}
} // namespace net

#endif // PING_HPP
