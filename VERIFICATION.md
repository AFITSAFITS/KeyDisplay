# v1.3.1 验证记录

## 已执行

- 使用 build-mingw.sh 严格交叉编译，C++17、-Wall -Wextra -Werror，通过，无警告。
- 输入状态和显示时长仍使用原共享逻辑；29 项断言通过，启用 AddressSanitizer/UndefinedBehaviorSanitizer，无检测错误。
- PE32+ x64 GUI；exe 大小254464字节，仅系统DLL，无外部MinGW运行库。
- SHA256：baf66f414e6e2ef0166c413ff9f90bf034d69e32e828967af2366964caa6ea5f。
- 导入表含 CreateMenu、SetMenu、DestroyWindow、PostQuitMessage、RegisterRawInputDevices、UpdateLayeredWindow。
- 静态检查控制窗口创建：WS_EX_APPWINDOW、WS_CAPTION、WS_SYSMENU、WS_MINIMIZEBOX；启动可见，普通窗口不置顶，区别于透明全屏叠加层。
- 退出按钮、导航退出菜单、托盘退出及主窗口WM_CLOSE统一调用幂等QuitApplication。
- QuitApplication先隐藏叠加层、取消计时、注销Raw Input、清空显示状态、删除托盘、销毁窗口，最后PostQuitMessage；消息循环返回后FreeSurface、DeleteObject字体、GdiplusShutdown及CloseHandle。
- RenderNow/HandleRawInput/ArmExpiry在退出状态下不再采集或重绘。

## 未验证

当前环境为macOS，无可用Windows实机运行环境。未验证主窗口真实显示、按钮与×点击、Alt+F4、托盘回调、资源释放实测、设置窗口焦点/退出行为及穿透回归。

CMake/Visual Studio配置已更新版本，但未执行该构建路径。29项断言仅覆盖共享状态逻辑，不能证明Windows菜单和退出流程正确；退出相关检查目前为源码/导入表静态检查。

延时和系统资源占用仍未标定，不承诺零延时或硬实时。
