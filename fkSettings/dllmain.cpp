#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x05010300
#endif

typedef struct IUnknown IUnknown;

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include "winerror.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <array>
#include <string>
#include <vector>
#include <cstdio>

#ifdef _X86_
extern "C" { int _afxForceUSRDLL; }
#else
extern "C" { int __afxForceUSRDLL; }
#endif

#include "include/MinHook.h"
#include "Hooks.h"
#include "SecretWeapons.h"
#include "NetworkTeams.h"

#include <sstream>
#include <fstream>
#include "CDButton.h"

#pragma comment(lib,"user32.lib") 
#pragma comment(lib,"libs\\libMinHook.x86.lib")

#define BUTTON_IDENTIFIER 1

std::string lang;
CButton advancedOptionsBtn;
LPCTSTR advancedOptionsLabel;

CWnd* tcpAddressDropdown;

bool leftAlign = false;

bool IPXEnabled = false;

bool createAdvancedOptions = false;
bool overrideAddressBook = false;
bool reposExitButton = true;
bool reposHintText = true;

// Windows XP compatible file/path helpers: FileExists, GetFrontendDirectory, BuildSpeechPath
bool FileExists(const char* path)
{
    if (!path || !*path)
        return false;
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
bool GetFrontendDirectory(char* outPath, size_t outPathSize)
{
    if (!outPath || outPathSize == 0)
        return false;
    outPath[0] = '\0';

    DWORD length = GetModuleFileNameA(NULL, outPath, static_cast<DWORD>(outPathSize));
    if (length == 0 || length >= outPathSize)
    {
        outPath[0] = '\0';
        return false;
    }

    char* lastSlash = strrchr(outPath, '\\');
    if (!lastSlash)
        lastSlash = strrchr(outPath, '/');
    if (!lastSlash)
    {
        outPath[0] = '.';
        outPath[1] = '\0';
        return true;
    }
    *lastSlash = '\0';
    return true;
}
bool BuildSpeechPath(
    char* outPath,
    size_t outPathSize,
    const char* gameDirectory,
    const char* speechBank,
    const char* defaultSpeechBank,
    const char* wavFile)
{
    if (!outPath ||
        outPathSize == 0 ||
        !gameDirectory ||
        !speechBank ||
        !defaultSpeechBank ||
        !wavFile)
    {
        return false;
    }

    int result = -1;

    if (_stricmp(speechBank, defaultSpeechBank) == 0)
    {
        result = sprintf_s(
            outPath,
            outPathSize,
            "%s\\Data\\Wav\\Speech\\%s",
            gameDirectory,
            wavFile
        );
    }
    else
    {
        result = sprintf_s(
            outPath,
            outPathSize,
            "%s\\Data\\Wav\\Speech\\%s\\%s",
            gameDirectory,
            speechBank,
            wavFile
        );
    }

    return result >= 0 && static_cast<size_t>(result) < outPathSize;
}

double GetDpiScaleFactor(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi / 96.0; // 96 is the default DPI
}

void PatchCall(void* callAddr, void* newFunc) {
    if (!callAddr || !newFunc)
        return;

    BYTE* p = static_cast<BYTE*>(callAddr);

    if (*p != 0xE8) return;

    DWORD oldProtect = 0;

    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        return;
    }

    DWORD src = reinterpret_cast<DWORD>(p) + 5;
    DWORD dst = reinterpret_cast<DWORD>(newFunc);
    *reinterpret_cast<DWORD*>(p + 1) = dst - src;
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    DWORD ignored;

    VirtualProtect(p, 5, oldProtect, &ignored);
}

namespace TabOrder
{
    static std::vector<HWND> controls;

    void Reset(){
        controls.clear();
    }

