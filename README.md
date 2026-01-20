# NetworkTools4QQBot
这是一个依赖onebot的异步网络工具库，用于提供QQ机器人网络工具服务。
此工具库包含Ping、Tracert、Nsloopup、Nmap等命令，方便随时调试。

## 运行插件
切记在`windwos`系统下，要打开icmp-v4与icmp-v6的防火墙
```powershell
netsh advfirewall firewall add rule name="All ICMP v4" dir=in action=allow protocol=icmpv4:any,any
netsh advfirewall firewall add rule name="All ICMP v6" dir=in action=allow protocol=icmpv6:any,any
```

1. generate project using `nb create` .
2. create your plugin using `nb plugin create` .
3. writing your plugins under `src/plugins` folder.
4. run your bot using `nb run --reload` .

## 插件用法
| 功能 | 命令格式 | 命令示例 | PS |
| --- | --- | --- | --- |
| Ping | `ping <host> [-4\|-6] [-i ttl] [-w timeout] [count]` | `/ping github.com -4 -i 64 -w 5000 3` |  |
| Whois | `whois <domain>` | `whois example.com` |  |
| Tracert |  |  | cpp api已实现，python调用接口还未完成 |
| Nslookup |  |  | 未实现 |

## Documentation

See [Docs](https://nonebot.dev/)
