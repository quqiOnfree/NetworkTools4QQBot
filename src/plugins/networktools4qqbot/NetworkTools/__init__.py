import json

from nonebot import on_command
from nonebot.adapters import Message
from nonebot.params import CommandArg
from nonebot.exception import FinishedException

from . import network_utils_externel_cpp
from .utils.format import format_ping_result, format_whois_result
from .utils.resolver import resolve_domain
from .utils.whois import whois_query

__plugin_name__ = "NetworkTools"
__plugin_usage__ = """
NetworkTools 插件提供了一些网络相关的工具命令，例如 ping、traceroute 等。
"""
ping = on_command("ping", priority=5, block=True)
whois = on_command("whois", priority=5, block=True)

@ping.handle()
async def handle_ping(args: Message = CommandArg()):
    if args:
        arg_list = args.extract_plain_text().split()
        # await ping.send(f"Received ping command with args: {arg_list}")
        if len(arg_list) >= 1:
            host = arg_list[0]
            count = 4
            ttl = 64
            timeout = 1000
            use_ipv4 = False
            use_ipv6 = False
            start_msg = []
            
            start_msg.append(f"开始准备发起PING：")
            
            # 遍历参数
            i = 1
            while i < len(arg_list):
                if arg_list[i] == "-4":
                    force_ipv4 = True
                elif arg_list[i] == "-6":
                    force_ipv6 = True
                elif arg_list[i] == "-i" and i + 1 < len(arg_list):
                    ttl = int(arg_list[i + 1])
                    i += 1
                elif arg_list[i] == "-w" and i + 1 < len(arg_list):
                    timeout = int(arg_list[i + 1])
                    i += 1
                else:
                    try:
                        count = int(arg_list[i])
                    except ValueError:
                        pass
                i += 1
            
            start_msg.append(f"主机名：{host}，次数：{count}, TTL：{ttl}, 超时：{timeout}毫秒")
        
            if 'force_ipv4' in locals() and 'force_ipv6' in locals():
                await ping.finish("不能同时指定 -4 和 -6 选项。")
            elif 'force_ipv4' in locals() or 'force_ipv6' in locals():
                # 用户强制指定了 IP 版本
                if 'force_ipv4' in locals():
                    use_ipv4 = True
                    start_msg.append("，用户强制使用 IPv4 进行测试。")
                elif 'force_ipv6' in locals():
                    use_ipv6 = True
                    start_msg.append("，用户强制使用 IPv6 进行测试。")
            else:
                # 判断是IP地址还是域名
                import ipaddress
                try:
                    ip = ipaddress.ip_address(host)
                    if ip.version == 4:
                        use_ipv4 = True
                        start_msg.append("，目标为 IPv4 地址。")
                    elif ip.version == 6:
                        use_ipv6 = True
                        start_msg.append("，目标为 IPv6 地址。")
                except ValueError:
                    # 如果不是有效的 IP 地址，则尝试 DNS 解析，传出地址（取第一个）
                    start_msg.append("，目标为域名，解析结果：")
                    a_records = await resolve_domain(host, "A")
                    aaaa_records = await resolve_domain(host, "AAAA")
                    if a_records:
                        use_ipv4 = True
                        start_msg.append(f"\nA记录：{a_records} ")
                    if aaaa_records:
                        use_ipv6 = True
                        start_msg.append(f"\nAAAA记录：{aaaa_records} ")
                    if not a_records and not aaaa_records:
                        await ping.finish(f"无法解析域名 {host} 的 A 或 AAAA 记录。")
            await ping.send("".join(start_msg))
            try:
                if use_ipv4 and use_ipv6:
                    await ping.send("发现这是一个双栈地址，同时进行 IPv4 和 IPv6 测试。")
                    try:
                        result4 = network_utils_externel_cpp.ping(host, count, ttl, timeout)
                        formatted_result4 = format_ping_result(result4)
                        await ping.send(f"IPv4 测试结果:\n{formatted_result4}")
                    except FinishedException:
                        raise
                    except Exception as e:
                        await ping.send(f"执行 IPv4 ping 命令时出错: {e}, 这可能是由于目标主机不可达或域名解析失败导致的。")
                    try:
                        result6 = network_utils_externel_cpp.pingv6(host, count, ttl, timeout)
                        formatted_result6 = format_ping_result(result6)
                        await ping.finish(f"IPv6 测试结果:\n{formatted_result6}")
                    except FinishedException:
                        raise
                    except Exception as e:
                        await ping.send(f"执行 IPv6 ping 命令时出错: {e}, 这可能是由于目标主机不可达或域名解析失败导致的。")
                elif use_ipv4:
                    result = network_utils_externel_cpp.ping(host, count, ttl, timeout)
                    formatted_result = format_ping_result(result)
                    await ping.finish(formatted_result)                       
                elif use_ipv6:
                    result = network_utils_externel_cpp.pingv6(host, count, ttl, timeout)
                    formatted_result = format_ping_result(result)
                    await ping.finish(formatted_result)


            except FinishedException:
                raise
            except Exception as e:
                await ping.finish(f"执行 ping 命令时出错: {e}, 这可能是由于目标主机不可达或域名解析失败导致的。")
            else:
                await ping.finish("未能正确识别到地址类型，请重试。")

    else:
        await ping.finish(f"用法: ping <host> [-4|-6] [-i ttl] [-w timeout] [count]\n示例: ping example.com -4 -i 64 -w 1000 4")

@whois.handle()
async def handle_whois(args: Message = CommandArg()):
    if args:
        arg_list = args.extract_plain_text().split()
        if len(arg_list) >= 1:
            domain = arg_list[0]
            raw = False
            # 遍历参数
            i = 1
            while i < len(arg_list):
                if arg_list[i] == "--raw":
                    raw = True
                    await whois.send("注意：启用 --raw 选项后，返回的结果可能非常冗长，可能会导致消息发送失败。")
                i += 1
            try:
                await whois.send(f"正在查询域名 {domain} 的 WHOIS 信息，请稍候...")
                result = await whois_query(domain, raw)
                formatted_result = await format_whois_result(result)
                if raw:
                    await whois.send(f"Whois 查询结果（原始数据）:\n{result.get('data', {}).get('raw', '无原始数据')}")
                if result.get("status") == 1:
                    if result.get("data", {}).get("is_available") == 1:
                        await whois.finish(f"域名 {domain} 可能未被注册。")
                    else:
                        await whois.finish(f"Whois 查询结果:\n{formatted_result}")

            except FinishedException:
                raise
            except Exception as e:
                await whois.finish(f"执行 whois 命令时出错: {e}, 这可能是由于查询失败导致的。")
    else:
        await whois.finish(f"用法: whois <domain>\n示例: whois example.com")