    void Add(HWND hwnd){
        if (hwnd) {
            controls.push_back(hwnd);
            for (size_t i = 0; i < controls.size(); ++i) {
                HWND insertAfter = (i == 0) ? HWND_TOP : controls[i - 1];
                SetWindowPos(controls[i], insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
    }
}

int __fastcall WeaponsSetWindowPos_Label(int hWnd, void* lol, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    HWND hwnd = *(HWND*)(hWnd + 28);
    double scale = GetDpiScaleFactor(hwnd);

    return SetWindowPos(hwnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

int __fastcall WeaponsSetWindowPos_Input(int hWnd, void* lol, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    HWND hwnd = *(HWND*)(hWnd + 28);
    DWORD style = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
    double scale = GetDpiScaleFactor(hwnd);

    if (style == 1342242821) { //Trackbar with tabstop
        TabOrder::Add(hwnd);
    }

    if(scale > 1)
        Y = (int)round(Y + (13 * (scale - 1))); //13 = Height of text

    return SetWindowPos(hwnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

int __fastcall WeaponsSetWindowPos_Button(int hWnd, void* lol, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    HWND hwnd = *(HWND*)(hWnd + 28);
    DWORD style = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
    double scale = GetDpiScaleFactor(hwnd);

    if (style == 1342242819) { //Checkbox with tabstop
        TabOrder::Add(hwnd);
    }
    else {
        if (scale > 1)
            X = (int)round(X + (18 * (scale - 1)));
    }
    return SetWindowPos(hwnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}


LPCSTR lastFoundResourceName = nullptr;

typedef HRSRC(WINAPI* FindResourceAType)(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
FindResourceAType pFindResourceA = nullptr; //original function pointer after hook
FindResourceAType pFindResourceATarget; //original function pointer BEFORE hook do not call this!
HRSRC WINAPI detourFindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType)
{
    auto returnVal = pFindResourceA(hModule, lpName, lpType);

    if ((int)lpType == (int)MAKEINTRESOURCE(5))
        lastFoundResourceName = lpName;

    return returnVal;
}

//Run the settings app if it exists
void HandleButtonClick(HWND hWnd)
{
    bool exists = FileExists("settings.exe");

    if (exists)
    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        CreateProcessA("settings.exe", NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

//The pointer to the original window message processing function
WNDPROC ogVideoOptWndProc = nullptr;

WNDPROC ogNetworkPlayWndProc = nullptr;
WNDPROC ogAddressBookWndProc = nullptr;
WNDPROC ogIPXBtnWndProc = nullptr;
WNDPROC ogTCPBtnWndProc = nullptr;

CWnd* hint;

//Soundbank Play button related
#define SPEECH_PLAY_BUTTON_ID 50001
HWND speechComboBoxHwnd = nullptr;
WNDPROC ogTeamEditorWndProc = nullptr;
DWORD speechRandomState = 0;
DWORD NextSpeechRandom()
{
    if (speechRandomState == 0)
    {
        LARGE_INTEGER counter = {};
        DWORD seed =
            GetTickCount() ^
            GetCurrentProcessId() ^
            GetCurrentThreadId() ^
            static_cast<DWORD>(GetMessageTime());

        if (QueryPerformanceCounter(&counter))
        {
            seed ^= counter.LowPart;
            seed ^= counter.HighPart;
        }
        seed ^= static_cast<DWORD>(
            reinterpret_cast<ULONG_PTR>(speechComboBoxHwnd)
            );
        // Xorshift cannot use zero as its state.
        if (seed == 0)
            seed = 0xA341316C;
        speechRandomState = seed;
    }

    // xorshift32
    DWORD x = speechRandomState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    speechRandomState = x;

    return x;
}
void PlaySpeechPreview() {
    if (!speechComboBoxHwnd || !::IsWindow(speechComboBoxHwnd)) {
        return;
    }
    int selectedIndex = (int)SendMessageA(speechComboBoxHwnd, CB_GETCURSEL, 0, 0);
    if (selectedIndex == CB_ERR) {
        return;
    }
    int textLength = (int)SendMessageA(speechComboBoxHwnd, CB_GETLBTEXTLEN, selectedIndex, 0);
    if (textLength == CB_ERR) {
        return;
    }
    std::string comboValue(textLength + 1, '\0');
    SendMessageA(speechComboBoxHwnd, CB_GETLBTEXT, selectedIndex, reinterpret_cast<LPARAM>(&comboValue[0]));
    comboValue.resize(textLength);

    char gameDirectory[MAX_PATH] = {};
    if (!GetFrontendDirectory(gameDirectory, sizeof(gameDirectory)))
    {
        return;
    }

    // Get StringTable resource ID 99 - "Default", in English
    char strDefault[256] = {};
    LoadStringA(GetModuleHandleA(NULL), 99, strDefault, sizeof(strDefault));

    static const std::array<const char*, 49> speechFiles = {
        "amazing.wav",
        "boring.wav",
        "brilliant.wav",
        "bummer.wav",
        "bungee.wav",
        "byebye.wav",
        "collect.wav",
        "comeonthen.wav",
        "coward.wav",
        "dragonpunch.wav",
        "drop.wav",
        "excellent.wav",
        "fatality.wav",
        "fire.wav",
        "fireball.wav",
        "firstblood.wav",
        "flawless.wav",
        "grenade.wav",
        "hello.wav",
        "hurry.wav",
        "illgetyou.wav",
        "incoming.wav",
        "jump1.wav",
        "jump2.wav",
        "justyouwait.wav",
        "kamikaze.wav",
        "laugh.wav",
        "missed.wav",
        "nooo.wav",
        "OHDEAR.WAV",
        "oinutter.wav",
        "ooff1.wav",
        "ooff2.wav",
        "ooff3.wav",
        "oops.wav",
        "orders.wav",
        "ow1.wav",
        "ow2.wav",
        "ow3.wav",
        "perfect.wav",
        "revenge.wav",
        "runaway.wav",
        "stupid.wav",
        "takecover.wav",
        "traitor.wav",
        "victory.wav",
        "watchthis.wav",
        "whatthe.wav",
        "youllregretthat.wav"
    };

    const size_t randomIndex = static_cast<size_t>(NextSpeechRandom() % speechFiles.size());

    const char* randomFile = speechFiles[randomIndex];
    char wavPath[MAX_PATH] = {};

    if (!BuildSpeechPath(
        wavPath,
        sizeof(wavPath),
        gameDirectory,
        comboValue.c_str(),
        strDefault,
        randomFile))
    {
        return;
    }

    if (!FileExists(wavPath))
        return;

    PlaySoundA(wavPath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}
LRESULT CALLBACK TeamEditorWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND) {
        const int controlId = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        if (controlId == SPEECH_PLAY_BUTTON_ID && notification == BN_CLICKED) {
            PlaySpeechPreview();
            return 0;
        }
    }
    LRESULT result = CallWindowProc(ogTeamEditorWndProc, hWnd, message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        speechComboBoxHwnd = nullptr;
        ogTeamEditorWndProc = nullptr;
    }
    return result;
}

//Process the TCP button's incoming messages
LRESULT CALLBACK TCPBtnWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);

    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        LRESULT result = CallWindowProc(ogTCPBtnWndProc, hWnd, message, wParam, lParam);
        IPXEnabled = false;

        if (tcpAddressDropdown != NULL)
            tcpAddressDropdown->ShowWindow(true);
        return result;
    }
    break;
    default:
        return CallWindowProc(ogTCPBtnWndProc, hWnd, message, wParam, lParam);
    }
    return 0;
}

//Process the IPX button's incoming messages
LRESULT CALLBACK IPXBtnWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);

    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        LRESULT result = CallWindowProc(ogIPXBtnWndProc, hWnd, message, wParam, lParam);
        IPXEnabled = true;

        if (tcpAddressDropdown != NULL)
            tcpAddressDropdown->ShowWindow(false);

        return result;
    }
    break;
    default:
        return CallWindowProc(ogIPXBtnWndProc, hWnd, message, wParam, lParam);
    }
    return 0;
}

//Process the Address book button's incoming messages
LRESULT CALLBACK AddressBookBtnWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);

    switch (message)
    {
    case WM_ENABLE:
    {
        //Prevent the frontend from disabling the address book button
        LRESULT result = CallWindowProc(ogAddressBookWndProc, hWnd, message, wParam, lParam);

        if(wmId == 0)
            EnableWindow(hWnd, true);

        return result;
    }
    break;
    default:
        return CallWindowProc(ogAddressBookWndProc, hWnd, message, wParam, lParam);
    }
    return 0;
}

