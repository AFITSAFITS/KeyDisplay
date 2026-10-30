// SPDX-License-Identifier: MIT
// KeyDisplay v1.3.1 — Win32 Raw Input, read-only input overlay.
// No hooks, injection, drivers, telemetry or input logging.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>
#include "display_state.h"

using namespace Gdiplus;
static constexpr wchar_t kVersion[] = L"KeyDisplay v1.3.1";
static constexpr UINT kExpireTimer = 1;
static constexpr UINT kTrayMessage = WM_APP + 1;
static constexpr UINT kTraySettings = 101;
static constexpr UINT kTrayExit = 102;
static constexpr UINT kDurationEdit = 201;
static constexpr UINT kApply = 202;
static constexpr UINT kSettingsClose = 203;
static constexpr UINT kSettingsStatus = 204;

static HINSTANCE g_instance = nullptr;
static HWND g_overlay = nullptr, g_control = nullptr, g_settings = nullptr;
static HFONT g_uiFont = nullptr;
static ULONG_PTR g_gdiToken = 0;
static NOTIFYICONDATAW g_tray{};
static std::wstring g_configPath;
static keydisplay::DisplayState g_state;
static bool g_visible = false;
static HDC g_memDC = nullptr;
static HBITMAP g_bitmap = nullptr, g_previousBitmap = nullptr;
static BYTE* g_pixels = nullptr;
static int g_width = 0, g_height = 0;
static bool g_quitting = false;
static bool g_rawRegistered = false;
static bool g_trayAdded = false;
static void QuitApplication();

