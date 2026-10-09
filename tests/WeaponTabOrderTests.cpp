#include "../fkSettings/WeaponTabOrder.h"
#include <commctrl.h>
#include <cstdio>
#include <stdexcept>

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM) { return FALSE; }

static bool IsTracked(HWND window)
{
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    return style == 1342242821 || style == 1342242819;
}

static LRESULT CALLBACK CountPositions(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
    UINT_PTR, DWORD_PTR reference)
{
    if (message == WM_WINDOWPOSCHANGING) ++*reinterpret_cast<unsigned*>(reference);
    return DefSubclassProc(window, message, wparam, lparam);
}

static unsigned WatchInputs(HWND page, unsigned& count)
{
    unsigned inputs = 0;
    for (int id = 5001; id <= 5171; id += 5)
    {
        HWND input = GetDlgItem(page, id);
        if (!input || !IsTracked(input)) continue;
        Check(SetWindowSubclass(input, CountPositions, 1, reinterpret_cast<DWORD_PTR>(&count)) != FALSE,
            "watch native input positioning");
        ++inputs;
    }
    return inputs;
}

// Replay the native constructor's ascending five-ID rows. Each input is moved
// to HWND_TOP after the old repair; slider readouts are positioned afterwards.
template<typename Add>
static void NativeLayout(HWND page, Add add)
{
    const auto place = [](HWND window) {
        if (window) Check(SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) != FALSE, "native layout positioning");
    };
    for (int id = 5000; id <= 5170; id += 5)
    {
        HWND label = GetDlgItem(page, id);
        if (!label) continue;
        HWND input = GetDlgItem(page, id + 1);
        place(label);
        if (input && IsTracked(input)) add(input);
        place(input);
        if (input && (GetWindowLongPtrW(input, GWL_STYLE) & BS_TYPEMASK) == 5)
            place(GetDlgItem(page, id + 2));
    }
}

static std::vector<int> TabSequence(HWND page, bool reverse)
{
    std::vector<int> result;
    HWND first = GetNextDlgTabItem(page, nullptr, FALSE);
    if (!first) return result;
    HWND current = first;
    do
    {
        result.push_back(GetDlgCtrlID(current));
        Check(result.size() <= 100, "tab traversal terminates");
        current = GetNextDlgTabItem(page, current, reverse);
        Check(current != nullptr, "tab traversal stays in native page");
    } while (current != first);
    return result;
}