//Process the Network Play tab's incoming messages
LRESULT CALLBACK NetworkPlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);

    switch (message)
    {
    case WM_COMMAND:
    {
        if (wmEvent == BN_CLICKED)
        {
            // Handle button click
            HWND hButtonClicked = (HWND)lParam;

            auto data = GetWindowLongPtr(hButtonClicked, GWLP_USERDATA);

            //Address Book btn
            if (wmId == 1243) {
                if (IPXEnabled) 
                {
                    //Run the custom IPX address book
                    bool exists = FileExists("ipxaddress.exe");

                    if (exists)
                    {
                        STARTUPINFOA si;
                        PROCESS_INFORMATION pi;

                        ZeroMemory(&si, sizeof(si));
                        si.cb = sizeof(si);
                        ZeroMemory(&pi, sizeof(pi));

                        CreateProcessA("ipxaddress.exe", NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);

                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                }
                else
                    return CallWindowProc(ogNetworkPlayWndProc, hWnd, message, wParam, lParam);
            }
            else //Something else has been clicked, let the frontend handle it
                return CallWindowProc(ogNetworkPlayWndProc, hWnd, message, wParam, lParam);
        }
        else {
            return CallWindowProc(ogNetworkPlayWndProc, hWnd, message, wParam, lParam);
        }
    }
    break;
    default:
        return CallWindowProc(ogNetworkPlayWndProc, hWnd, message, wParam, lParam);
    }
    return 0;
}