static std::wstring KeyName(UINT vk) {
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= '0' && vk <= '9') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return L"Num" + std::to_wstring(vk - VK_NUMPAD0);
    switch (vk) {
    case VK_LCONTROL: case VK_RCONTROL: case VK_CONTROL: return L"Ctrl";
    case VK_LMENU: case VK_RMENU: case VK_MENU: return L"Alt";
    case VK_LSHIFT: case VK_RSHIFT: case VK_SHIFT: return L"Shift";
    case VK_LWIN: case VK_RWIN: return L"Win";
    case VK_RETURN: return L"Enter";
    case VK_SPACE: return L"Space";
    case VK_BACK: return L"Backspace";
    case VK_TAB: return L"Tab";
    case VK_ESCAPE: return L"Esc";
    case VK_DELETE: return L"Del";
    case VK_INSERT: return L"Ins";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_PRIOR: return L"PgUp";
    case VK_NEXT: return L"PgDn";
    case VK_UP: return L"Up";
    case VK_DOWN: return L"Down";
    case VK_LEFT: return L"Left";
    case VK_RIGHT: return L"Right";
    case VK_CAPITAL: return L"CapsLock";
    case VK_NUMLOCK: return L"NumLock";
    case VK_SCROLL: return L"ScrollLock";
    case VK_SNAPSHOT: return L"PrtSc";
    case VK_PAUSE: return L"Pause";
    case VK_APPS: return L"Menu";
    case VK_OEM_1: return L";";
    case VK_OEM_PLUS: return L"+";
    case VK_OEM_COMMA: return L",";
    case VK_OEM_MINUS: return L"-";
    case VK_OEM_PERIOD: return L".";
    case VK_OEM_2: return L"/";
    case VK_OEM_3: return L"`";
    case VK_OEM_4: return L"[";
    case VK_OEM_5: return L"\\";
    case VK_OEM_6: return L"]";
    case VK_OEM_7: return L"'";
    case VK_DECIMAL: return L"Num.";
    case VK_MULTIPLY: return L"Num*";
    case VK_ADD: return L"Num+";
    case VK_SUBTRACT: return L"Num-";
    case VK_DIVIDE: return L"Num/";
    default: break;
    }
    // Return a key name, never harvest text/IME content from another app.
    wchar_t name[64]{};
    const LONG scan = static_cast<LONG>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16);
    if (GetKeyNameTextW(scan, name, 64)) return name;
    return L"VK_" + std::to_wstring(vk);
}
static bool Modifier(UINT vk) {
    return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
           vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LCONTROL ||
           vk == VK_RCONTROL || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

static void FreeSurface() {
    if (g_memDC && g_previousBitmap) SelectObject(g_memDC, g_previousBitmap);
    if (g_bitmap) DeleteObject(g_bitmap);
    if (g_memDC) DeleteDC(g_memDC);
    g_bitmap = nullptr; g_memDC = nullptr; g_previousBitmap = nullptr;
    g_pixels = nullptr;
}
static bool CreateSurface() {
    FreeSurface();
    g_width = GetSystemMetrics(SM_CXSCREEN);
    g_height = GetSystemMetrics(SM_CYSCREEN);
    if (g_width <= 0 || g_height <= 0) return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = g_width;
    info.bmiHeader.biHeight = -g_height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    g_memDC = CreateCompatibleDC(nullptr);
    if (!g_memDC) return false;
    void* bits = nullptr;
    g_bitmap = CreateDIBSection(g_memDC, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!g_bitmap || !bits) { FreeSurface(); return false; }
    g_previousBitmap = static_cast<HBITMAP>(SelectObject(g_memDC, g_bitmap));
    g_pixels = static_cast<BYTE*>(bits);
    return true;
}

static void RenderNow() {
    if (g_quitting || !g_overlay || !g_pixels) return;
    const bool visible = g_state.Visible(GetTickCount64());
    std::memset(g_pixels, 0, static_cast<size_t>(g_width) * g_height * 4);
    {
        // Explicit PARGB surface: GDI+ writes premultiplied alpha for ULW_ALPHA.
        Bitmap surface(g_width, g_height, g_width * 4, PixelFormat32bppPARGB, g_pixels);
        Graphics graphics(&surface);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
        if (visible) {
            Font font(L"Arial", 36.0f, FontStyleBold, UnitPoint);
            StringFormat format;
            format.SetAlignment(StringAlignmentCenter);
            format.SetLineAlignment(StringAlignmentCenter);
            format.SetTrimming(StringTrimmingEllipsisCharacter);
            format.SetFormatFlags(StringFormatFlagsNoWrap);
            const REAL available = std::max(100.0f, static_cast<REAL>(g_width) - 48.0f);
            RectF layout(0, 0, available - 56, 90), measured;
            graphics.MeasureString(g_state.text.c_str(), -1, &font, layout, &format, &measured);
            const REAL bw = std::min(available, std::max(140.0f, measured.Width + 56.0f));
            const REAL bh = std::max(64.0f, measured.Height + 30.0f);
            RectF bubble(static_cast<REAL>(g_width) - bw - 24,
                         static_cast<REAL>(g_height) - bh - 24, bw, bh);
            GraphicsPath path;
            const REAL r = 24.0f;
            path.AddArc(bubble.X, bubble.Y, r, r, 180.0f, 90.0f);
            path.AddArc(bubble.X + bw - r, bubble.Y, r, r, 270.0f, 90.0f);
            path.AddArc(bubble.X + bw - r, bubble.Y + bh - r, r, r, 0.0f, 90.0f);
            path.AddArc(bubble.X, bubble.Y + bh - r, r, r, 90.0f, 90.0f);
            path.CloseFigure();
            SolidBrush background(Color(210, 24, 26, 36));
            SolidBrush foreground(Color(255, 255, 255, 255));
            graphics.FillPath(&background, &path);
            RectF textRect(bubble.X + 12, bubble.Y, bw - 24, bh);
            graphics.DrawString(g_state.text.c_str(), -1, &font, textRect, &format, &foreground);
        }
        graphics.Flush(FlushIntentionSync);
    }
    POINT destination{0, 0}, source{0, 0};
    SIZE size{g_width, g_height};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (!UpdateLayeredWindow(g_overlay, nullptr, &destination, &size, g_memDC,
                            &source, 0, &blend, ULW_ALPHA)) {
        // Fail safe: if rendering fails, remove the visible window rather than
        // leave a blocking fullscreen surface. The tray remains available.
        ShowWindow(g_overlay, SW_HIDE);
        g_visible = false;
        return;
    }
    if (!IsWindowVisible(g_overlay)) ShowWindow(g_overlay, SW_SHOWNOACTIVATE);
    g_visible = visible;
}
static void ArmExpiry() {
    if (g_quitting || !g_overlay) return;
    KillTimer(g_overlay, kExpireTimer);
    const auto remaining = g_state.Remaining(GetTickCount64());
    if (remaining) SetTimer(g_overlay, kExpireTimer,
                            std::max<UINT>(USER_TIMER_MINIMUM, remaining), nullptr);
}
static void DisplayEvent() {
    // Immediate submission on WM_INPUT, never wait for the display-duration timer.
    RenderNow();
    ArmExpiry();
}

static void HandleRawInput(LPARAM parameter) {
    if (g_quitting) return;
    UINT bytes = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(parameter), RID_INPUT, nullptr,
                       &bytes, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1) ||
        bytes < sizeof(RAWINPUTHEADER)) return;
    std::vector<BYTE> buffer(bytes);
    const UINT received = GetRawInputData(reinterpret_cast<HRAWINPUT>(parameter), RID_INPUT,
                                          buffer.data(), &bytes, sizeof(RAWINPUTHEADER));
    if (received == static_cast<UINT>(-1) || received != bytes) return;
    const auto* input = reinterpret_cast<const RAWINPUT*>(buffer.data());
    bool changed = false;
    const auto now = GetTickCount64();
    if (input->header.dwType == RIM_TYPEKEYBOARD &&
        bytes >= sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD)) {
        const auto& key = input->data.keyboard;
        UINT vk = key.VKey;
        if (vk == 0 || vk == 255) return;
        if (vk == VK_SHIFT) vk = MapVirtualKeyW(key.MakeCode, MAPVK_VSC_TO_VK_EX);
        if (vk == VK_CONTROL) vk = (key.Flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;
        if (vk == VK_MENU) vk = (key.Flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;
        if (key.Flags & RI_KEY_BREAK) g_state.KeyUp(vk);
        else {
            g_state.KeyDown(vk, KeyName(vk), Modifier(vk), now);
            changed = true;
        }
    } else if (input->header.dwType == RIM_TYPEMOUSE &&
               bytes >= sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE)) {
        const auto& mouse = input->data.mouse;
        const USHORT flags = mouse.usButtonFlags;
        auto emit = [&](const wchar_t* name) { g_state.Mouse(name, now); changed = true; };
        if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) emit(L"LMB");
        if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) emit(L"RMB");
        if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) emit(L"MMB");
        if (flags & RI_MOUSE_BUTTON_4_DOWN) emit(L"X1");
        if (flags & RI_MOUSE_BUTTON_5_DOWN) emit(L"X2");
        if (flags & RI_MOUSE_WHEEL)
            emit(static_cast<SHORT>(mouse.usButtonData) > 0 ? L"Scroll Up" : L"Scroll Down");
        if (flags & RI_MOUSE_HWHEEL)
            emit(static_cast<SHORT>(mouse.usButtonData) > 0 ? L"Scroll Right" : L"Scroll Left");
        // Mouse movement is deliberately ignored; it does not repaint the overlay.
    }
    if (changed) DisplayEvent();
}

