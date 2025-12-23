import json
from .resolver import resolve_domain


def format_ping_result(result: list) -> str:
    # 汇总结果
    # 总传输包
    transmitted = len(result)
    # 总接收包
    received = sum(1 for r in result if r.get("status") == "success")
    # 丢包率
    loss_percentage = ((transmitted - received) / transmitted) * 100 if transmitted > 0 else 0
    # RTT 统计
    min_time = min((r.get("time") for r in result if r.get("status") == "success"), default=0)
    max_time = max((r.get("time") for r in result if r.get("status") == "success"), default=0)
    avg_time = sum((r.get("time") for r in result if r.get("status") == "success"), 0) / received if received > 0 else 0
    # 抖动
    stddev_time = (sum((r.get("time") - avg_time) ** 2 for r in result if r.get("status") == "success") / received) ** 0.5 if received > 0 else 0
    # 响应的地址
    address = None
    for r in result:
        if r.get("status") == "success":
            address = r.get("address")
            break
    # 判断地址是否为IP
    if address:
        import ipaddress
        try:
            ip = ipaddress.ip_address(address)
            address = str(ip)
        except ValueError:
            pass

    # 构建输出字符串
    output_lines = []
    if len(result) > 10:
        output_lines.append(f"结果过多，不显示明细结果。\n响应IP: {address}")
    else:
        output_lines.append(f"响应IP: {address}")
        for r in result:
            ttl_result = ""
            if r.get("status") == "success":
                if r.get("ttl") is not None:
                    ttl_result = f"ttl={r.get('ttl')}"
                output_lines.append(f"{r.get('bytes')} bytes from {r.get('address')}: icmp_seq={r.get('icmp_seq')}{ttl_result} time={r.get('time')} ms")
            else:
                output_lines.append(f"Request timeout")
    output_lines.append(f"\n--- Ping statistics ---")
    output_lines.append(f"{transmitted} packets transmitted, {received} packets received, {loss_percentage:.1f}% packet loss")
    if received > 0:
        output_lines.append(f"rtt min/avg/max/mdev = {min_time:.3f}/{avg_time:.3f}/{max_time:.3f}/{stddev_time:.3f} ms")


    return "\n".join(output_lines)