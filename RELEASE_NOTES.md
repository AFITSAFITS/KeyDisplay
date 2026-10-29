# KeyDisplay v1.3.0 — Windows x64

## 下载

下载附件 **KeyDisplay.exe**，在 Windows 10/11 x64 上双击运行，无需安装。

- 本机 Raw Input 键盘/鼠标事件，主屏右下角显示。
- 普通按键、组合键、鼠标左/右/中键、侧键、滚轮。
- 托盘右键 → 设置显示时长，单位 ms；默认 1200，范围 1～600000。
- 新事件收到立即提交绘制，显示时长只控制保留，不推迟下一条事件。
- 托盘菜单提供退出。

## 校验

`KeyDisplay.exe` 为 Windows x64 PE32+ GUI，大小 **251904 字节**。

SHA256：

```
e409618ee51afc3de86c807b01c9d5eb6936aff67bcf6b74b8ce529e01a4a62f
```

PowerShell：

```powershell
Get-FileHash .\KeyDisplay.exe -Algorithm SHA256
```

附件 `SHA256SUMS.txt` 提供同一校验值。`README.md` 包含使用、构建与限制说明；源码使用 MIT 许可证。

## 已验证与限制

已通过 29 项共享状态逻辑断言、严格 MinGW-w64 交叉编译、PE/导入表检查。只有系统 DLL 依赖，不需要单独安装 MinGW 运行库。

**v1.3 尚未完成 Windows 实机设置交互、配置恢复、穿透回归、真实延时与资源占用验证。** 本次发布不代表硬实时、零延时或硬件级时间戳。设置时长不是屏幕显示的毫秒精度保证，极短气泡可能录不到。请在真实被测配置下标定工具自身的显示延迟。

EXE 未签名，可能出现 SmartScreen 提示；不要求关闭系统安全功能。敏感输入/密码输入前请退出工具，即使没有日志也可能显示按键名称。

README 配图为**界面示意，非 Windows 实机截图**。