static void LoadConfig() {
    wchar_t appData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData))) return;
    const std::wstring folder = std::wstring(appData) + L"\\KeyDisplay";
    if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return;
    g_configPath = folder + L"\\settings.ini";
    wchar_t value[32]{};
    GetPrivateProfileStringW(L"Display", L"DurationMs", L"1200", value, 32, g_configPath.c_str());
    std::uint32_t parsed = 0;
    if (keydisplay::ParseDuration(value, parsed)) g_state.durationMs = parsed;
}
static HWND AddControl(HWND owner, const wchar_t* type, const wchar_t* text,
                       DWORD style, int x, int y, int w, int h, UINT id) {
    HWND result = CreateWindowExW(type == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
                                  type, text, WS_CHILD | WS_VISIBLE | style,
                                  x, y, w, h, owner,
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                                  g_instance, nullptr);
    if (result) SendMessageW(result, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    return result;
}
static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE:
        AddControl(hwnd, L"STATIC", L"显示时长（ms）", 0, 20, 20, 170, 24, 0);
        AddControl(hwnd, L"EDIT", std::to_wstring(g_state.durationMs).c_str(),
                   WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 190, 17, 140, 28, kDurationEdit);
        SendDlgItemMessageW(hwnd, kDurationEdit, EM_SETLIMITTEXT, 6, 0);
        AddControl(hwnd, L"STATIC", L"范围：1～600000 ms；默认 1200 ms。", 0, 20, 59, 395, 24, 0);
        AddControl(hwnd, L"STATIC", L"新事件立即显示，不等待旧气泡消失。\n这里只设置保留时长，不设置输入采样间隔。", 0, 20, 94, 395, 48, 0);
        AddControl(hwnd, L"BUTTON", L"保存并应用", WS_TABSTOP | BS_DEFPUSHBUTTON,
                   20, 158, 130, 32, kApply);
        AddControl(hwnd, L"BUTTON", L"关闭", WS_TABSTOP, 166, 158, 92, 32, kSettingsClose);
        AddControl(hwnd, L"STATIC", L"", 0, 20, 206, 398, 38, kSettingsStatus);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == kApply && HIWORD(wp) == BN_CLICKED) {
            wchar_t value[32]{};
            GetDlgItemTextW(hwnd, kDurationEdit, value, 32);
            std::uint32_t parsed = 0;
            if (!keydisplay::ParseDuration(value, parsed)) {
                SetDlgItemTextW(hwnd, kSettingsStatus, L"请输入 1～600000 的整数毫秒值。未更改设置。");
                return 0;
            }
            if (g_configPath.empty() || !WritePrivateProfileStringW(L"Display", L"DurationMs",
                                std::to_wstring(parsed).c_str(), g_configPath.c_str())) {
                SetDlgItemTextW(hwnd, kSettingsStatus, L"设置文件写入失败，未更改原设置。");
                return 0;
            }
            g_state.durationMs = parsed;
            RenderNow();
            ArmExpiry();
            SetDlgItemTextW(hwnd, kSettingsStatus, L"已保存并立即应用；下次启动自动恢复。");
            return 0;
        }
        if (LOWORD(wp) == kSettingsClose) { DestroyWindow(hwnd); return 0; }
        break;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: g_settings = nullptr; return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}
