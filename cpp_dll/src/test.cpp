#include <asio.hpp>
#include <iostream>
#include <sstream>

#include "icmp_header.hpp"
#include "ipv4_header.hpp"
#include "ping.hpp"

std::string ping(const std::string& dest, std::size_t count)
{
    asio::io_context io_context;
    auto future = asio::co_spawn(io_context, async_ping(dest, count), asio::use_future);
    io_context.run();
    if (future.wait_for(std::chrono::nanoseconds(0)) == std::future_status::deferred)
    {
        return "Totally loss";
    }
    std::stringstream ss;
    for (const auto& [ipv4_hdr, icmp_hdr, length, elapsed]: future.get())
    {
        ss << length - ipv4_hdr.header_length()
        << " bytes from " << ipv4_hdr.source_address()
        << ": icmp_seq=" << icmp_hdr.sequence_number()
        << ", ttl=" << ipv4_hdr.time_to_live()
        << ", time="
        << chrono::duration_cast<chrono::milliseconds>(elapsed).count()
        << "ms\n";
    }
    std::string data = ss.str();
    if (data.empty()) {
        return "Totally lost";
    }
    return data;
}

int main(){
    std::cout << ping("www.baidu.com", 4) << '\n';
    asio::io_context io_context;
    pinger p(io_context, "www.baidu.com");
    io_context.run();
}
