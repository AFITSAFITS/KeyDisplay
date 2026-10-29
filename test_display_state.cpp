// SPDX-License-Identifier: MIT
#include "display_state.h"
#include <cstdio>
#include <cstdlib>
static int checks = 0;
static void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
    using namespace keydisplay;
    std::uint32_t value = 77;
    Check(ParseDuration(L"1", value) && value == 1, "minimum duration");
    Check(ParseDuration(L"600000", value) && value == 600000, "maximum duration");
    Check(ParseDuration(L"1200", value) && value == 1200, "default duration");
    Check(!ParseDuration(L"0", value), "zero invalid");
    Check(!ParseDuration(L"600001", value), "overflow limit invalid");
    Check(!ParseDuration(L"999999999999", value), "long overflow invalid");
    Check(!ParseDuration(L"", value), "empty invalid");
    Check(!ParseDuration(L"-1", value), "negative invalid");
    Check(!ParseDuration(L"10.5", value), "fraction invalid");
    Check(!ParseDuration(L" 10", value), "space invalid");
    Check(!ParseDuration(L"1ms", value), "units invalid in input");
    DisplayState state;
    Check(!state.Visible(0), "empty panel hidden");
    state.KeyDown(83, L"S", false, 1000);
    Check(state.Visible(1000) && state.text == L"S", "new key immediate");
    state.KeyUp(83);
    Check(state.Visible(1001) && state.text == L"S" && state.pressed.empty(),
          "quick release retains event");
    Check(state.Visible(2199) && state.Remaining(2199) == 1, "one ms before expiry");
    Check(!state.Visible(2200) && state.Remaining(2200) == 0, "exact expiry");
    state.durationMs = 500000;
    state.KeyDown(83, L"S", false, 3000);
    state.KeyUp(83);
    state.KeyDown(65, L"A", false, 3001);
    Check(state.text == L"A" && state.lastEventMs == 3001, "long duration never delays next input");
    state.KeyUp(65);
    state.KeyDown(162, L"Ctrl", true, 4000);
    state.KeyDown(83, L"S", false, 4001);
    Check(state.text == L"Ctrl + S", "modifier combo");
    state.KeyUp(83); state.KeyUp(162);
    Check(state.text == L"Ctrl + S" && state.pressed.empty(), "combo stays after release");
    state.KeyDown(164, L"Alt", true, 5000);
    state.KeyUp(164);
    state.KeyDown(66, L"B", false, 5001);
    Check(state.text == L"B", "Alt key release leaves no stuck modifier");
    state.KeyDown(66, L"B", false, 5002);
    Check(state.pressed.size() == 1 && state.lastEventMs == 5002, "repeat refresh without duplicate");
    state.KeyUp(66);
    state.Mouse(L"LMB", 6000);
    Check(state.text == L"LMB" && state.lastEventMs == 6000, "mouse immediate");
    state.Mouse(L"LMB", 6001);
    Check(state.lastEventMs == 6001, "rapid same button click not filtered");
    state.Mouse(L"RMB", 6002);
    Check(state.text == L"RMB", "new click replaces old click");
    state.KeyDown(162, L"Ctrl", true, 6003);
    state.Mouse(L"LMB", 6004);
    Check(state.text == L"Ctrl + LMB", "modifier mouse combo");
    state.KeyUp(162);
    state.durationMs = 1;
    state.Mouse(L"MMB", 7000);
    Check(state.Visible(7000) && !state.Visible(7001), "one-ms timing model");
    state.durationMs = 600000;
    Check(state.Visible(7001) && state.Remaining(7001) == 599999, "apply duration immediately");
    state.Clear();
    Check(state.text.empty() && state.pressed.empty() && !state.Visible(7001), "clear state");
    // Monotonic 64-bit uptime: works beyond the legacy GetTickCount wrap.
    const std::uint64_t longUptime = (1ULL << 32) + 100;
    state.KeyDown(83, L"S", false, longUptime);
    Check(state.Remaining(longUptime + 100) == 599900, "64-bit uptime");
    std::printf("PASS: %d assertions\n", checks);
    return 0;
}