static void OpenSettings() {
    if (g_settings) { ShowWindow(g_settings, SW_RESTORE); SetForegroundWindow(g_settings); return; }
    g_settings = CreateWindowExW(0, L"KeyDisplaySettings", L"KeyDisplay v1.3.1 — 设置",
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                    452, 298, g_control, nullptr, g_instance, nullptr);
    if (g_settings) { ShowWindow(g_settings, SW_SHOW); SetForegroundWindow(g_settings); }
    // Modeless window: the same message pump continues servicing WM_INPUT.
}
static void ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kTraySettings, L"设置");
    AppendMenuW(menu, MF_STRING, kTrayExit, L"退出");
    POINT point{}; GetCursorPos(&point);
    SetForegroundWindow(g_control);
    const UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                       point.x, point.y, 0, g_control, nullptr);
    PostMessageW(g_control, WM_NULL, 0, 0);
    DestroyMenu(menu);
    if (choice == kTraySettings) OpenSettings();
    if (choice == kTrayExit) QuitApplication();
}
static void QuitApplication() {
    // All exit entries use one idempotent shutdown path. Never hide-to-tray.
    if (g_quitting) return;
    g_quitting = true;
    if (g_overlay) {
        ShowWindow(g_overlay, SW_HIDE);
        KillTimer(g_overlay, kExpireTimer);
    }
    if (g_rawRegistered) {
        RAWINPUTDEVICE remove[2]{{0x01, 0x06, RIDEV_REMOVE, nullptr},
                                  {0x01, 0x02, RIDEV_REMOVE, nullptr}};
        RegisterRawInputDevices(remove, 2, sizeof(RAWINPUTDEVICE));
        g_rawRegistered = false;
    }
    g_state.Clear();
    if (g_trayAdded) {
        Shell_NotifyIconW(NIM_DELETE, &g_tray);
        g_trayAdded = false;
    }
    if (g_settings) DestroyWindow(g_settings);
    if (g_overlay && IsWindow(g_overlay)) DestroyWindow(g_overlay);
    if (g_control && IsWindow(g_control)) DestroyWindow(g_control);
    PostQuitMessage(0);
}
static LRESULT CALLBACK ControlProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    static const UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (message == taskbarCreated && !g_quitting) {
        g_trayAdded = Shell_NotifyIconW(NIM_ADD, &g_tray) != FALSE;
        return 0;
    }
    if (message == kTrayMessage) {
        if (lp == WM_RBUTTONUP) ShowTrayMenu();
        if (lp == WM_LBUTTONUP) {
            ShowWindow(g_control, SW_RESTORE);
            SetForegroundWindow(g_control);
        }
        if (lp == WM_LBUTTONDBLCLK) OpenSettings();
        return 0;
    }
    switch (message) {
    case WM_CREATE: {
        HMENU menu = CreateMenu();
        if (!menu) return -1;
        AppendMenuW(menu, MF_STRING, kTraySettings, L"设置");
        AppendMenuW(menu, MF_STRING, kTrayExit, L"退出");
        SetMenu(hwnd, menu);
        if (!AddControl(hwnd, L"BUTTON", L"设置", WS_TABSTOP | BS_PUSHBUTTON,
                        18, 18, 146, 42, kTraySettings) ||
            !AddControl(hwnd, L"BUTTON", L"退出", WS_TABSTOP | BS_PUSHBUTTON,
                        182, 18, 146, 42, kTrayExit)) return -1;
        AddControl(hwnd, L"STATIC", L"最小化继续显示；点退出或右上角 × 完全退出。",
                   0, 18, 78, 326, 50, 0);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == kTraySettings) { OpenSettings(); return 0; }
        if (LOWORD(wp) == kTrayExit) { QuitApplication(); return 0; }
        break;
    case WM_CLOSE:
        QuitApplication();
        return 0;
    case WM_ENDSESSION:
        if (wp) QuitApplication();
        return 0;
    case WM_DESTROY:
        if (g_control == hwnd) g_control = nullptr;
        QuitApplication();
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}
static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_INPUT:
        HandleRawInput(lp);
        // Required foreground Raw Input cleanup; does not suppress target input.
        return DefWindowProcW(hwnd, message, wp, lp);
    case WM_TIMER:
        if (wp == kExpireTimer) {
            KillTimer(hwnd, kExpireTimer);
            if (g_state.Visible(GetTickCount64())) ArmExpiry();
            else if (g_visible) RenderNow();
        }
        return 0;
    case WM_DISPLAYCHANGE:
        if (!CreateSurface()) ShowWindow(hwnd, SW_HIDE);
        else RenderNow();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; BeginPaint(hwnd, &ps); EndPaint(hwnd, &ps); return 0;
    }
    case WM_CLOSE:
        QuitApplication();
        return 0;
    case WM_DESTROY:
        if (g_overlay == hwnd) g_overlay = nullptr;
        QuitApplication();
        return 0;
    default: return DefWindowProcW(hwnd, message, wp, lp);
    }
}
static bool RegisterClassFor(const wchar_t* name, WNDPROC procedure, HBRUSH background) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc); wc.hInstance = g_instance; wc.lpfnWndProc = procedure;
    wc.lpszClassName = name; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = background;
    return RegisterClassExW(&wc) != 0;
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    g_instance = instance;
    SetProcessDPIAware();
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\KeyDisplay_SingleInstance");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Gracefully replace only our own named overlay, not unrelated processes.
        HWND old = FindWindowW(L"KeyDisplayOverlay", nullptr);
        if (old) {
            PostMessageW(old, WM_CLOSE, 0, 0);
            for (int i = 0; i < 40 && IsWindow(old); ++i) Sleep(50);
            if (IsWindow(old)) { CloseHandle(mutex); return 1; }
        } else { CloseHandle(mutex); return 0; }
    }
    GdiplusStartupInput startup;
    if (GdiplusStartup(&g_gdiToken, &startup, nullptr) != Ok) { CloseHandle(mutex); return 1; }
    g_uiFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           DEFAULT_PITCH, L"Microsoft YaHei UI");
    LoadConfig();
    if (!RegisterClassFor(L"KeyDisplayOverlay", OverlayProc, nullptr) ||
        !RegisterClassFor(L"KeyDisplayControl", ControlProc,
                          reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1)) ||
        !RegisterClassFor(L"KeyDisplaySettings", SettingsProc,
                          reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1))) return 1;
    g_control = CreateWindowExW(WS_EX_APPWINDOW, L"KeyDisplayControl", kVersion,
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 366, 196,
                                nullptr, nullptr, instance, nullptr);
    g_overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                L"KeyDisplayOverlay", kVersion, WS_POPUP,
                                0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                                nullptr, nullptr, instance, nullptr);
    if (!g_control || !g_overlay || !CreateSurface()) {
        MessageBoxW(nullptr, L"创建显示层失败，程序将退出。", kVersion, MB_OK | MB_ICONERROR);
        return 1;
    }
    RAWINPUTDEVICE devices[2]{};
    devices[0] = {0x01, 0x06, RIDEV_INPUTSINK, g_overlay};
    devices[1] = {0x01, 0x02, RIDEV_INPUTSINK, g_overlay};
    // No RIDEV_NOLEGACY: preserve all application keyboard and mouse messages.
    if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE))) {
        MessageBoxW(nullptr, L"Raw Input 注册失败，程序将退出。", kVersion, MB_OK | MB_ICONERROR);
        DestroyWindow(g_overlay); FreeSurface(); return 1;
    }
    g_rawRegistered = true;
    g_tray.cbSize = sizeof(g_tray); g_tray.hWnd = g_control; g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = kTrayMessage;
    g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(g_tray.szTip, L"KeyDisplay v1.3.1 — 右键：设置 / 退出", 128);
    // Tray is optional. The visible taskbar window always provides an exit.
    g_trayAdded = Shell_NotifyIconW(NIM_ADD, &g_tray) != FALSE;
    RenderNow();  // Apply transparent pixels BEFORE showing the layered window.
    ShowWindow(g_control, SW_SHOWNOACTIVATE);
    UpdateWindow(g_control);
    MSG message{};
    int result = 0;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (g_settings && IsDialogMessageW(g_settings, &message)) continue;
        if (g_control && (message.hwnd == g_control || IsChild(g_control, message.hwnd)) &&
            IsDialogMessageW(g_control, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    QuitApplication();
    FreeSurface();
    if (g_uiFont) DeleteObject(g_uiFont);
    GdiplusShutdown(g_gdiToken); CloseHandle(mutex);
    return result == -1 ? 1 : 0;
}
