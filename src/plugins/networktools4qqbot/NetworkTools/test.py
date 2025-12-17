import network_utils_externel_cpp
import json

print(json.dumps(network_utils_externel_cpp.ping("www.baidu.com", 4, 64, 3), indent=4)) # host times ttl timeout
print(json.dumps(network_utils_externel_cpp.pingv6("::1", 4, 64, 3), indent=4))
print(json.dumps(network_utils_externel_cpp.tracert("www.baidu.com", 30, 1), indent=4)) # host hops timeout
