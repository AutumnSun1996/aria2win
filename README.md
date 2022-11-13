# Aira2win 极简aria2 windows客户端

极简的aria2 windows客户端(exe大小为259KB, 无后台进程)

可关联.torrent等文件, 实现打开文件/命令行输入链接地址后自动触发下载

使用config.json配置aria2c jsonrpc, 支持远程下载


使用方式:
1. RPC配置:

在`aria2win.exe`的相同路径下创建`config.json`, 如:
```json
{
  "rpc_url": "http://localhost:6800/jsonrpc",
  "rpc_token": "",
  "rpc_id": "aria2win",
  "timeout": 10
}
```
以上为默认配置, 可根据实际情况进行调整. 其中`timeout`为请求jsonrpc的超时时间, 单位为s.

2. 通过文件关联配置, 打开文件时自动触发下载任务

将 .torrent/.meta4 文件关联到 aria2win.exe, 打开文件将自动触发下载任务.

手动配置: 右键点击目标文件->打开方式->选择其他应用->更多应用->查找其他应用->选择aria2win.exe

自动配置:
   1. 在aria2win.exe所在的文件夹点击Shift+右键
   2. 选择'在此处打开Powershell窗口'
   3. 执行脚本`SetFTA.ps1`(输入并按回车)

执行脚本`SetFTA.ps1`(基于[PS-SFTA](https://github.com/DanysysTeam/PS-SFTA)实现)


3. 通过命令行调用:

```cmd
aria2win.exe TARGET
```
`TARGET`可为本地种子文件地址/下载链接/magnet链接
