import network_utils_externel_cpp
import json

print(json.dumps(network_utils_externel_cpp.ping("183.6.16.5", 1000, 64, 1000), indent=4)) # host times ttl timeout
print(json.dumps(network_utils_externel_cpp.pingv6("::1", 4, 64, 500), indent=4))
print(json.dumps(network_utils_externel_cpp.tracert("qqof.net", 30, 10), indent=4)) # host hops timeout
print(json.dumps(network_utils_externel_cpp.tracertv6("::1", 30, 10), indent=4)) # host hops timeout
print(json.dumps(network_utils_externel_cpp.tcping("127.0.0.1", 25565, 1000), indent=4)) # host port timeout