static std::vector<int> SiblingSequence(HWND page)
{
    std::vector<int> result;
    for (HWND child = GetWindow(page, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        result.push_back(GetDlgCtrlID(child));
    return result;
}

static void PageTests(HMODULE resources, HWND parent, int id)
{
    HWND legacy = CreateDialogParamW(resources, MAKEINTRESOURCEW(id), parent, DialogProc, 0);
    HWND batched = CreateDialogParamW(resources, MAKEINTRESOURCEW(id), parent, DialogProc, 0);
    HWND fallback = CreateDialogParamW(resources, MAKEINTRESOURCEW(id), parent, DialogProc, 0);
    Check(legacy && batched && fallback, "create actual weapon-page resources");
    Check(GetWindowTextLengthW(legacy) == 0, "native weapon page matches dialog-reset detection");
    ShowWindow(legacy, SW_SHOWNOACTIVATE);
    ShowWindow(batched, SW_SHOWNOACTIVATE);
    ShowWindow(fallback, SW_SHOWNOACTIVATE);
    unsigned legacyWrites = 0, batchWrites = 0, fallbackWrites = 0;
    const unsigned inputs = WatchInputs(legacy, legacyWrites);
    Check(inputs != 0 && WatchInputs(batched, batchWrites) == inputs && WatchInputs(fallback, fallbackWrites) == inputs,
        "native pages have matching tracked inputs");

    // Independent copy of the original implementation provides the navigation baseline.
    std::vector<HWND> original;
    NativeLayout(legacy, [&](HWND input) {
        original.push_back(input);
        for (size_t i = 0; i < original.size(); ++i)
            Check(SetWindowPos(original[i], i == 0 ? HWND_TOP : original[i - 1], 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) != FALSE, "legacy tab repair");
    });
    Check(legacyWrites == inputs + inputs * (inputs + 1) / 2, "legacy repair performs quadratic input positioning");

    TabOrder::Batch batch;
    batch.Enable(true);
    for (unsigned repeat = 0; repeat < 2; ++repeat)
    {
        batch.Reset(batched);
        batchWrites = 0;
        HWND focus = GetNextDlgTabItem(batched, nullptr, FALSE);
        SetFocus(focus);
        const HWND focused = GetFocus();
        RECT bounds{}; GetWindowRect(focus, &bounds);
        NativeLayout(batched, [&](HWND input) { batch.Add(input); });
        Check(batchWrites == inputs, "collection adds no positioning during native layout");
        batch.Add(GetDlgItem(legacy, 5001));
        batch.Apply(legacy);
        Check(batchWrites == inputs, "unrelated pages cannot flush or enter a pending batch");
        batch.Apply(batched);
        Check(batchWrites == 2 * inputs, "one final repair positions each input once");
        batch.Apply(batched);
        Check(batchWrites == 2 * inputs, "completion drains the batch exactly once");
        Check(GetFocus() == focused, "batching preserves keyboard focus");
        RECT after{}; GetWindowRect(focus, &after);
        Check(EqualRect(&bounds, &after) != FALSE, "batching preserves input bounds");
        Check(SiblingSequence(batched) == SiblingSequence(legacy), "batching preserves full sibling order including labels");
        for (bool reverse : { false, true })
        {
            const auto expected = TabSequence(legacy, reverse);
            const auto actual = TabSequence(batched, reverse);
            if (expected.empty() || actual != expected)
            {
                fprintf(stderr, "Page %d, reverse %d: expected", id, reverse);
                for (int control : expected) fprintf(stderr, " %d", control);
                fprintf(stderr, "; actual");
                for (int control : actual) fprintf(stderr, " %d", control);
                fprintf(stderr, "\n");
            }
            Check(!expected.empty() && actual == expected,
                "Tab and Shift+Tab preserve the legacy entry point and full traversal");
        }
    }

    TabOrder::Batch unavailable;
    unavailable.Reset(fallback);
    NativeLayout(fallback, [&](HWND input) { unavailable.Add(input); });
    unavailable.Apply(fallback);
    Check(fallbackWrites == legacyWrites && TabSequence(fallback, false) == TabSequence(legacy, false) &&
        TabSequence(fallback, true) == TabSequence(legacy, true), "unavailable completion hook retains legacy repair");

    batch.Reset(batched);
    batch.Add(GetDlgItem(batched, 5001));
    DestroyWindow(batched);
    batch.Apply(batched); // A destroyed/reparented input must never be positioned.
    DestroyWindow(legacy);
    DestroyWindow(fallback);
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "supply frontend.exe resource path");
        INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_BAR_CLASSES };
        Check(InitCommonControlsEx(&controls) != FALSE, "initialize trackbars");
        HMODULE resources = LoadLibraryExA(argv[1], nullptr, LOAD_LIBRARY_AS_DATAFILE);
        Check(resources != nullptr, "load frontend resources without running it");
        HWND parent = CreateWindowW(L"STATIC", L"Weapon tab order fixture", WS_OVERLAPPEDWINDOW,
            0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr);
        Check(parent != nullptr, "create fixture parent");
        ShowWindow(parent, SW_SHOWNOACTIVATE);
        for (int id = 4900; id <= 4937; ++id) PageTests(resources, parent, id);
        DestroyWindow(parent);
        FreeLibrary(resources);
        puts("PASS: all 38 native weapon pages retain forward/reverse tab order, focus and bounds; linear positioning, repeated layout, cleanup and legacy fallback");
        return 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
