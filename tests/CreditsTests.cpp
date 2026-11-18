#include "../fkSettings/Credits.cpp"
#include <richedit.h>
#include <array>
#include <cstdio>
#include <stdexcept>

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static const char* textClass = "RICHEDIT";
static bool applyFix = true;
static HWND creditsText = nullptr, otherText = nullptr;

static void __fastcall NativeInitialize(void* object, void*)
{
    const HWND dialog = *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 0x1c);
    creditsText = CreateWindowExA(0, textClass, "Credits\r\nSecond line", WS_CHILD | WS_VISIBLE |
        WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0, 0, 240, 80, dialog,
        reinterpret_cast<HMENU>(1000), GetModuleHandleW(nullptr), nullptr);
    otherText = CreateWindowExA(0, "EDIT", "Other field", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 90, 240, 25, dialog, reinterpret_cast<HMENU>(1001), GetModuleHandleW(nullptr), nullptr);
    // Native loading may leave a selection. The fix must clear that too.
    SendMessageA(creditsText, EM_SETSEL, 0, -1);
}

static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM, LPARAM)
{
    if (message != WM_INITDIALOG) return FALSE;
    std::array<BYTE, 0x20> object{};
    memcpy(object.data() + 0x1c, &dialog, sizeof(dialog));
    Credits::originalInitialize = reinterpret_cast<Credits::NativeInitialize>(NativeInitialize);
    if (applyFix) Credits::Initialize(object.data(), nullptr);
    else NativeInitialize(object.data(), nullptr);
    return TRUE;
}

static void CheckSelection(HWND text, DWORD expectedStart, DWORD expectedEnd, const char* message)
{
    DWORD start = 0, end = 0;
    SendMessageA(text, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    if (start != expectedStart || end != expectedEnd)
        fprintf(stderr, "%s fixed=%d: selection %lu,%lu expected %lu,%lu\n",
            textClass, applyFix, start, end, expectedStart, expectedEnd);
    Check(start == expectedStart && end == expectedEnd, message);
}

static void DialogTests()
{
    // Empty dialog template: controls are created by the native-init fixture.
    struct { DLGTEMPLATE header; WORD menu, cls, title; } layout{};
    layout.header.style = WS_POPUP | WS_CAPTION;
    layout.header.cx = 260; layout.header.cy = 140;
    for (int fixed = 0; fixed < 2; ++fixed)
    {
        applyFix = fixed != 0;
        HWND dialog = CreateDialogIndirectParamW(GetModuleHandleW(nullptr), &layout.header,
            nullptr, DialogProc, 0);
        Check(dialog && creditsText && otherText, "create credits dialog and controls");
        const int textLength = GetWindowTextLengthA(creditsText);
        Check(textLength > 0, "native credits text is loaded");
        if (applyFix) CheckSelection(creditsText, 0, 0, "initial dialog focus leaves credits unselected");
        // Rich Edit 1.0's selection offsets and WM_GETTEXTLENGTH can differ
        // around paragraph marks. Obtain the end in selection coordinates.
        DWORD length = 0;
        SendMessageA(creditsText, EM_SETSEL, 0, -1);
        SendMessageA(creditsText, EM_GETSEL, 0, reinterpret_cast<LPARAM>(&length));
        SendMessageA(creditsText, EM_SETSEL, 0, 0);
        // This is the real dialog manager's focus path, not a simulated
        // EM_SETSEL. An unmodified Rich Edit selects everything here.
        for (int visit = 0; visit < 3; ++visit)
        {
            SendMessageA(dialog, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(otherText), TRUE);
            CheckSelection(otherText, 0, GetWindowTextLengthA(otherText), "other edit retains automatic selection");
            SendMessageA(dialog, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(creditsText), TRUE);
            CheckSelection(creditsText, 0, applyFix ? 0 : length, "credits focus suppresses select-all only when fixed");
        }
        if (applyFix)
        {
            const LRESULT flags = SendMessageA(creditsText, WM_GETDLGCODE, 0, 0);
            Check(!(flags & DLGC_HASSETSEL) && (flags & DLGC_WANTARROWS), "preserve navigation flags");
            SendMessageA(creditsText, EM_SETSEL, 2, 7);
            CheckSelection(creditsText, 2, 7, "explicit manual selection remains available");
            SendMessageA(dialog, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(otherText), TRUE);
            SendMessageA(dialog, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(creditsText), TRUE);
            CheckSelection(creditsText, 2, 7, "focus preserves an intentional selection");
            SendMessageA(creditsText, EM_SETSEL, 0, -1);
            CheckSelection(creditsText, 0, length, "explicit select-all remains available");
            SendMessageA(creditsText, WM_CHAR, 'x', 0);
            Check(GetWindowTextLengthA(creditsText) == textLength, "credits remain read-only");
        }
        DestroyWindow(dialog);
    }
}

static void HookTests(const char* path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    Check(mapping != nullptr, "map frontend privately");
    BYTE* image = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(image != nullptr, "view frontend");
    std::array<BYTE, 16> original{};
    memcpy(original.data(), image + Credits::InitializeRva, original.size());
    Check(MH_Initialize() == MH_OK, "initialize MinHook");
    DWORD previous, ignored;
    Check(VirtualProtect(image + 0x7244, 5, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "make signature writable");
    const BYTE saved = image[0x7244]; image[0x7244] = 0x90;
    Check(!Credits::InstallInImage(image), "reject unsupported credits signature");
    Check(!memcmp(original.data(), image + Credits::InitializeRva, original.size()), "signature rejection leaves code unchanged");
    image[0x7244] = saved;
    VirtualProtect(image + 0x7244, 5, previous, &ignored);
    Check(Credits::InstallInImage(image), "install credits hook in supported frontend");
    Check(memcmp(original.data(), image + Credits::InitializeRva, original.size()) != 0, "credits initializer is detoured");
    Check(MH_Uninitialize() == MH_OK, "remove credits hook");
    Check(!memcmp(original.data(), image + Credits::InitializeRva, original.size()), "cleanup restores native code");
    UnmapViewOfFile(image); CloseHandle(mapping); CloseHandle(file);
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Usage: CreditsTests.exe frontend.exe");
        HMODULE legacy = LoadLibraryA("riched32.dll"), modern = LoadLibraryA("riched20.dll");
        Check(legacy && modern, "load legacy and modern Rich Edit");
        for (int reopen = 0; reopen < 2; ++reopen)
        {
            textClass = "RICHEDIT"; DialogTests();
            textClass = "RichEdit20A"; DialogTests();
        }
        HookTests(argv[1]);
        FreeLibrary(modern); FreeLibrary(legacy);
        puts("Credits tests passed.");
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "%s\n", error.what()); return 1; }
}
