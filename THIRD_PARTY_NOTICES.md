# 工具链与依赖说明

- 应用代码：MIT，见 LICENSE。
- 运行接口：Microsoft Win32、Raw Input、GDI+ 和 Windows Shell。所需 Windows 系统 DLL 不打包分发。
- 本次 EXE 使用 MinGW-w64 GCC 构建，静态链接 GCC C++/线程运行库。GCC 运行库具有 GPL 与 GCC Runtime Library Exception 等相应许可；静态库的工具链许可不由本项目的 MIT 许可证覆盖。
- 此源码包不包含或复制 GCC/MinGW 运行库源码。重新分发或采用其他工具链构建时，维护者应检查所用版本的第三方许可证及运行库例外。
- CMake 仅为可选构建工具；不作为程序运行时依赖。标准字体由 Windows 提供，没有嵌入第三方字体或图片。
