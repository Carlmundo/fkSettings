#include "../fkSettings/FrontendFocus.cpp"
#include <array>
#include <cstdio>
#include <stdexcept>

namespace FF = FrontendFocus;
static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static int sentMessages = 0, postedMessages = 0;
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_APP) { ++sentMessages; return 42; }
    if (message == WM_APP + 1) { ++postedMessages; return 0; }
    return DefWindowProcA(window, message, w, l);
}
struct Sender { HWND window; HANDLE exit; LRESULT reply = 0; DWORD error = 0; };
static DWORD WINAPI SendAndExit(void* context)
{
    auto& sender = *static_cast<Sender*>(context);
    DWORD_PTR reply = 0;
    if (!SendMessageTimeoutA(sender.window, WM_APP, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &reply))
        sender.error = GetLastError();
    sender.reply = static_cast<LRESULT>(reply);
    SetEvent(sender.exit);
    return 0;
}

static int minimized = 0, restored = 0, passedThrough = 0;
static void __fastcall NativeMinimize(void*, void*) { ++minimized; }
static void __fastcall NativeRestore(void*, void*)
{
    Check(!FF::launchWindow && !FF::launchThread, "clear launch state before native restoration");
    ++restored;
}
static DWORD WINAPI OtherWait(HANDLE, DWORD) { ++passedThrough; return 123; }
static DWORD WINAPI WaitOnWorker(void*)
{
    return FF::WaitForCaller(nullptr, INFINITE, FF::image + 0x2782e);
}

static void* nativeWaitSite = nullptr;
__declspec(naked) static DWORD __stdcall InvokeNativeWait(HANDLE, DWORD)
{
    __asm
    {
        pop eax
        pop ecx
        pop edx
        push eax
        push edx
        push ecx
        jmp dword ptr [nativeWaitSite]
    }
}

static void WaitTests()
{
    WNDCLASSA cls{}; cls.lpfnWndProc = WindowProc; cls.lpszClassName = "fkSettings.FocusTest";
    cls.hInstance = GetModuleHandleW(nullptr);
    Check(RegisterClassA(&cls) != 0, "register test window");
    HWND window = CreateWindowA(cls.lpszClassName, "Focus test", WS_OVERLAPPEDWINDOW,
        0, 0, 200, 100, nullptr, nullptr, cls.hInstance, nullptr);
    Check(window != nullptr, "create UI window");
    std::array<BYTE, 0x30> object{};
    memcpy(object.data() + 0x1c, &window, sizeof(window));
    FF::originalMinimize = reinterpret_cast<FF::NativeWindow>(NativeMinimize);
    FF::originalRestore = reinterpret_cast<FF::NativeWindow>(NativeRestore);
    FF::originalWait = OtherWait;
    // Real allocated storage makes caller arithmetic defined without loading
    // or executing the game for this message-wait regression.
    auto fixture = new BYTE[0xb0000]; FF::image = fixture;
    for (int round = 0; round < 2; ++round)
    {
        FF::Minimize(object.data(), nullptr);
        Check(FF::launchWindow == object.data() && minimized == round + 1, "capture game launch state");
        for (size_t site : FF::EngineWaitSites)
        {
            HANDLE exit = CreateEventW(nullptr, round == 0, FALSE, nullptr);
            Sender sender{window, exit};
            Check(PostMessageA(window, WM_APP + 1, 0, 0) != FALSE, "queue command before engine wait");
            Check(PostThreadMessageA(GetCurrentThreadId(), WM_QUIT, 17, 0) != FALSE, "queue quit message");
            MSG beforeQuit{};
            Check(PeekMessageA(&beforeQuit, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE) && beforeQuit.wParam == 17,
                "quit is queued before engine wait");
            HANDLE worker = CreateThread(nullptr, 0, SendAndExit, &sender, 0, nullptr);
            Check(worker != nullptr, "start simulated engine exit");
            Check(FF::WaitForCaller(exit, INFINITE, fixture + site + 6) == WAIT_OBJECT_0, "engine exit unblocks frontend");
            Check(WaitForSingleObject(worker, 3000) == WAIT_OBJECT_0, "sender finishes");
            Check(sender.reply == 42 && !sender.error, "service synchronous activation-style message before engine exits");
            Check(postedMessages == 0, "do not dispatch posted commands during native launch");
            MSG message{};
            Check(PeekMessageA(&message, window, WM_APP + 1, WM_APP + 1, PM_REMOVE) && message.message == WM_APP + 1,
                "posted commands remain for normal UI loop");
            const BOOL quit = PeekMessageA(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE);
            Check(quit && message.wParam == 17,
                "preserve WM_QUIT and its exit code");
            CloseHandle(worker); CloseHandle(exit);
        }
        Check(FF::WaitForCaller(nullptr, 0, fixture + 0xa73a4) == 123, "finite waits pass through");
        Check(FF::WaitForCaller(nullptr, INFINITE, fixture + 0xa73a3) == 123, "unrelated callers pass through");
        HANDLE otherThread = CreateThread(nullptr, 0, WaitOnWorker, nullptr, 0, nullptr);
        Check(otherThread && WaitForSingleObject(otherThread, 3000) == WAIT_OBJECT_0, "worker wait finishes");
        DWORD workerResult = 0;
        Check(GetExitCodeThread(otherThread, &workerResult) && workerResult == 123, "other threads pass through");
        CloseHandle(otherThread);
        FF::Restore(object.data(), nullptr);
        Check(restored == round + 1, "restore every completed round");
        Check(FF::WaitForCaller(nullptr, INFINITE, fixture + 0xa73a4) == 123, "waits outside game launch pass through");
    }
    Check(sentMessages == 12 && passedThrough == 8, "all engine wait paths and pass-through cases exercised");
    Check(FF::WaitForEngine(INVALID_HANDLE_VALUE) == WAIT_FAILED, "invalid handles retain failure result");
    delete[] fixture;
    DestroyWindow(window); UnregisterClassA(cls.lpszClassName, cls.hInstance);
}

