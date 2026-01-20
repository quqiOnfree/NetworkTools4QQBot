# NetworkTools4QQBot
这是一个依赖onebot的异步网络工具库，用于提供QQ机器人网络工具服务。
此工具库包含Ping、Tracert、Nsloopup、Nmap等命令，方便随时调试。

## 编译Cpp模块
为了实现与本地原生工具等价的延迟测量工具，用Python写是不现实的。
我们一开始使用Python写延迟测量工具，Python Runtime的内部延迟为`7-10ms`，远远达不到我们延迟测量工具的使用要求，所以我们使用Cpp重构了`Ping`、`Tracert`等命令。
1. 用CMake自动生成项目配置文件
   ```powershell
   cd cpp_dll #指定运行目录到cpp_dll
   mkdir build
   cd build
   cmake .. #生成项目配置文件
   ```
2. 构建项目
   ```powershell
   #依旧在build目录
   cmake --build . --config Release #生成Release二进制库，自动转移到bot的本地目录
   ```

## 运行插件
### Windows
1. 在`windwos`系统下，要打开icmp-v4与icmp-v6的防火墙
  ```powershell
  netsh advfirewall firewall add rule name="All ICMP v4" dir=in action=allow protocol=icmpv4:any,any
  netsh advfirewall firewall add rule name="All ICMP v6" dir=in action=allow protocol=icmpv6:any,any
  ```
2. 运行插件
  ```powershell
  nb run
  ```

### Linux
运行此插件需要管理员权限，以接收icmp包。
```bash
nb run
```

## 插件用法
| 功能 | 命令格式 | 命令示例 | PS |
| --- | --- | --- | --- |
| Ping | `ping <host> [-4\|-6] [-i ttl] [-w timeout] [count]` | `/ping github.com -4 -i 64 -w 5000 3` |  |
| Whois | `whois <domain>` | `whois example.com` |  |
| Tracert |  |  | cpp api已实现，python调用接口还未完成 |
| Nslookup |  |  | 未实现 |
