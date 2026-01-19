#include <asio.hpp>
#include <chrono>
#include <format>
#include <iostream>
#include <pybind11/pybind11.h>
#include <ranges>
#include <sstream>

#include "icmp_header.hpp"
#include "ipv4_header.hpp"
#include "ping.hpp"
#include "tcping.hpp"

namespace py = pybind11;

static inline py::dict make_status_dict(std::string_view status,
                                        std::string_view message) {
  py::dict dict;
  dict["status"] = status;
  dict["message"] = message;
  return dict;
}

template <class IPType>
py::list ping(const std::string &dest, int count, int ttl, int timeout) {
  using OriginalIPType = std::remove_cvref_t<IPType>;
  asio::io_context io_context;
  auto future = asio::co_spawn(
      io_context,
      net::async_ping(dest, count, ttl, std::chrono::milliseconds(timeout),
                      OriginalIPType{}),
      asio::use_future);
  io_context.run();
  py::list list;
  if (future.wait_for(std::chrono::nanoseconds(0)) ==
      std::future_status::deferred) {
    list.append(make_status_dict("error",
                                 "error occurred, the task was not processed"));
    return list;
  }
  std::vector<net::icmp_compose<net::ip_token_to_header_t<OriginalIPType>>>
      composes;
  try {
    composes = future.get();
  } catch (const std::exception &e) {
    list.append(make_status_dict("error", e.what()));
    return list;
  }
  for (const auto &[ipv4_hdr, icmp_hdr, length, elapsed] : composes) {
    if (!length) {
      list.append(make_status_dict("error", "timeout"));
      continue;
    }
    py::dict dict =
        make_status_dict("success", "successfully receive the icmp package");
    dict["bytes"] = length - ipv4_hdr.header_length();
    dict["address"] = ipv4_hdr.source_address().to_string();
    dict["icmp_seq"] = icmp_hdr.sequence_number();
    dict["ttl"] = ipv4_hdr.time_to_live();
    dict["time"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    list.append(std::move(dict));
  }
  return list;
}

template <class IPType>
py::list tracert(const std::string &dest, int hops_count, int timeout) {
  using OriginalIPType = std::remove_cvref_t<IPType>;
  bool can_ping = false;
  {
    asio::io_context io_context;
    auto future = asio::co_spawn(
        io_context,
        net::async_ping(dest, 3, 64, std::chrono::milliseconds(1000),
                        OriginalIPType{}),
        asio::use_future);
    io_context.run();
    py::list list;
    if (future.wait_for(std::chrono::nanoseconds(0)) ==
        std::future_status::deferred) {
      list.append(make_status_dict(
          "error", "error occurred, the task was not processed"));
      return list;
    }
    std::vector<net::icmp_compose<net::ip_token_to_header_t<OriginalIPType>>>
        composes;
    try {
      composes = future.get();
    } catch (const std::exception &e) {
      list.append(make_status_dict("error", e.what()));
      return list;
    }
    for (const auto &[ipv4_hdr, icmp_hdr, length, elapsed] : composes) {
      if (length) {
        can_ping = true;
        break;
      }
    }
  }
  py::list list;
  asio::ip::icmp::endpoint destination;
  try {
    asio::io_context io_context;
    asio::ip::icmp::resolver resolver(io_context);
    constexpr bool is_v4 = std::is_same_v<OriginalIPType, net::use_ipv4_t>;
    constexpr auto get_icmp = [] -> asio::ip::icmp {
      if constexpr (is_v4) {
        return asio::ip::icmp::v4();
      } else {
        return asio::ip::icmp::v6();
      }
    };
    destination = *resolver.resolve(get_icmp(), dest, "").begin();
  } catch (const std::exception &e) {
    list.append(make_status_dict("error", e.what()));
    return list;
  }
  std::size_t valid_idx = 0;
  for (auto ttl : std::views::iota(1) | std::views::take(hops_count)) {
    asio::io_context io_context;
    auto future = asio::co_spawn(
        io_context,
        net::async_ping(dest, 3, ttl, std::chrono::milliseconds(timeout),
                        OriginalIPType{}),
        asio::use_future);
    io_context.run();
    if (future.wait_for(std::chrono::nanoseconds(0)) ==
        std::future_status::deferred) {
      list.append(make_status_dict("error", "timeout"));
      continue;
    }
    std::vector<net::icmp_compose<net::ip_token_to_header_t<OriginalIPType>>>
        composes;
    try {
      composes = future.get();
    } catch (const std::exception &e) {
      list.append(make_status_dict("error", e.what()));
      continue;
    }
    py::dict local_dict = make_status_dict("success", "successfuly tested");
    local_dict["ttl"] = ttl;
    asio::ip::address address;
    py::list local_list;
    for (const auto &[ipv4_hdr, _1, length, elapsed] : composes) {
      if (address.is_unspecified() &&
          !ipv4_hdr.source_address().is_unspecified()) {
        address = ipv4_hdr.source_address();
      }
      local_list.append(
          length
              ? std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                    .count()
              : -1);
    }
    local_dict["delay"] = std::move(local_list);
    if (!address.is_unspecified()) {
      valid_idx = ttl - 1;
      local_dict["address"] = address.to_string();
    } else {
      local_dict["address"] = "timeout";
    }
    list.append(std::move(local_dict));
    if (address == destination.address() ||
        (ttl - valid_idx > 10 && !can_ping)) {
      break;
    }
  }
  return list;
}

py::dict tcping(const std::string &host, std::uint16_t port, int timeout) {
  asio::io_context io_context;
  auto future = asio::co_spawn(
      io_context,
      net::async_tcping(host, port, std::chrono::milliseconds(timeout)),
      asio::use_future);
  io_context.run();
  if (future.wait_for(std::chrono::nanoseconds(0)) ==
      std::future_status::deferred) {
    py::dict dict =
        make_status_dict("error", "error occurred, the task was not processed");
    return dict;
  }
  try {
    auto delay = future.get();
    py::dict dict = make_status_dict("success", "successfully tested");
    dict["value"] = delay.count();
    return dict;
  } catch (const std::exception &e) {
    py::dict dict = make_status_dict("error", e.what());
    return dict;
  } catch (...) {
    py::dict dict = make_status_dict("error", "Unknown error occurred");
    return dict;
  }
}

PYBIND11_MODULE(network_utils_externel_cpp, m) {
  m.doc() = "A Cpp network utils module for python";
  m.def("ping", &ping<decltype(net::use_ipv4)>, "ping the destination");
  m.def("pingv6", &ping<decltype(net::use_ipv6)>,
        "ping the destination in ipv6");
  m.def("tracert", &tracert<decltype(net::use_ipv4)>,
        "tracert the destination");
  m.def("tracertv6", &tracert<decltype(net::use_ipv6)>,
        "tracert the destination in ipv6");
  m.def("tcping", &tcping, "tcping a host");
}
