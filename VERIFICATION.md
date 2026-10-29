# v1.3 验证记录

## 已执行

- 共享状态逻辑：clang++ C++17 编译，启用 `-Wall -Wextra -Werror` 和 AddressSanitizer/UndefinedBehaviorSanitizer；29 项断言通过，无检测错误。
- Windows GUI：使用交付的 `build-mingw.sh` 实际交叉编译，`-Wall -Wextra -Werror` 无警告通过。
- PE 回读：MZ/PE 签名正确，x64、PE32+ GUI；Windows exe 大小 251904 字节。
- SHA256：`e409618ee51afc3de86c807b01c9d5eb6936aff67bcf6b74b8ce529e01a4a62f`。
- 导入表检查：包含 Raw Input / UpdateLayeredWindow / Shell_NotifyIconW / WritePrivateProfileStringW / GetTickCount64；仅依赖 Windows 系统 DLL，无 MinGW 外部运行库 DLL。
- 导入表未包含 BlockInput、SendInput、SetWindowsHookEx、SetCapture、SetFocus 等拦截/注入/焦点 API；输入注册为 RIDEV_INPUTSINK，无 RIDEV_NOLEGACY。
- 源码执行路径检查：新事件直接调用 DisplayEvent → RenderNow → ArmExpiry，无 33ms 固定批次刷新；计时器仅管理隐藏。

## 尚未验证

当前构建环境为 macOS，不具备可用 Windows 实机运行环境。

以下必须在目标 Windows 上复测，不能由编译/静态检查代替：托盘操作、设置窗口交互、配置文件实际保存和恢复、跨进程点击穿透回归、气泡位置和字符渲染、系统负载下显示时序、真实延时与资源占用。

CMake/Visual Studio 构建配置已提供，但没有在本次环境执行。无“零延时”“硬实时”或毫秒级光子显示保证。