//Process the Video options tab's incoming messages
LRESULT CALLBACK VideoOptWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        if (wmEvent == BN_CLICKED)
        {
            // Handle button click
            HWND hButtonClicked = (HWND)lParam;

            auto data = GetWindowLongPtr(hButtonClicked, GWLP_USERDATA);

            //Advanced Options button
            if (data == 1)
                HandleButtonClick(hButtonClicked);
            else //Something else has been clicked, let the frontend handle it
                return CallWindowProc(ogVideoOptWndProc, hWnd, message, wParam, lParam);
        }
    }
    break;
    default:
        return CallWindowProc(ogVideoOptWndProc, hWnd, message, wParam, lParam);
    }
    return 0;
}

HWND statisticsScreen;

//Function that hooks to the CreateDialogIndirectParamA method
typedef HWND(WINAPI* CreateDialogIndirectParamAType)(HINSTANCE hInstance, LPCDLGTEMPLATEA lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
CreateDialogIndirectParamAType pCreateDialogIndirectParamA = nullptr; //original function pointer after hook
CreateDialogIndirectParamAType pCreateDialogIndirectParamATarget; //original function pointer BEFORE hook do not call this!
HWND WINAPI detourCreateDialogIndirectParamA(HINSTANCE hInstance, LPCDLGTEMPLATEA lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    auto returnVal = pCreateDialogIndirectParamA(hInstance, lpTemplate, hWndParent, lpDialogFunc, dwInitParam);

    //Custom hints are unused for now
    //if (hint == NULL && hWndParent != NULL)
    //{
    //    CWnd* parent = CWnd::FromHandle(hWndParent);

    //    if (parent->GetParent())
    //    {
    //        hint = parent->GetParent()->GetDlgItem(1003);
    //        //advancedOptionsBtn.hintObject = hint;
    //    }
    //}

    if (returnVal != NULL) {

        CWnd* pWnd = CWnd::FromHandle(returnVal);

        CString title;
        pWnd->GetWindowTextW(title);
        double scale = GetDpiScaleFactor(returnVal);

        if (reposHintText) {
            if (scale == 2) {
                CWnd* txtHint = pWnd->GetDlgItem(1003);
                if (txtHint) {
                    CRect rectHint;
                    txtHint->GetWindowRect(&rectHint);
                    txtHint->GetParent()->ScreenToClient(&rectHint);
                    rectHint.bottom = rectHint.bottom + 4;
                    txtHint->MoveWindow(&rectHint);
                    reposHintText = false;
                }
            }
            else {
                reposHintText = false;
            }
        }
        
        if (reposExitButton){
            CWnd* btnExit = pWnd->GetDlgItem(1248);
            if (btnExit) {
                if (scale > 1 && title == "Worms2") {
                    CWnd* landingStart = pWnd->GetDlgItem(1216);
                    CRect rectExit;
                    CRect rectLanding;

                    btnExit->GetWindowRect(&rectExit);
                    // Convert screen coordinates to parent client coordinates
                    btnExit->GetParent()->ScreenToClient(&rectExit);

                    landingStart->GetWindowRect(&rectLanding);
                    landingStart->GetParent()->ScreenToClient(&rectLanding);

                    int rectLandingPad = (rectLanding.top - 4) * 2;
                    rectExit.bottom = 24 + (rectLanding.top * scale) + rectLandingPad;
                    //Original control dimensions: 27x24, top: 0
                    double btnExitMultiplier = (double)rectExit.bottom / 24;
                    rectExit.left = rectExit.right - (27 * btnExitMultiplier);
                    btnExit->MoveWindow(&rectExit);
                }
                reposExitButton = false;
            }
        }

        if (createAdvancedOptions && !(advancedOptionsBtn.GetSafeHwnd() && ::IsWindow(advancedOptionsBtn.GetSafeHwnd()))) {
            //Video options advanced button
            if(lastFoundResourceName == (LPCSTR)0xC3)
            {
                //Get preview rectangle
                CRect previewRect;
                CWnd* previewWnd = pWnd->GetDlgItem(1258);
                previewWnd->GetWindowRect(&previewRect);
                pWnd->ScreenToClient(&previewRect);
                //

                //Calculate text size, 2002 is one of the checkboxes in Video options menu.
                CWnd* comboBox = pWnd->GetDlgItem(2002);
                CDC* comboDC = comboBox->GetDC();

                CFont* font = comboBox->GetFont();

                CFont* old = comboDC->SelectObject(font);

                CSize textSize = comboDC->GetTextExtent(advancedOptionsLabel);

                comboDC->SelectObject(old);

                int buttonWidth = textSize.cx;
                if (leftAlign)
                    buttonWidth += round(8 * scale);
                else
                    buttonWidth += round(13 * scale);

                int buttonHeight = round(25 * scale);

                RECT btnRect;
                btnRect.left = previewRect.left + (previewRect.Width() / 2) - (buttonWidth / 2);
                btnRect.top = (previewRect.top / 2) - (buttonHeight / 2);
                btnRect.right = btnRect.left + buttonWidth;
                btnRect.bottom = btnRect.top + buttonHeight;

                if (leftAlign)
                    advancedOptionsBtn.Create(advancedOptionsLabel, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_LEFT, btnRect, pWnd, BUTTON_IDENTIFIER);
                else
                    advancedOptionsBtn.Create(advancedOptionsLabel, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, btnRect, pWnd, BUTTON_IDENTIFIER);

                advancedOptionsBtn.SetFont(font);

                SetWindowLongPtr(advancedOptionsBtn.m_hWnd, GWLP_USERDATA, BUTTON_IDENTIFIER);

                ogVideoOptWndProc = (WNDPROC)SetWindowLongPtr(returnVal, GWLP_WNDPROC, (LONG_PTR)VideoOptWndProc);

                return returnVal;
            }
        }
        // Music options - give track rows more room at high DPI.
        if (lastFoundResourceName == MAKEINTRESOURCEA(208) && scale > 1)
        {
            HWND trackList = ::GetDlgItem(returnVal, 2092);
            if (trackList) {
                LRESULT itemHeight = SendMessageA(trackList, LB_GETITEMHEIGHT, 0, 0);
                if (itemHeight > 0) {
                    int scaledHeight = (int)round(itemHeight * scale);
                    // LB_SETITEMHEIGHT accepts heights up to 255 pixels.
                    if (scaledHeight > 255)
                        scaledHeight = 255;
                    SendMessageA(trackList, LB_SETITEMHEIGHT, 0, scaledHeight);
                    InvalidateRect(trackList, NULL, TRUE);
                }
            }
        }
        // Team Editor dialog - Add play button for soundbanks
        if (lastFoundResourceName == MAKEINTRESOURCEA(131))
        {
            CWnd* cbScheme = pWnd->GetDlgItem(1027);
            if (cbScheme) {
                speechComboBoxHwnd = cbScheme->GetSafeHwnd();
                CRect comboRect;
                cbScheme->GetWindowRect(&comboRect);
                pWnd->ScreenToClient(&comboRect);

                // Load text from StringTable ID 2011 - "Play" in English
                char playButtonText[256] = {};
                if (LoadStringA(GetModuleHandleA(NULL), 2011, playButtonText, sizeof(playButtonText)) == 0) {
                    // Fallback if tring cannot be loaded
                    strcpy_s(playButtonText, "Play");
                }

                const int buttonWidth = (int)round(50 * scale);
                int buttonX = comboRect.right + round(10 * scale);
                int buttonY = comboRect.top;
                int buttonHeight = comboRect.Height();

                HWND speechPlayButtonHwnd = CreateWindowExA(
                    0,
                    "BUTTON",
                    playButtonText,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    buttonX,
                    buttonY,
                    buttonWidth,
                    buttonHeight,
                    returnVal,
                    (HMENU)(INT_PTR)SPEECH_PLAY_BUTTON_ID,
                    hInstance,
                    NULL
                );

                if (speechPlayButtonHwnd) {
                    // Make the new button use the same font as the combo box.
                    HFONT font = (HFONT)SendMessageW(speechComboBoxHwnd, WM_GETFONT, 0, 0);
                    if (font) {
                        SendMessageW(speechPlayButtonHwnd, WM_SETFONT,(WPARAM)font, TRUE);
                    }
                    // Intercept button clicks from this dialog.
                    ogTeamEditorWndProc = (WNDPROC)SetWindowLongPtr(returnVal, GWLP_WNDPROC, (LONG_PTR)TeamEditorWndProc);
                }
            }
        }
        //Weapon options - reset tab order
        if (IS_INTRESOURCE(lastFoundResourceName) && title.IsEmpty())
        {
            int dialogId;
            dialogId = static_cast<int>(reinterpret_cast<ULONG_PTR>(lastFoundResourceName));
            if (dialogId >= 4900 && dialogId <=4937) {
                TabOrder::Reset();
            }
        }

        //IPX address book
        if (overrideAddressBook)
        {
            if (lastFoundResourceName == (LPCSTR)0x192)
            {
                //Get original controls
                CWnd* orgAddressBook = pWnd->GetDlgItem(1243);

                CWnd* ipxBtn = pWnd->GetDlgItem(4003);
                CWnd* tcpBtn = pWnd->GetDlgItem(4004);

                tcpAddressDropdown = pWnd->GetDlgItem(1242);

                //Override original window processing methods
                ogNetworkPlayWndProc = (WNDPROC)SetWindowLongPtr(returnVal, GWLP_WNDPROC, (LONG_PTR)NetworkPlayWndProc);

                HWND addressBookHWND = orgAddressBook->GetSafeHwnd();
                ogAddressBookWndProc = (WNDPROC)SetWindowLongPtr(addressBookHWND, GWLP_WNDPROC, (LONG_PTR)AddressBookBtnWndProc);

                HWND ipxHWND = ipxBtn->GetSafeHwnd();
                ogIPXBtnWndProc = (WNDPROC)SetWindowLongPtr(ipxHWND, GWLP_WNDPROC, (LONG_PTR)IPXBtnWndProc);

                HWND tcpHWND = tcpBtn->GetSafeHwnd();
                ogTCPBtnWndProc = (WNDPROC)SetWindowLongPtr(tcpHWND, GWLP_WNDPROC, (LONG_PTR)TCPBtnWndProc);

                //Check if TCP or IPX is selected upon entering and reenable the IPX button if its disabled
                IPXEnabled = !IsWindowEnabled(addressBookHWND);

                if (IPXEnabled) 
                {
                    EnableWindow(addressBookHWND, true);

                    if (tcpAddressDropdown != NULL)
                        tcpAddressDropdown->ShowWindow(false);
                }

                return returnVal;
            }
        }

        //Statistics screen
        if(lastFoundResourceName == (LPCSTR)0x124)
        {
			statisticsScreen = returnVal;
        }
    }

    return returnVal;
}

std::string GetWindowTextAsString(HWND hWnd) {
    // Get the text length first
    int length = GetWindowTextLengthA(hWnd);
    if (length == 0) return ""; // No text or error

    // Allocate a buffer (including null terminator)
    std::string text(length + 1, '\0');

    // Retrieve the text
    GetWindowTextA(hWnd, &text[0], length + 1);

    // Remove excess null characters (optional)
    text.resize(length);

    return text;
}

//Function that hooks to the TextOutA method
typedef BOOL(WINAPI* TextOutAType)(HDC hdc, int x, int y, LPCSTR lpString, int c);
TextOutAType pTextOutA = nullptr; //original function pointer after hook
TextOutAType pTextOutATarget; //original function pointer BEFORE hook do not call this!
BOOL WINAPI detourTextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c) {
    RECT rect;
    rect.left = x;
    rect.top = y;
    rect.right = x;
    rect.bottom = y;

    HWND wnd = WindowFromDC(hdc);

    double scale = GetDpiScaleFactor(wnd);

    if (wnd == statisticsScreen)
    {
        rect.top = rect.top * scale;
        rect.bottom = rect.bottom * scale;

        rect.left = rect.left * scale;
        rect.right = rect.right * scale;
    }

    DrawTextA(hdc, lpString, c, &rect, DT_LEFT | DT_NOCLIP);

    return TRUE;
}


