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

namespace py = pybind11;

static inline py::dict make_status_dict(std::string_view status,
                                        std::string_view message) {
  py::dict dict;
  dict["status"] = status;
  dict["message"] = message;
  return dict;
}

py::list ping(const std::string &dest, int count, int ttl, int timeout) {
  asio::io_context io_context;
  auto future = asio::co_spawn(io_context,
                               net::async_ping(dest, count, ttl,
                                               std::chrono::milliseconds(timeout),
                                               net::use_ipv4),
                               asio::use_future);
  io_context.run();
  py::list list;
  if (future.wait_for(std::chrono::nanoseconds(0)) ==
      std::future_status::deferred) {
    list.append(make_status_dict("error",
                                 "error occurred, the task was not processed"));
    return list;
  }
  std::vector<net::icmp_compose<net::ipv4_header>> composes;
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

py::list pingv6(const std::string &dest, int count, int ttl, int timeout) {
  asio::io_context io_context;
  auto future = asio::co_spawn(io_context,
                               net::async_ping(dest, count, ttl,
                                               std::chrono::milliseconds(timeout),
                                               net::use_ipv6),
                               asio::use_future);
  io_context.run();
  py::list list;
  if (future.wait_for(std::chrono::nanoseconds(0)) ==
      std::future_status::deferred) {
    list.append(make_status_dict("error",
                                 "error occurred, the task was not processed"));
    return list;
  }
  std::vector<net::icmp_compose<net::ipv6_header>> composes;
  try {
    composes = future.get();
  } catch (const std::exception &e) {
    list.append(make_status_dict("error", e.what()));
    return list;
  }
  for (const auto &[_, icmp_hdr, length, elapsed] : composes) {
    if (!length) {
      list.append(make_status_dict("error", "timeout"));
      continue;
    }
    py::dict dict =
        make_status_dict("success", "successfully receive the icmp package");
    dict["bytes"] = length;
    dict["address"] = dest;
    dict["icmp_seq"] = icmp_hdr.sequence_number();
    dict["time"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    list.append(std::move(dict));
  }
  return list;
}

py::list tracert(const std::string &dest, int hops_count, int timeout) {
  bool can_ping = false;
  {
    asio::io_context io_context;
    auto future = asio::co_spawn(io_context,
                                net::async_ping(dest, 3, 64,
                                                std::chrono::milliseconds(1000),
                                                net::use_ipv4),
                                asio::use_future);
    io_context.run();
    py::list list;
    if (future.wait_for(std::chrono::nanoseconds(0)) ==
        std::future_status::deferred) {
      list.append(make_status_dict("error",
                                  "error occurred, the task was not processed"));
      return list;
    }
    std::vector<net::icmp_compose<net::ipv4_header>> composes;
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
    destination = *resolver.resolve(asio::ip::icmp::v4(), dest, "").begin();
  } catch (const std::exception &e) {
    list.append(make_status_dict("error", e.what()));
    return list;
  }
  std::size_t valid_idx = 0;
  for (auto ttl : std::views::iota(1) | std::views::take(hops_count)) {
    asio::io_context io_context;
    auto future = asio::co_spawn(io_context,
                                 net::async_ping(dest, 3, ttl,
                                                 std::chrono::milliseconds(timeout),
                                                 net::use_ipv4),
                                 asio::use_future);
    io_context.run();
    if (future.wait_for(std::chrono::nanoseconds(0)) ==
        std::future_status::deferred) {
      list.append(make_status_dict("error", "timeout"));
      continue;
    }
    std::vector<net::icmp_compose<net::ipv4_header>> composes;
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
    if (address == destination.address() || (ttl - valid_idx > 10 && !can_ping)) {
      break;
    }
  }
  return list;
}

PYBIND11_MODULE(network_utils_externel_cpp, m) {
  m.doc() = "A Cpp network utils module for python";
  m.def("ping", &ping, "ping the destination");
  m.def("pingv6", &pingv6, "ping the destination in ipv6");
  m.def("tracert", &tracert, "tracert the destination");
}