static void HookTests(const char* path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    Check(mapping != nullptr, "map frontend privately");
    BYTE* image = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(image != nullptr, "view frontend");
    std::array<BYTE, 16> minimize{}, restore{};
    memcpy(minimize.data(), image + 0x25b80, 16); memcpy(restore.data(), image + 0x25ba6, 16);
    Check(MH_Initialize() == MH_OK, "initialize MinHook");
    DWORD previous;
    Check(VirtualProtect(image + 0xa739e, 6, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "make signature writable");
    const BYTE saved = image[0xa739e]; image[0xa739e] = 0x90;
    Check(!FF::InstallInImage(image), "reject unsupported engine wait signature");
    Check(!memcmp(image + 0x25b80, minimize.data(), 16), "rejection leaves native code intact");
    image[0xa739e] = saved; VirtualProtect(image + 0xa739e, 6, previous, &previous);
    Check(FF::InstallInImage(image), "install focus hooks against actual frontend");
    Check(image[0x25b80] == 0xe9 && image[0x25ba6] == 0xe9, "activate both lifecycle hooks");
    // Execute each actual native indirect CALL through the installed Windows
    // detour. This also verifies _ReturnAddress gating, not just the helper.
    auto import = reinterpret_cast<FF::NativeWait*>(image + 0x1bc004);
    Check(VirtualProtect(import, sizeof(*import), PAGE_READWRITE, &previous) != FALSE, "resolve fixture wait import");
    const auto savedImport = *import;
    *import = reinterpret_cast<FF::NativeWait>(GetProcAddress(GetModuleHandleW(L"kernel32"), "WaitForSingleObject"));
    DWORD ignored;
    VirtualProtect(import, sizeof(*import), previous, &ignored);
    WNDCLASSA cls{}; cls.lpfnWndProc = WindowProc; cls.lpszClassName = "fkSettings.NativeFocusTest";
    cls.hInstance = GetModuleHandleW(nullptr);
    Check(RegisterClassA(&cls) != 0, "register native wait test window");
    HWND window = CreateWindowA(cls.lpszClassName, "Native wait", 0, 0, 0, 100, 100,
        nullptr, nullptr, cls.hInstance, nullptr);
    Check(window != nullptr, "create native wait test window");
    FF::launchWindow = &window;
    InterlockedExchange(&FF::launchThread, static_cast<LONG>(GetCurrentThreadId()));
    for (size_t site : FF::EngineWaitSites)
    {
        BYTE* call = image + site;
        Check(VirtualProtect(call, 7, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "prepare native wait continuation");
        DWORD savedOperand;
        memcpy(&savedOperand, call + 2, 4);
        const DWORD operand = reinterpret_cast<DWORD>(import);
        memcpy(call + 2, &operand, 4);
        const BYTE continuation = call[6]; call[6] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), call, 7);
        HANDLE exit = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Sender sender{window, exit};
        HANDLE worker = CreateThread(nullptr, 0, SendAndExit, &sender, 0, nullptr);
        nativeWaitSite = call;
        Check(InvokeNativeWait(exit, INFINITE) == WAIT_OBJECT_0, "actual native call returns after engine exit");
        Check(WaitForSingleObject(worker, 3000) == WAIT_OBJECT_0 && sender.reply == 42 && !sender.error,
            "actual native caller services synchronous message through installed detour");
        memcpy(call + 2, &savedOperand, 4); call[6] = continuation;
        FlushInstructionCache(GetCurrentProcess(), call, 7);
        VirtualProtect(call, 7, previous, &ignored);
        CloseHandle(worker); CloseHandle(exit);
    }
    FF::launchWindow = nullptr; InterlockedExchange(&FF::launchThread, 0);
    DestroyWindow(window); UnregisterClassA(cls.lpszClassName, cls.hInstance);
    VirtualProtect(import, sizeof(*import), PAGE_READWRITE, &previous);
    *import = savedImport;
    VirtualProtect(import, sizeof(*import), previous, &ignored);
    Check(MH_Uninitialize() == MH_OK, "remove hooks");
    Check(!memcmp(image + 0x25b80, minimize.data(), 16) && !memcmp(image + 0x25ba6, restore.data(), 16),
        "restore original frontend code");
    UnmapViewOfFile(image); CloseHandle(mapping); CloseHandle(file);
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "supply frontend.exe path");
        WaitTests(); HookTests(argv[1]);
        puts("PASS: synchronous UI messages during all six engine waits, queued command/quit preservation, repeated restoration, signature rejection and hook cleanup");
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
