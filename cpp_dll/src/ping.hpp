#ifndef PING_HPP
#define PING_HPP

#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <concepts>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

#include "icmp_header.hpp"
#include "ipv4_header.hpp"
#include "ipv6_header.hpp"

namespace net {
struct use_ipv4_t {};
struct use_ipv6_t {};

inline constexpr use_ipv4_t use_ipv4;
inline constexpr use_ipv6_t use_ipv6;

template <class IPType> struct ip_token {};

template <> struct ip_token<use_ipv4_t> {
  using type = ipv4_header;
};

template <> struct ip_token<use_ipv6_t> {
  using type = ipv6_header;
};

template <class IPType>
using ip_token_to_header_t =
    typename ip_token<std::remove_cvref_t<IPType>>::type;

template <class IPType>
inline constexpr bool is_ip_token_v =
    std::is_same_v<IPType, use_ipv4_t> || std::is_same_v<IPType, use_ipv6_t>;

template <class HeaderType, class OHT = std::remove_cvref_t<HeaderType>>
  requires std::is_same_v<OHT, ipv4_header> || std::is_same_v<OHT, ipv6_header>
struct icmp_compose {
  OHT ipv4header;
  icmp_header icmpheader;
  std::size_t length;
  std::chrono::steady_clock::duration elapsed;
};

template <class IPType, class DurationRepType, class DurationPeriodType,
          class OIPT = std::remove_cvref_t<IPType>>
  requires is_ip_token_v<OIPT>
inline asio::awaitable<std::vector<icmp_compose<ip_token_to_header_t<OIPT>>>>
async_ping(std::string_view dest, int count, int ttl,
           const std::chrono::duration<DurationRepType, DurationPeriodType> &timeout,
           IPType &&type) {
  using namespace asio::experimental::awaitable_operators;

  constexpr bool is_v4 = std::is_same_v<OIPT, use_ipv4_t>;

  auto executor = co_await asio::this_coro::executor;
  asio::ip::icmp::resolver resolver(executor);

  auto get_icmp = [] -> asio::ip::icmp {
    if constexpr (is_v4) {
      return asio::ip::icmp::v4();
    } else {
      return asio::ip::icmp::v6();
    }
  };

  asio::ip::icmp::endpoint destination =
      *resolver.resolve(get_icmp(), dest, "").begin();
  asio::ip::icmp::socket socket(executor, get_icmp());
  std::string body("\"Hello!\" from Asio ping.");
  asio::streambuf reply_buffer;

  if constexpr (is_v4) {
    if (ttl != 64) {
      asio::ip::unicast::hops hops(ttl);
      socket.set_option(hops);
    }
  }

  auto get_identifier = [] -> unsigned short {
#if defined(ASIO_WINDOWS)
    return static_cast<unsigned short>(::GetCurrentProcessId());
#else
    return static_cast<unsigned short>(::getpid());
#endif
  };

  std::vector<icmp_compose<ip_token_to_header_t<OIPT>>> composes;
  for (int sequence_number = 0; sequence_number < count; ++sequence_number) {
    // Create an ICMP header for an echo request.
    icmp_header echo_request;
    if constexpr (is_v4) {
      echo_request.type(icmp_header::echo_request);
    } else {
      echo_request.type(icmp_header::v6_echo_request);
    }
    echo_request.code(0);
    echo_request.identifier(get_identifier());
    echo_request.sequence_number(sequence_number);
    compute_checksum(echo_request, body.begin(), body.end());

    // Encode the request packet.
    asio::streambuf request_buffer;
    std::ostream os(&request_buffer);
    os << echo_request << body;

    asio::steady_timer timer(executor);

    timer.expires_after(timeout);

    socket.async_send_to(request_buffer.data(), destination, asio::detached);

    reply_buffer.consume(reply_buffer.size());
    auto time_sent = std::chrono::steady_clock::now();
    auto value = co_await (socket.async_receive(reply_buffer.prepare(65536),
                                                asio::use_awaitable) ||
                           timer.async_wait(asio::use_awaitable));

    auto now = std::chrono::steady_clock::now();
    auto value_ptr = std::get_if<std::size_t>(&value);
    if (!value_ptr) {
      composes.emplace_back(ip_token_to_header_t<OIPT>{}, icmp_header{}, 0,
                            std::chrono::nanoseconds(0));
      continue;
    }

    std::size_t length = *value_ptr;

    reply_buffer.commit(length);
    std::istream is(&reply_buffer);
    ip_token_to_header_t<OIPT> ip_hdr;
    icmp_header icmp_hdr;
    if constexpr (is_v4) {
      is >> ip_hdr >> icmp_hdr;
    } else {
      is >> icmp_hdr;
    }

    if (is/* && icmp_hdr.type() == (is_v4 ? icmp_header::echo_reply : icmp_header::v6_echo_reply)
          && icmp_hdr.identifier() == get_identifier()
          && icmp_hdr.sequence_number() == sequence_number*/)
    {
      auto elapsed = now - time_sent;
      composes.emplace_back(std::move(ip_hdr), std::move(icmp_hdr), length,
                            std::move(elapsed));
    }
  }

  co_return composes;
}
} // namespace net

#endif // PING_HPP