void AssignLabels() 
{
    if (lang == "en")
    {
        advancedOptionsLabel = _TEXT("Advanced options");
        //advancedOptionsBtn.hintText = _TEXT("\nChange advanced graphic settings such as resolution or the renderer");
    }
    else if (lang == "pl")
    {
        advancedOptionsLabel = _TEXT("Zaawansowane opcje");
    }
    else if (lang == "de")
    {
        advancedOptionsLabel = _TEXT("Erweiterte Einstellungen");
    }
    else if (lang == "es" || lang == "es-419")
    {
        advancedOptionsLabel = _TEXT("Opciones avanzadas");
    }
    else if (lang == "fr")
    {
        advancedOptionsLabel = _TEXT("Options avancées");
    }
    else if (lang == "it") 
    {
        advancedOptionsLabel = _TEXT("Impostazioni avanzate");
    }
    else if (lang == "nl") 
    {
        advancedOptionsLabel = _TEXT("Uitgebreide instellingen");
    }
    else if (lang == "pt" || lang == "pt-br")
    {
        advancedOptionsLabel = _TEXT("Opções Avançadas");
    }
    else if (lang == "ru")
    {
        advancedOptionsLabel = _TEXT("Расширенные настройки");
        leftAlign = true;
    }
    else if (lang == "sv")
    {
        advancedOptionsLabel = _TEXT("Avancerade inställningar");
    }
    else if (lang == "cs")
    {
        advancedOptionsLabel = _TEXT("Pokročilá nastavení");
    }
    else if (lang == "zh-Hans")
    {
        advancedOptionsLabel = _TEXT("高级选项");
    }
    else
    {
        advancedOptionsLabel = _TEXT("Advanced options");
    }
}

