// Included by both the modern and XP-toolset colour-map fixtures.
namespace DirectPlayCompatFixture
{
    namespace DP = DirectPlayCompat;
    HRESULT initializationResult = S_OK;
    unsigned initializationCalls = 0, deletionCalls = 0;
    DP::ProviderInitData* receivedInitialization = nullptr;
    DP::DeletePlayerData receivedDeletion{};
    HRESULT WINAPI Initialize(DP::ProviderInitData* data)
    { ++initializationCalls; receivedInitialization = data; return initializationResult; }
    HRESULT WINAPI Delete(DP::DeletePlayerData* data)
    { ++deletionCalls; receivedDeletion = *data; return S_FALSE; }
    FARPROC WINAPI Resolve(HMODULE, LPCSTR)
    { SetLastError(1234); return reinterpret_cast<FARPROC>(Initialize); }

    // The callback invocation at XP dplayx.dll + 0x13e21..0x13e2b,
    // including its internal object offset and unchecked indirect call.
    __declspec(naked) HRESULT __cdecl InvokeXpCallback(void*, DP::DeletePlayerData*)
    {
        __asm
        {
            push ebp
            mov ebp, esp
            push edi
            sub esp, 28h
            mov edx, [ebp+0ch]
            mov eax, [edx]
            mov [ebp-28h], eax
            mov eax, [edx+4]
            mov [ebp-24h], eax
            mov eax, [edx+8]
            mov [ebp-20h], eax
            mov edi, [ebp+8]
            mov eax, [edi+14h]
            lea ecx, [ebp-28h]
            push ecx
            call dword ptr [eax+28h]
            mov edi, [ebp-4]
            mov esp, ebp
            pop ebp
            ret
        }
    }
    HRESULT CatchNullCallback(void* object, DP::DeletePlayerData* data)
    {
        __try { return InvokeXpCallback(object, data); }
        __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        { return E_POINTER; }
    }
    void Require(bool value, const char* message)
    { if (!value) throw std::runtime_error(message); }
    void Run(const char* providerPath = nullptr)
    {
        DP::ProviderCallbacks callbacks{};
        callbacks.size = sizeof(callbacks); callbacks.version = 0x60000;
        callbacks.send = callbacks.createPlayer = reinterpret_cast<FARPROC>(Initialize);
        DP::ProviderInitData data{ &callbacks };
        void* object[6]{}; object[5] = &callbacks;
        DP::DeletePlayerData deletion{ 20, 8, &data };
        Require(CatchNullCallback(object, &deletion) == E_POINTER, "reproduce XP's unchecked null provider callback");
        const auto original = reinterpret_cast<FARPROC>(Initialize);
        DP::originalProviderInit = nullptr;
        Require(DP::AdaptProviderExport(original, false) == original && !DP::originalProviderInit,
            "other providers retain their original SPInit export");
        const auto adapted = DP::AdaptProviderExport(original, true);
        Require(adapted == reinterpret_cast<FARPROC>(DP::InitializeProvider) &&
            DP::AdaptProviderExport(adapted, true) == adapted && DP::originalProviderInit == reinterpret_cast<void*>(original),
            "IPX export adaptation is idempotent and preserves the real initializer");
        Require(DP::AdaptProviderExport(nullptr, true) == nullptr, "missing exports remain missing");
        const auto before = callbacks;
        initializationResult = S_FALSE;
        Require(DP::InitializeProvider(&data) == S_FALSE && receivedInitialization == &data && initializationCalls == 1,
            "real provider initialization receives the original data and preserves its HRESULT");
        Require(callbacks.deletePlayer && callbacks.send == before.send && callbacks.createPlayer == before.createPlayer &&
            callbacks.size == before.size && callbacks.version == before.version,
            "only the missing DeletePlayer callback is supplied");
        Require(CatchNullCallback(object, &deletion) == S_OK, "XP callback invocation succeeds with the correct stdcall ABI");
        callbacks.deletePlayer = Delete;
        Require(DP::InitializeProvider(&data) == S_FALSE && callbacks.deletePlayer == Delete &&
            InvokeXpCallback(object, &deletion) == S_FALSE && deletionCalls == 1 && receivedDeletion.player == 20 &&
            receivedDeletion.flags == 8 && receivedDeletion.directPlaySp == &data,
            "existing callbacks and all native callback arguments are preserved");
        callbacks.deletePlayer = nullptr; initializationResult = E_FAIL;
        Require(DP::InitializeProvider(&data) == E_FAIL && !callbacks.deletePlayer, "failed initialization is never patched");
        initializationResult = S_OK; callbacks.size = offsetof(DP::ProviderCallbacks, deletePlayer);
        Require(DP::InitializeProvider(&data) == S_OK && !callbacks.deletePlayer, "short provider tables are never extended");
        data.callbacks = nullptr;
        Require(DP::InitializeProvider(&data) == S_OK && DP::InitializeProvider(nullptr) == S_OK,
            "missing callback tables preserve the provider result");
        DP::originalProviderInit = nullptr;
        Require(DP::InitializeProvider(&data) == E_FAIL, "initializer absence returns a bounded failure");
        DP::originalResolveExport = Resolve;
        const HMODULE executable = GetModuleHandleW(nullptr);
        Require(DP::GetProviderExport(executable, "SPInit") == original && GetLastError() == 1234 &&
            DP::GetProviderExport(executable, MAKEINTRESOURCEA(1)) == original &&
            DP::GetProviderExport(executable, "OtherExport") == original,
            "unrelated modules, ordinals and names retain their exports and last error");
        const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        const FARPROC tick = GetProcAddress(kernel, "GetTickCount");
        Require(MH_Initialize() == MH_OK && DP::Install(), "install the actual GetProcAddress compatibility detour");
        Require(GetProcAddress(kernel, "GetTickCount") == tick && !GetProcAddress(kernel, "SPInit") && !DP::IsIpxProvider(executable),
            "installed hook leaves system exports and missing exports unchanged");
        if (providerPath)
        {
            // Inspect the real wrapper's export without running its initializer
            // or DllMain, opening sockets, or loading its dependencies.
            const HMODULE provider = LoadLibraryExA(providerPath, nullptr, DONT_RESOLVE_DLL_REFERENCES);
            Require(provider && DP::IsIpxProvider(provider), "load the named IPX provider for export-only inspection");
            const auto nativeInit = DP::originalResolveExport(provider, "SPInit");
            Require(nativeInit && GetProcAddress(provider, "SPInit") == reinterpret_cast<FARPROC>(DP::InitializeProvider) &&
                DP::originalProviderInit == reinterpret_cast<void*>(nativeInit),
                "actual IPX SPInit resolution passes through the installed compatibility hook");
            Require(GetProcAddress(provider, MAKEINTRESOURCEA(1)) == nativeInit,
                "provider ordinal queries remain unchanged");
            DP::originalProviderInit = nullptr;
            FreeLibrary(provider);
        }
        Require(MH_Uninitialize() == MH_OK && GetProcAddress(kernel, "GetTickCount") == tick, "restore the system resolver after testing");
        DP::originalResolveExport = nullptr; DP::originalProviderInit = nullptr;
        puts("PASS: XP null DeletePlayer reproduction, provider callback ABI, preserved initialization and scoped export-hook forwarding");
    }
}
