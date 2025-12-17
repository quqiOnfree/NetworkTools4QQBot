//
// ipv4_header.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef IPV4_HEADER_HPP
#define IPV4_HEADER_HPP

#include <algorithm>
#include <asio/ip/address_v4.hpp>
#include <iostream>

// Packet header for IPv4.
//
// The wire format of an IPv4 header is:
//
// 0               8               16                             31
// +-------+-------+---------------+------------------------------+      ---
// |       |       |               |                              |       ^
// |version|header |    type of    |    total length in bytes     |       |
// |  (4)  | length|    service    |                              |       |
// +-------+-------+---------------+-+-+-+------------------------+       |
// |                               | | | |                        |       |
// |        identification         |0|D|M|    fragment offset     |       |
// |                               | |F|F|                        |       |
// +---------------+---------------+-+-+-+------------------------+       |
// |               |               |                              |       |
// | time to live  |   protocol    |       header checksum        |   20 bytes
// |               |               |                              |       |
// +---------------+---------------+------------------------------+       |
// |                                                              |       |
// |                      source IPv4 address                     |       |
// |                                                              |       |
// +--------------------------------------------------------------+       |
// |                                                              |       |
// |                   destination IPv4 address                   |       |
// |                                                              |       v
// +--------------------------------------------------------------+      ---
// |                                                              |       ^
// |                                                              |       |
// /                        options (if any)                      /    0 - 40
// /                                                              /     bytes
// |                                                              |       |
// |                                                              |       v
// +--------------------------------------------------------------+      ---

namespace net {
class ipv4_header {
public:
  ipv4_header() noexcept { std::fill(rep_, rep_ + sizeof(rep_), 0); }
  ~ipv4_header() noexcept = default;

  ipv4_header(const ipv4_header &ih) noexcept {
    std::copy(ih.rep_, ih.rep_ + sizeof(rep_), rep_);
  }

  ipv4_header(ipv4_header &&ih) noexcept {
    std::copy(ih.rep_, ih.rep_ + sizeof(rep_), rep_);
  }

  unsigned char version() const noexcept { return (rep_[0] >> 4) & 0xF; }
  unsigned short header_length() const noexcept { return (rep_[0] & 0xF) * 4; }
  unsigned char type_of_service() const noexcept { return rep_[1]; }
  unsigned short total_length() const noexcept { return decode(2, 3); }
  unsigned short identification() const noexcept { return decode(4, 5); }
  bool dont_fragment() const noexcept { return (rep_[6] & 0x40) != 0; }
  bool more_fragments() const noexcept { return (rep_[6] & 0x20) != 0; }
  unsigned short fragment_offset() const noexcept {
    return decode(6, 7) & 0x1FFF;
  }
  unsigned int time_to_live() const noexcept { return rep_[8]; }
  unsigned char protocol() const noexcept { return rep_[9]; }
  unsigned short header_checksum() const noexcept { return decode(10, 11); }

  asio::ip::address_v4 source_address() const {
    asio::ip::address_v4::bytes_type bytes = {
        {rep_[12], rep_[13], rep_[14], rep_[15]}};
    return asio::ip::address_v4(bytes);
  }

  asio::ip::address_v4 destination_address() const {
    asio::ip::address_v4::bytes_type bytes = {
        {rep_[16], rep_[17], rep_[18], rep_[19]}};
    return asio::ip::address_v4(bytes);
  }

  friend std::istream &operator>>(std::istream &is, ipv4_header &header) {
    is.read(reinterpret_cast<char *>(header.rep_), 20);
    if (header.version() != 4)
      is.setstate(std::ios::failbit);
    std::streamsize options_length = header.header_length() - 20;
    if (options_length < 0 || options_length > 40)
      is.setstate(std::ios::failbit);
    else
      is.read(reinterpret_cast<char *>(header.rep_) + 20, options_length);
    return is;
  }

private:
  unsigned short decode(int a, int b) const noexcept {
    return (rep_[a] << 8) + rep_[b];
  }

  unsigned char rep_[60];
};
} // namespace net

#endif // IPV4_HEADER_HPP