//int lastWeaponParamID = 0;
//int ObtainWeaponParamIDAddrRet = 0;
//DWORD CWndGetDlgItemTarget = 0;
//__declspec(naked) void ObtainWeaponParamID() {
//
//    __asm {
//        mov lastWeaponParamID, eax
//
//        call dword ptr[CWndGetDlgItemTarget]
//
//        mov[ebp - 38h], eax
//
//        jmp ObtainWeaponParamIDAddrRet
//    }
//}

void shutdown() {

    MH_Uninitialize();
}

bool Initialized = false;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        createAdvancedOptions = FileExists("settings.exe");
        overrideAddressBook = FileExists("ipxaddress.exe");

        MH_STATUS status = MH_Initialize();

        if (status != MH_OK)
        {
            std::string sStatus = MH_StatusToString(status);
            shutdown();
            return 0;
        }

        if (MH_CreateHookApiEx(L"user32", "CreateDialogIndirectParamA", &detourCreateDialogIndirectParamA, reinterpret_cast<void**>(&pCreateDialogIndirectParamA), reinterpret_cast<void**>(&pCreateDialogIndirectParamATarget)) != MH_OK) {
            shutdown();
            return 1;
        }

        if (MH_EnableHook(reinterpret_cast<void**>(pCreateDialogIndirectParamATarget)) != MH_OK) {
            shutdown();
            return 1;
        }

        if (MH_CreateHookApiEx(L"kernel32", "FindResourceA", &detourFindResourceA, reinterpret_cast<void**>(&pFindResourceA), reinterpret_cast<void**>(&pFindResourceATarget)) != MH_OK) {
            shutdown();
            return 1;
        }

        if (MH_EnableHook(reinterpret_cast<void**>(pFindResourceATarget)) != MH_OK) {
            shutdown();
            return 1;
        }

        if (MH_CreateHookApiEx(L"gdi32", "TextOutA", &detourTextOutA, reinterpret_cast<void**>(&pTextOutA), reinterpret_cast<void**>(&pTextOutATarget)) != MH_OK) {
            shutdown();
            return 1;
        }

        if (MH_EnableHook(reinterpret_cast<void**>(pTextOutATarget)) != MH_OK) {
            shutdown();
            return 1;
        }

        DWORD CFormViewSetWindowPos4Addr = Hooks::scanPattern2("CFormViewSetWindowPos4", "E8 55 4E 09 00 8B 45 F0 83 C0");
        DWORD CFormViewSetWindowPos5Addr = Hooks::scanPattern2("CFormViewSetWindowPos5", "E8 06 4E 09 00 8D 4D A4 E8 13 3C");

        DWORD CFormViewSetWindowPos6Addr = Hooks::scanPattern2("CFormViewSetWindowPos6", "E8 E6 4E 09 00 6A 00 8D 4D A4 E8 F1");
        DWORD CFormViewSetWindowPos7Addr = Hooks::scanPattern2("CFormViewSetWindowPos7", "E8 BF 4E 09 00 8D 4D A4 E8 CC");

        PatchCall((void*)CFormViewSetWindowPos5Addr, WeaponsSetWindowPos_Input); //Labels adjacent to trackbars
        PatchCall((void*)CFormViewSetWindowPos4Addr, WeaponsSetWindowPos_Input); //Trackbars

        PatchCall((void*)CFormViewSetWindowPos6Addr, WeaponsSetWindowPos_Button); //Checkbox label
        PatchCall((void*)CFormViewSetWindowPos7Addr, WeaponsSetWindowPos_Button); //Checkbox

        //DWORD CFromViewGetDlgItemCall = Hooks::scanPattern2("CFromViewGetDlgItemCall", "E8 B1 4B 09 00 89 45 C8");
        //CWndGetDlgItemTarget = CFromViewGetDlgItemCall + 5 + *(DWORD*)(CFromViewGetDlgItemCall + 1);
        //ObtainWeaponParamIDAddrRet = CFromViewGetDlgItemCall + 8;

		//Hooks::hookAsm(CFromViewGetDlgItemCall, (DWORD)ObtainWeaponParamID);

        if (!SecretWeapons::Install())
            OutputDebugStringA("fkSettings: Secret weapon editor is unavailable.\n");
        else if (!NetworkTeams::Install())
            OutputDebugStringA("fkSettings: Network computer teams are unavailable.\n");

        Initialized = true;

        if (FileExists("language.txt")) 
        {
            std::ifstream t("language.txt");
            std::stringstream buffer;
            buffer << t.rdbuf();
            lang = buffer.str();
        }
        else {
            lang = "en";
        }

        AssignLabels();
    }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        if(Initialized && lpReserved)
            shutdown();
        break;
    }
    return TRUE;
}

