#include "../fkSettings/Alliances.cpp"
#include "../fkSettings/OfflineAlliances.cpp"
#include <cstdio>
#include <stdexcept>

namespace OA = OfflineAlliances;
namespace AC = Alliances;

constexpr COLORREF ExpectedColours[] = {
    RGB(255, 128, 128), RGB(128, 128, 255), RGB(128, 255, 128),
    RGB(255, 255, 128), RGB(255, 128, 255), RGB(128, 255, 255)
};

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static void ImageTests()
{
    for (int size : { 16, 24, 32 })
    {
        HIMAGELIST images = AC::MakeImages(size);
        Check(images != nullptr, "create alliance colour images");
        HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, size, size);
        ReleaseDC(nullptr, screen);
        Check(dc && bitmap, "create colour rendering surface");
        HGDIOBJ old = SelectObject(dc, bitmap);
        const COLORREF background = RGB(231, 231, 231);
        HBRUSH brush = CreateSolidBrush(background);
        RECT bounds{ 0, 0, size, size };
        Check(ImageList_GetImageCount(images) == 6, "six colour images with no blank placeholder");
        for (int index = 0; index < 6; ++index)
        {
            FillRect(dc, &bounds, brush);
            Check(ImageList_Draw(images, index, dc, 0, 0, ILD_TRANSPARENT) != FALSE, "draw alliance image");
            const COLORREF actual = GetPixel(dc, size / 2, size / 2);
            const COLORREF expected = ExpectedColours[index];
            if (actual != expected)
                fprintf(stderr, "Image %d at %d px: expected RGB %06lX, rendered %06lX\n", index, size, expected, actual);
            Check(actual == expected, "alliance image renders its assigned colour instead of black");
            Check(GetPixel(dc, 0, 0) == background, "image padding stays transparent");
            Check(GetPixel(dc, 2, 2) == RGB(0, 0, 0), "colour square retains its outline");
        }
        DeleteObject(brush);
        SelectObject(dc, old);
        DeleteObject(bitmap);
        DeleteDC(dc);
        ImageList_Destroy(images);
    }
}

static void RosterTests()
{
    OA::Roster state;
    Check(!state.Enabled(), "empty roster has no alliances");
    state.Sync({ { 7, "First", 0 } });
    Check(!state.Enabled() && state.teams[0].colour == 0, "one team has no alliance colour");
    state.Cycle(7);
    Check(state.teams[0].colour == 0, "one-team colour shortcut is inactive");
    state.Sync({ { 3, "Second", 0 }, { 7, "First", 0 } });
    Check(state.Enabled() && state.teams[0].name == "First" && state.teams[0].colour == 1 && state.teams[1].colour == 2,
        "second team automatically colours both teams in insertion order, independent of list sorting");
    state.teams[1].colour = 4;
    state.Sync({ { 10, "Third", 0 }, { 3, "Second", 0 }, { 7, "First", 0 } });
    Check(state.teams[2].colour == 2, "red and yellow choose blue next");
    state.Sync({ { 10, "Third", 0 }, { 3, "Second", 0 } });
    Check(state.teams[0].colour == 4 && state.teams[1].colour == 2, "removal preserves remaining assignments");
    state.Sync({ { 7, "First", 0 }, { 10, "Third", 0 }, { 3, "Second", 0 } });
    Check(state.teams[2].colour == 1, "re-added team receives least-used colour");
    state.Cycle(3);
    Check(state.teams[0].colour == 5, "cycle changes the intended team");
    state.Cycle(3); state.Cycle(3);
    Check(state.teams[0].colour == 1, "teal wraps to red");
    state.Sync({ { 3, "Second", 0 } });
    Check(!state.Enabled() && state.teams[0].colour == 0, "returning to one team clears its colour");
    state.Sync({ { 10, "Third", 0 }, { 3, "Second", 0 } });
    for (size_t i = 0; i < state.teams.size(); ++i)
        Check(state.teams[i].colour == i + 1, "returning to two teams resets colours in insertion order");
    state.Sync({});
    Check(!state.Enabled() && state.teams.empty(), "removing all teams disables alliances");
    state = {};
    for (int i = 0; i < 6; ++i) state.teams.push_back({ i, std::to_string(i), static_cast<BYTE>(i + 1) });
    Check(state.LeastUsed() == 1, "equal use prioritizes red");
    state.teams[0].colour = 2;
    Check(state.LeastUsed() == 1, "unused red beats duplicated blue");
}

static int Insert(HWND list, int id, const char* name)
{
    LVITEMA item{};
    item.mask = LVIF_PARAM | LVIF_TEXT;
    item.pszText = const_cast<char*>(name);
    item.lParam = id;
    return static_cast<int>(SendMessageA(list, LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&item)));
}

static int ChildCount(HWND window)
{
    int count = 0;
    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) ++count;
    return count;
}

static void EditorTests()
{
    INITCOMMONCONTROLSEX common{ sizeof(common), ICC_LISTVIEW_CLASSES };
    Check(InitCommonControlsEx(&common) != FALSE, "initialize common controls");
    HWND page = CreateWindowExW(WS_EX_CONTROLPARENT, L"STATIC", L"Alliance fixture", WS_OVERLAPPED,
        0, 0, 600, 300, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HWND list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT,
        12, 20, 500, 200, page, reinterpret_cast<HMENU>(1033), GetModuleHandleW(nullptr), nullptr);
    Check(page && list, "create roster fixture");
    LVCOLUMNA column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<char*>("Team");
    column.cx = 220;
    SendMessageA(list, LVM_INSERTCOLUMNA, 0, reinterpret_cast<LPARAM>(&column));
    // Attach before native additions, as the setup refresh does during init.
    RECT originalBounds{};
    GetWindowRect(list, &originalBounds);
    const int children = ChildCount(page);
    Check(OA::Attach(page), "attach alliance editor");
    auto editor = static_cast<OA::Editor*>(GetPropW(page, OA::ContextProperty));
    Check(editor && !OA::roster.Enabled(), "new empty game starts inactive");
    RECT listRect{};
    GetWindowRect(list, &listRect);
    Check(EqualRect(&originalBounds, &listRect) && ChildCount(page) == children, "no checkbox or reserved list space");
    Check(Insert(list, 7, "First") >= 0, "add first team");
    Check(!OA::roster.Enabled() && ListView_GetItemState(list, 0, LVIS_STATEIMAGEMASK) == 0,
        "first team stays uncoloured until another team is added");
    Check(Insert(list, 3, "Second") >= 0 && OA::roster.Enabled(), "second team activates alliances");
    Check(OA::roster.teams[0].name == "First", "list inserting at the top retains addition order");
    Check(ListView_GetItemState(list, 1, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(1), "existing first team displays red");
    Check(ListView_GetItemState(list, 0, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2), "existing second team displays blue");
    // Space retains native handling and cannot change alliance colours.
    ListView_SetItemState(list, 0, LVIS_FOCUSED, LVIS_FOCUSED);
    SendMessageW(list, WM_KEYDOWN, VK_SPACE, 0);
    SendMessageW(list, WM_CHAR, L' ', 0);
    SendMessageW(list, WM_KEYUP, VK_SPACE, 0);
    Check(OA::roster.teams[1].colour == 2 &&
        ListView_GetItemState(list, 0, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2),
        "space leaves the focused team's colour and displayed square unchanged");
    // Exercise mouse hit testing on the state image.
    RECT row{};
    Check(ListView_GetItemRect(list, 0, &row, LVIR_BOUNDS) != FALSE, "find colour square row");
    const LPARAM point = MAKELPARAM(row.left + 5, (row.top + row.bottom) / 2);
    SendMessageW(list, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(list, WM_LBUTTONUP, 0, point);
    Check(OA::roster.teams[1].colour == 3, "left click still cycles forwards after pressing space");
    SendMessageW(list, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(list, WM_LBUTTONUP, 0, point);
    Check(OA::roster.teams[1].colour == 4, "click cycles square to yellow");
    for (int expected : { 3, 2, 1, 6, 5, 4 })
    {
        SendMessageW(list, WM_RBUTTONDOWN, MK_RBUTTON, point);
        Check(OA::roster.teams[1].colour == expected && OA::roster.teams[0].colour == 1,
            "right click reverses only the clicked team, including blue to red and red to teal");
        SendMessageW(list, WM_RBUTTONUP, 0, point);
        Check(OA::roster.teams[1].colour == expected &&
            ListView_GetItemState(list, 0, LVIS_STATEIMAGEMASK) == static_cast<UINT>(INDEXTOSTATEIMAGEMASK(expected)),
            "right release does not cycle again and the square displays the new colour");
    }
    SendMessageW(list, WM_RBUTTONDBLCLK, MK_RBUTTON, point);
    SendMessageW(list, WM_RBUTTONUP, 0, point);
    Check(OA::roster.teams[1].colour == 3, "second right click of a double click also reverses once");
    SendMessageW(list, WM_LBUTTONDBLCLK, MK_LBUTTON, point);
    SendMessageW(list, WM_LBUTTONUP, 0, point);
    Check(OA::roster.teams[1].colour == 4, "left double click still cycles forwards once");
    Check(Insert(list, 10, "Third") >= 0, "add team after enabling");
    Check(OA::roster.teams[2].colour == 2 && ListView_GetItemState(list, 0, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2),
        "new team immediately receives unused blue");
    ListView_DeleteItem(list, 2);
    Check(OA::roster.teams.size() == 2 && OA::roster.teams[0].colour == 4, "delete updates model and keeps yellow");
    ListView_DeleteItem(list, 0);
    Check(!OA::roster.Enabled() && ListView_GetImageList(list, LVSIL_STATE) == editor->nativeImages &&
        ListView_GetItemState(list, 0, LVIS_STATEIMAGEMASK) == 0, "one remaining team hides squares and restores native list");
    Check(Insert(list, 7, "First") >= 0 && OA::roster.Enabled(), "adding a second team reactivates alliances");
    Check(OA::roster.teams[0].colour == 1 && OA::roster.teams[1].colour == 2,
        "reactivation resets surviving and added teams to red/blue");
    Check(OA::Attach(page) && OA::roster.Enabled(), "repeated refresh retains activation");
    Check(DestroyWindow(page) != FALSE, "destroy editor and its list safely");
    Check(OA::roster.Enabled() && OA::roster.teams.size() == 2, "assignments survive for subsequent rounds");
}

static int writes = 0, nativeResult = 1;
static int __fastcall NativeWriter(void*, void*, const char*) { ++writes; return nativeResult; }
static bool observedLocal = false, observedFirstRound = false;
static void __fastcall NativeLocal(void*, void*)
{
    observedLocal = OA::localLaunch;
    observedFirstRound = OA::firstRoundLaunch;
}

static INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM) { return FALSE; }

static void CheckRenderedRow(HWND list, int row, COLORREF expected)
{
    Check(ListView_EnsureVisible(list, row, FALSE) != FALSE, "scroll colour square into view");
    RECT bounds{}, item{};
    GetClientRect(list, &bounds);
    Check(ListView_GetItemRect(list, row, &item, LVIR_BOUNDS) != FALSE, "find rendered roster row");
    if (item.bottom > bounds.bottom)
    {
        // The hidden native fixture can leave the final row partly clipped.
        ListView_Scroll(list, 0, item.bottom - bounds.bottom);
        ListView_GetItemRect(list, row, &item, LVIR_BOUNDS);
    }
    Check(item.top >= 0 && item.bottom <= bounds.bottom, "render entire roster row within the surface");
    int width = 0, height = 0;
    Check(ImageList_GetIconSize(ListView_GetImageList(list, LVSIL_STATE), &width, &height) != FALSE,
        "find state image size");
    HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, bounds.right, bounds.bottom);
    ReleaseDC(nullptr, screen);
    Check(dc && bitmap, "create roster rendering surface");
    HGDIOBJ old = SelectObject(dc, bitmap);
    SendMessageW(list, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
    const COLORREF actual = GetPixel(dc, item.left + width / 2, (item.top + item.bottom) / 2);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    if (actual != expected)
        fprintf(stderr, "Roster row %d: expected RGB %06lX, rendered %06lX; rect %ld,%ld,%ld,%ld, client %ld,%ld; state %X\n",
            row, expected, actual, item.left, item.top, item.right, item.bottom, bounds.right, bounds.bottom,
            ListView_GetItemState(list, row, LVIS_STATEIMAGEMASK));
    Check(actual == expected, "roster displays assigned colour, including red and teal");
}

static void NativeDialogTests(const char* path)
{
    INITCOMMONCONTROLSEX common{ sizeof(common), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES };
    Check(InitCommonControlsEx(&common) != FALSE, "initialize native dialog controls");
    HMODULE resources = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    Check(resources != nullptr, "load frontend dialog resources");
    HWND parent = CreateWindowExW(0, L"STATIC", L"Native dialog fixture", WS_OVERLAPPED,
        0, 0, 1000, 700, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HWND page = CreateDialogParamW(resources, MAKEINTRESOURCEW(149), parent, DialogProc, 0);
    Check(page != nullptr, "create native single-player dialog");
    RECT originalList{};
    GetWindowRect(GetDlgItem(page, 1033), &originalList);
    const LONG_PTR originalStyle = GetWindowLongPtrW(GetDlgItem(page, 1033), GWL_STYLE);
    const int children = ChildCount(page);
    const HWND nextTab = GetNextDlgTabItem(page, GetDlgItem(page, 1033), FALSE);
    Check(page && OA::Attach(page), "attach to actual single-player dialog 149");
    auto editor = static_cast<OA::Editor*>(GetPropW(page, OA::ContextProperty));
    RECT list{};
    GetWindowRect(editor->list, &list);
    Check(EqualRect(&list, &originalList) && ChildCount(page) == children,
        "native roster keeps its full height with no extra checkbox");
    Check(GetWindowLongPtrW(editor->list, GWL_STYLE) == originalStyle, "native roster retains its original tab-stop style");
    Check(GetNextDlgTabItem(page, editor->list, FALSE) == nextTab, "Tab proceeds directly to next native control");
    LVCOLUMNA column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<char*>("Team");
    column.cx = list.right - list.left - GetSystemMetrics(SM_CXVSCROLL);
    SendMessageA(editor->list, LVM_INSERTCOLUMNA, 0, reinterpret_cast<LPARAM>(&column));
    for (int i = 0; i < 6; ++i) Check(Insert(editor->list, i, std::to_string(i).c_str()) >= 0, "insert six native roster rows");
    Check(OA::roster.Enabled(), "six native rows automatically enable alliances");
    for (int row = 0; row < 6; ++row)
        Check(ListView_GetItemState(editor->list, row, LVIS_STATEIMAGEMASK) == static_cast<UINT>(INDEXTOSTATEIMAGEMASK(6 - row)),
            "all six actual native rows receive their colour squares");
    for (int row = 0; row < 6; ++row) CheckRenderedRow(editor->list, row, ExpectedColours[5 - row]);
    // The first-added team is the last visible row because insertion is at 0.
    // Verify its actual pixels through a full cycle, including teal and wraparound.
    for (int colour = 0; colour < 6; ++colour)
    {
        OA::CycleRow(*editor, 5);
        CheckRenderedRow(editor->list, 5, ExpectedColours[(colour + 1) % 6]);
    }
    ListView_DeleteAllItems(editor->list);
    Check(!OA::roster.Enabled() && ListView_GetImageList(editor->list, LVSIL_STATE) == editor->nativeImages,
        "empty native roster automatically hides all colour images");
    // Check each addition immediately, starting with saved-team index zero.
    for (int i = 0; i < 6; ++i)
    {
        Check(Insert(editor->list, i, std::to_string(i).c_str()) >= 0, "add native team to automatic alliances");
        if (i == 0) Check(!OA::roster.Enabled(), "first native team leaves alliances inactive");
        else
        {
            CheckRenderedRow(editor->list, 0, ExpectedColours[i]);
            CheckRenderedRow(editor->list, i, ExpectedColours[0]);
        }
    }
    DestroyWindow(parent);
    FreeLibrary(resources);
}

struct ImageRestoreCheck
{
    HIMAGELIST teams, state;
    bool restored = false;
};

static LRESULT CALLBACK ObserveImageRestore(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR reference)
{
    if (message == WM_NCDESTROY)
    {
        auto& check = *reinterpret_cast<ImageRestoreCheck*>(reference);
        check.restored = ListView_GetImageList(window, LVSIL_SMALL) == check.teams &&
            ListView_GetImageList(window, LVSIL_STATE) == check.state;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

static void DpiLayoutTests(const char* path)
{
    HMODULE resources = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    Check(resources != nullptr, "load native sprites for DPI checks");
    HBITMAP sprites = static_cast<HBITMAP>(LoadImageW(resources, MAKEINTRESOURCEW(244), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    HIMAGELIST native = ImageList_Create(32, 16, ILC_COLOR32 | ILC_MASK, 5, 0);
    Check(sprites && native && ImageList_AddMasked(native, sprites, RGB(255, 0, 255)) == 0 &&
        ImageList_GetImageCount(native) == 5, "load native worm and CPU sprite strip without resizing");
    DeleteObject(sprites);
    HWND parent = CreateWindowExW(0, L"STATIC", L"DPI fixture", WS_OVERLAPPED,
        0, 0, 1000, 700, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HWND page = CreateDialogParamW(resources, MAKEINTRESOURCEW(149), parent, DialogProc, 0);
    HWND list = GetDlgItem(page, 1033);
    Check(list != nullptr, "create actual roster for DPI checks");
    SetWindowPos(list, nullptr, 0, 0, 600, 300, SWP_NOZORDER);
    ListView_SetImageList(list, native, LVSIL_SMALL);
    ListView_SetBkColor(list, RGB(255, 255, 255));
    ListView_SetTextBkColor(list, RGB(255, 255, 255));
    ImageRestoreCheck restore{ native, ListView_GetImageList(list, LVSIL_STATE) };
    Check(SetWindowSubclass(list, ObserveImageRestore, 2, reinterpret_cast<DWORD_PTR>(&restore)) != FALSE,
        "observe original image-list restoration");
    LVCOLUMNA column{};
    column.mask = LVCF_WIDTH;
    column.cx = 580;
    SendMessageA(list, LVM_INSERTCOLUMNA, 0, reinterpret_cast<LPARAM>(&column));
    Check(OA::Attach(page), "attach DPI-aware roster");
    for (int team = 0; team < 6; ++team)
    {
        const int row = Insert(list, team, "Hgppy");
        LVITEMA item{};
        item.mask = LVIF_IMAGE;
        item.iItem = row;
        item.iImage = team % 5;
        Check(row >= 0 && SendMessageA(list, LVM_SETITEMA, 0, reinterpret_cast<LPARAM>(&item)), "assign native sprite index");
    }
    HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen), source = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 600, 300), sourceBitmap = CreateCompatibleBitmap(screen, 32, 16);
    ReleaseDC(nullptr, screen);
    Check(dc && source && bitmap && sourceBitmap, "create DPI rendering surfaces");
    HGDIOBJ old = SelectObject(dc, bitmap), oldSource = SelectObject(source, sourceBitmap);
    HFONT previousFont = nullptr;
    const COLORREF transparent = RGB(255, 0, 255), white = RGB(255, 255, 255);
    HBRUSH background = CreateSolidBrush(transparent);
    for (int dpi : { 96, 144, 192, 96, 192 })
    {
        // The dialog already scales its font with DPI; do not scale it twice.
        HFONT font = CreateFontW(-MulDiv(13, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");
        Check(font != nullptr, "create scaled roster font");
        SendMessageW(list, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        if (previousFont) DeleteObject(previousFont);
        previousFont = font;
        auto editor = static_cast<OA::Editor*>(GetPropW(page, OA::ContextProperty));
        int width = 0, height = 0, nativeWidth = 0, nativeHeight = 0;
        HIMAGELIST aligned = ListView_GetImageList(list, LVSIL_SMALL);
        Check(ImageList_GetIconSize(aligned, &width, &height) && width == 32 && height >= editor->imageSize,
            "sprite cells accommodate the DPI-scaled colour squares");
        Check(ImageList_GetImageCount(aligned) == 5 && ImageList_GetIconSize(native, &nativeWidth, &nativeHeight) &&
            nativeWidth == 32 && nativeHeight == 16, "shared native sprites keep their size, count and indices");
        Check((height > 16) == (aligned != native), "only taller fonts need a private padded sprite list");
        RECT surface{ 0, 0, 600, 300 };
        FillRect(dc, &surface, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SendMessageW(list, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
        for (int row = 0; row < 6; ++row)
        {
            RECT bounds{}, icon{};
            Check(ListView_GetItemRect(list, row, &bounds, LVIR_BOUNDS) &&
                ListView_GetItemRect(list, row, &icon, LVIR_ICON), "find actual DPI-scaled image positions");
            const COLORREF colour = ExpectedColours[5 - row];
            int top = bounds.bottom, bottom = -1;
            for (int y = bounds.top; y < bounds.bottom; ++y)
                for (int x = bounds.left; x < icon.left; ++x)
                    if (GetPixel(dc, x, y) == colour) { top = (std::min)(top, y); bottom = (std::max)(bottom, y); }
            if (bottom - top + 1 != editor->imageSize - 6)
                fprintf(stderr, "DPI %d row %d: square y %d..%d, cell %d, row y %ld..%ld, icon x %ld, colour %06lx\n",
                    dpi, row, top, bottom, editor->imageSize, bounds.top, bounds.bottom, icon.left, colour);
            Check(bottom - top + 1 == editor->imageSize - 6, "entire square stays in its text row, including the first row");
            Check(abs((top + bottom) - (bounds.top + bounds.bottom - 1)) <= 3,
                "colour square is vertically centred in its text row");
            LVITEMA item{};
            item.mask = LVIF_IMAGE;
            item.iItem = row;
            Check(SendMessageA(list, LVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item)) != FALSE, "read native sprite index");
            RECT sourceArea{ 0, 0, 32, 16 };
            FillRect(source, &sourceArea, background);
            Check(ImageList_Draw(native, item.iImage, source, 0, 0, ILD_TRANSPARENT) != FALSE, "render reference native sprite");
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < 32; ++x)
                {
                    const int sourceY = y - (height - 16) / 2;
                    COLORREF expected = sourceY >= 0 && sourceY < 16 ? GetPixel(source, x, sourceY) : transparent;
                    if (expected == transparent) expected = white;
                    Check(GetPixel(dc, icon.left + x, bounds.top + y) == expected,
                        "worm and CPU sprites retain every pixel and are centred within the scaled row");
                }
            const LPARAM point = MAKELPARAM(bounds.left + editor->imageSize / 2, (bounds.top + bounds.bottom) / 2);
            const UINT state = ListView_GetItemState(list, row, LVIS_STATEIMAGEMASK);
            SendMessageW(list, WM_LBUTTONDOWN, MK_LBUTTON, point);
            SendMessageW(list, WM_LBUTTONUP, 0, point);
            SendMessageW(list, WM_RBUTTONDOWN, MK_RBUTTON, point);
            SendMessageW(list, WM_RBUTTONUP, 0, point);
            Check(ListView_GetItemState(list, row, LVIS_STATEIMAGEMASK) == state,
                "both mouse directions still hit the correctly aligned DPI-scaled square");
        }
        const HIMAGELIST beforeRefresh = ListView_GetImageList(list, LVSIL_SMALL);
        Check(OA::Attach(page) && ListView_GetImageList(list, LVSIL_SMALL) == beforeRefresh,
            "repeated setup refresh keeps the existing aligned image list");
    }
    DestroyWindow(parent);
    Check(restore.restored && ImageList_GetImageCount(native) == 5, "cleanup restores both native lists without destroying shared sprites");
    DeleteObject(previousFont);
    SelectObject(dc, old);
    SelectObject(source, oldSource);
    DeleteObject(bitmap); DeleteObject(sourceBitmap); DeleteObject(background);
    DeleteDC(dc); DeleteDC(source); ImageList_Destroy(native); FreeLibrary(resources);
}

static std::array<BYTE, 7> ReadFileBytes(const std::string& path)
{
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open teams.dat");
    Check(GetFileSize(file, nullptr) == 7, "teams.dat is exactly seven bytes");
    std::array<BYTE, 7> bytes{};
    DWORD count;
    Check(ReadFile(file, bytes.data(), 7, &count, nullptr) && count == 7, "read teams.dat");
    CloseHandle(file);
    return bytes;
}

static void LaunchTests()
{
    OA::localLaunch = false;
    OA::RunLocal(nullptr, reinterpret_cast<OA::NativeVoid>(NativeLocal));
    Check(observedLocal && !observedFirstRound && !OA::localLaunch && !OA::firstRoundLaunch,
        "next-round scope does not enable another alliance shuffle");
    OA::RunLocal(nullptr, reinterpret_cast<OA::NativeVoid>(NativeLocal), true);
    Check(observedLocal && observedFirstRound && !OA::localLaunch && !OA::firstRoundLaunch,
        "first-round shuffle is scoped to the initial local launch");
    std::array<BYTE, 0xcf0> game{};
    const auto setName = [&](int slot, const char* name) {
        strcpy_s(reinterpret_cast<char*>(game.data() + 0x486 + slot * 0x108), 17, name);
    };
    OA::roster = {};
    OA::roster.Sync({ { 7, "First", 0 }, { 3, "Second", 0 }, { 10, "Third", 0 } });
    OA::roster.teams[1].colour = 1;
    setName(0, "Third"); setName(1, "First"); setName(2, "Second");
    OA::localLaunch = true;
    std::array<BYTE, 7> data{};
    Check(OA::LaunchData(game.data(), data) && data == std::array<BYTE, 7>{{ 1, 3, 1, 1, 0, 0, 0 }},
        "serialized colours follow reordered game players, sharing red and zeroing empty slots");
    setName(4, "Unknown");
    Check(!OA::LaunchData(game.data(), data), "unmatched active team cannot silently receive the wrong alliance");
    setName(4, "");
    Check(AC::SidecarPath("Data\\game.dat") == "Data\\teams.dat" &&
        AC::SidecarPath("D:/test/GAME.DAT") == "D:/test/teams.dat" && AC::SidecarPath("scheme.opt").empty(), "sidecar path follows game.dat");
    char temporary[MAX_PATH]{};
    Check(GetTempPathA(MAX_PATH, temporary) != 0, "temporary directory");
    char directory[MAX_PATH]{};
    Check(GetTempFileNameA(temporary, "fka", 0, directory) != 0, "reserve unique fixture path");
    DeleteFileA(directory);
    Check(CreateDirectoryA(directory, nullptr) != FALSE, "create fixture directory");
    const std::string gamePath = std::string(directory) + "\\game.dat";
    const std::string sidecar = AC::SidecarPath(gamePath.c_str());
    AC::originalWrite = reinterpret_cast<AC::NativeWrite>(NativeWriter);
    game[0x485] = 2; game[0x485 + 0x108] = 4; game[0x485 + 2 * 0x108] = 1;
    Check(AC::WriteGame(game.data(), nullptr, gamePath.c_str()) == 1 && writes == 1, "sidecar and native writer succeed");
    Check(ReadFileBytes(sidecar) == std::array<BYTE, 7>{{ 1, 3, 1, 1, 0, 0, 0 }}, "exact enabled bytes on disk");
    Check(AC::launchedTeams.size() == 3 && AC::launchedTeams[0].name == "Third" &&
        AC::launchedTeams[0].colour == 3 && AC::launchedTeams[2].name == "Second" && AC::launchedTeams[2].colour == 1,
        "successful launch freezes the actual serialized names and alliances");
    Check(AC::launchedTeams[0].wins == 2 && AC::launchedTeams[1].wins == 4 && AC::launchedTeams[2].wins == 1,
        "launch snapshot retains each named team's previous-round score");
    OA::roster.teams[1].colour = 6;
    Check(AC::launchedTeams[2].colour == 1, "later roster edits cannot change result scoring context");
    OA::roster.Sync({ { 7, "First", 0 } });
    setName(0, "First"); setName(1, ""); setName(2, "");
    Check(AC::WriteGame(game.data(), nullptr, gamePath.c_str()) == 1 && ReadFileBytes(sidecar) == std::array<BYTE, 7>{},
        "one-team launch overwrites previous alliances with seven zeros");
    Check(AC::launchedTeams.empty(), "inactive launch clears previous scoring context");
    OA::roster.Sync({}); setName(0, "");
    Check(AC::WriteGame(game.data(), nullptr, gamePath.c_str()) == 1 && ReadFileBytes(sidecar) == std::array<BYTE, 7>{},
        "empty launch writes seven zeros");
    OA::roster.Sync({ { 7, "First", 0 }, { 3, "Second", 0 } });
    setName(0, "Second"); setName(1, "First");
    Check(AC::WriteGame(game.data(), nullptr, gamePath.c_str()) == 1 &&
        ReadFileBytes(sidecar) == std::array<BYTE, 7>{{ 1, 2, 1, 0, 0, 0, 0 }},
        "two-team launch automatically writes active alliance assignments");
    Check(AC::launchedTeams.size() == 2 && AC::launchedTeams[0].name == "Second" && AC::launchedTeams[0].colour == 2,
        "next local launch replaces scoring context");
    OA::localLaunch = false;
    Check(AC::WriteGame(game.data(), nullptr, gamePath.c_str()) == 1 && ReadFileBytes(sidecar) == std::array<BYTE, 7>{},
        "online and Quick Play do not inherit local alliances");
    Check(AC::launchedTeams.empty(), "online and Quick Play clear alliance scoring context");
    OA::localLaunch = true;
    AC::launchedTeams.push_back({ "Old match", 1 });
    nativeResult = 0;
    Check(AC::WriteGame(game.data(), nullptr, gamePath.c_str()) == 0 && GetFileAttributesA(sidecar.c_str()) == INVALID_FILE_ATTRIBUTES,
        "native failure removes sidecar");
    Check(AC::launchedTeams.empty(), "native write failure cannot retain stale scoring context");
    nativeResult = 1;
    HANDLE blocked = CreateFileA(sidecar.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    Check(blocked != INVALID_HANDLE_VALUE, "lock sidecar fixture");
    const int before = writes;
    AC::launchedTeams.push_back({ "Old match", 1 });
    Check(AC::WriteGame(game.data(), nullptr, gamePath.c_str()) == 0 && writes == before, "sidecar failure stops native game writer");
    Check(AC::launchedTeams.empty(), "sidecar failure cannot retain stale scoring context");
    CloseHandle(blocked);
    DeleteFileA(sidecar.c_str());
    Check(RemoveDirectoryA(directory) != FALSE, "clean fixture directory");
    OA::localLaunch = false;
}

static const char* TurnNames[] = { "Red one", "Red two", "Red three", "Blue one", "Blue two", "Yellow" };
static const BYTE TurnColours[] = { 1, 1, 1, 2, 2, 4 };

static std::array<BYTE, 0xcf0> TurnGame()
{
    std::array<BYTE, 0xcf0> game{};
    game[0x483] = 6;
    for (int slot = 0; slot < 6; ++slot)
    {
        BYTE* record = game.data() + 0x484 + slot * 0x108;
        memset(record, 0x30 + slot, 0x108);
        memset(record + 2, 0, 17);
        strcpy_s(reinterpret_cast<char*>(record + 2), 17, TurnNames[slot]);
        *reinterpret_cast<DWORD*>(game.data() + 4 + slot * 4) = 0xabc000 + slot;
    }
    *reinterpret_cast<unsigned*>(game.data() + 0xc74) = 12345;
    return game;
}

static void TurnRoster()
{
    OA::roster = {};
    // Reverse UI/addition order to exercise name matching instead of row order.
    for (int index = 5; index >= 0; --index)
        OA::roster.teams.push_back({ index, TurnNames[index], TurnColours[index] });
}

static std::array<int, 2> turnDraws{};
static size_t turnDraw = 0;
static int __cdecl TurnRandom()
{
    Check(turnDraw < turnDraws.size(), "shuffle draws exactly once for each remaining colour choice");
    return turnDraws[turnDraw++];
}

static void TurnOrderTests()
{
    TurnRoster();
    const auto game = TurnGame();
    const auto originalRandom = AC::random;
    AC::random = TurnRandom;
    std::array<int, 6> order{};
    turnDraws = {{ 0, 0 }}; turnDraw = 0;
    Check(AC::AllianceTurnOrderFor(game.data(), OA::MatchTeams(), order) && order == std::array<int, 6>{{ 3, 5, 0, 4, 1, 2 }} && turnDraw == 2,
        "Blue Yellow Red yields Blue Yellow Red Blue Red Red");
    turnDraws = {{ 2, 1 }}; turnDraw = 0;
    Check(AC::AllianceTurnOrderFor(game.data(), OA::MatchTeams(), order) && order == std::array<int, 6>{{ 0, 3, 5, 1, 4, 2 }},
        "Red Blue Yellow yields Red Blue Yellow Red Blue Red");
    std::vector<std::array<BYTE, 3>> permutations;
    for (int last = 0; last < 3; ++last)
        for (int second = 0; second < 2; ++second)
        {
            turnDraws = {{ last, second }}; turnDraw = 0;
            Check(AC::AllianceTurnOrderFor(game.data(), OA::MatchTeams(), order), "build every possible three-colour ordering");
            std::array<BYTE, 3> first{{ TurnColours[order[0]], TurnColours[order[1]], TurnColours[order[2]] }};
            Check(first[0] != first[1] && first[0] != first[2] && first[1] != first[2], "each first cycle visits every colour once");
            Check(std::find(permutations.begin(), permutations.end(), first) == permutations.end(), "every colour permutation is reachable once");
            permutations.push_back(first);
            Check(TurnColours[order[5]] == 1, "remaining Red receives its final turn after other colours run out");
            std::array<BYTE, 2> remaining{};
            int next = 0;
            for (BYTE colour : first) if (colour != 4) remaining[next++] = colour;
            Check(TurnColours[order[3]] == remaining[0] && TurnColours[order[4]] == remaining[1],
                "following cycles keep the chosen colour order and skip exhausted Yellow");
        }
    for (auto& team : OA::roster.teams) team.colour = team.savedIndex < 3 ? 1 : 6;
    turnDraws = {{ 1, 0 }}; turnDraw = 0;
    Check(AC::AllianceTurnOrderFor(game.data(), OA::MatchTeams(), order) && order == std::array<int, 6>{{ 0, 3, 1, 4, 2, 5 }} && turnDraw == 1,
        "balanced Red and Teal teams alternate with one colour-order draw");
    for (auto& team : OA::roster.teams) team.colour = team.savedIndex < 5 ? 1 : 6;
    turnDraws = {{ 0, 0 }}; turnDraw = 0;
    Check(AC::AllianceTurnOrderFor(game.data(), OA::MatchTeams(), order) && order == std::array<int, 6>{{ 5, 0, 1, 2, 3, 4 }} && turnDraw == 1,
        "a lone Teal can start before five Reds and exhausted colours are skipped");
    turnDraw = 0;
    for (auto& team : OA::roster.teams) team.colour = static_cast<BYTE>(team.savedIndex + 1);
    Check(!AC::AllianceTurnOrderFor(game.data(), OA::MatchTeams(), order) && turnDraw == 0, "distinct colours retain the native shuffle and RNG state");
    for (auto& team : OA::roster.teams) team.colour = 1;
    Check(!AC::AllianceTurnOrderFor(game.data(), OA::MatchTeams(), order) && turnDraw == 0, "one colour cannot form an opposing alliance match");
    TurnRoster();
    auto invalid = game;
    for (int count : { 0, 1, 7 })
    {
        invalid[0x483] = static_cast<BYTE>(count);
        Check(!AC::AllianceTurnOrderFor(invalid.data(), OA::MatchTeams(), order) && turnDraw == 0, "invalid team counts do not consume randomness or reorder teams");
    }
    invalid = game;
    strcpy_s(reinterpret_cast<char*>(invalid.data() + 0x486), 17, "Unknown");
    Check(!AC::AllianceTurnOrderFor(invalid.data(), OA::MatchTeams(), order) && turnDraw == 0, "unmatched names preserve the native order");
    invalid = game;
    strcpy_s(reinterpret_cast<char*>(invalid.data() + 0x486), 17, TurnNames[1]);
    Check(!AC::AllianceTurnOrderFor(invalid.data(), OA::MatchTeams(), order) && turnDraw == 0, "duplicate names cannot duplicate a team in the turn order");
    Check(game == TurnGame(), "planning the turn order does not change game data");
    AC::random = originalRandom;
}

static std::array<BYTE, 0x100> turnThreadData{};
static void* __cdecl TurnThreadData() { return turnThreadData.data(); }
static void NativeTurnOrderTests(BYTE* image)
{
    // SEC_IMAGE does not relocate this private image. Relocate the native
    // memcpy jump-table operands/entries before executing its team-record copies.
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    size_t relocated = 0;
    for (size_t offset = 0; offset < directory.Size;)
    {
        const auto block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(image + directory.VirtualAddress + offset);
        Check(block->SizeOfBlock >= sizeof(*block), "valid native relocation block");
        const auto entries = reinterpret_cast<const WORD*>(block + 1);
        for (size_t index = 0; index < (block->SizeOfBlock - sizeof(*block)) / sizeof(WORD); ++index)
        {
            const size_t rva = block->VirtualAddress + (entries[index] & 0xfff);
            if ((entries[index] >> 12) != IMAGE_REL_BASED_HIGHLOW || rva < 0x96980 || rva >= 0x96cc0) continue;
            DWORD previous;
            Check(VirtualProtect(image + rva, 4, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "prepare native memcpy relocation");
            *reinterpret_cast<DWORD*>(image + rva) += reinterpret_cast<DWORD>(image) - nt->OptionalHeader.ImageBase;
            VirtualProtect(image + rva, 4, previous, &previous);
            ++relocated;
        }
        offset += block->SizeOfBlock;
    }
    Check(relocated > 0, "relocate native memcpy jump tables");
    FlushInstructionCache(GetCurrentProcess(), image + 0x96980, 0x340);
    Check(AC::swapTeams == reinterpret_cast<AC::NativeSwapTeams>(image + 0x2bd78) &&
        AC::random == reinterpret_cast<AC::NativeRandom>(image + 0x96770), "use validated native team swapping and random generation");
    // Supply only CRT thread-local storage. Run the real srand/rand, shuffle,
    // full-record/pointer swaps and later-round starter rotation machine code.
    Check(MH_CreateHook(image + 0x9d680, reinterpret_cast<void*>(TurnThreadData), nullptr) == MH_OK &&
        MH_EnableHook(image + 0x9d680) == MH_OK, "supply private CRT thread state for native shuffle execution");
    const auto shuffle = reinterpret_cast<OA::NativeVoid>(image + 0x16fe);
    const auto rotate = reinterpret_cast<OA::NativeVoid>(image + 0x1843);
    const auto original = TurnGame();
    auto native = original;
    OA::localLaunch = false;
    OA::firstRoundLaunch = false;
    shuffle(native.data());
    for (const auto context : { std::make_pair(false, true), std::make_pair(true, false) })
    {
        auto game = original;
        TurnRoster();
        OA::localLaunch = context.first; OA::firstRoundLaunch = context.second;
        shuffle(game.data());
        Check(game == native, "online Quick Play and later-round contexts retain the exact native shuffled bytes");
    }
    OA::localLaunch = OA::firstRoundLaunch = true;
    TurnRoster();
    for (auto& team : OA::roster.teams) team.colour = static_cast<BYTE>(team.savedIndex + 1);
    auto distinct = original;
    shuffle(distinct.data());
    Check(distinct == native, "offline games with all distinct colours retain the exact native shuffle");
    std::array<bool, 6> firstTeams{};
    for (unsigned seed = 1; seed <= 96; ++seed)
    {
        TurnRoster();
        auto game = original;
        *reinterpret_cast<unsigned*>(game.data() + 0xc74) = seed;
        const auto before = game;
        shuffle(game.data());
        std::array<BYTE, 6> colours{};
        unsigned seen = 0;
        for (int slot = 0; slot < 6; ++slot)
        {
            const char* name = reinterpret_cast<const char*>(game.data() + 0x486 + slot * 0x108);
            int source = 0;
            while (source < 6 && strcmp(name, TurnNames[source])) ++source;
            Check(source < 6 && !(seen & (1u << source)), "every team appears exactly once after native swapping");
            seen |= 1u << source;
            colours[slot] = TurnColours[source];
            if (!slot) firstTeams[source] = true;
            Check(!memcmp(game.data() + 0x484 + slot * 0x108, before.data() + 0x484 + source * 0x108, 0x108) &&
                *reinterpret_cast<const DWORD*>(game.data() + 4 + slot * 4) == 0xabc000u + source,
                "entire team records and their native pointers follow their names");
        }
        Check(colours[0] != colours[1] && colours[0] != colours[2] && colours[1] != colours[2] && colours[5] == 1,
            "seeded native shuffle starts with all three colours and ends with the remaining Red");
        int next = 3;
        for (int first = 0; first < 3; ++first)
            if (colours[first] != 4) Check(colours[next++] == colours[first], "native turn sequence keeps the first colour cycle's order");
        std::array<BYTE, 7> sidecar{};
        Check(OA::LaunchData(game.data(), sidecar) && sidecar[0] == 1, "serialize alliance colours after the final turn ordering");
        for (int slot = 0; slot < 6; ++slot) Check(sidecar[slot + 1] == colours[slot], "teams.dat follows the reordered team names");
        auto unchanged = game;
        for (int round = 1; round <= 7; ++round)
        {
            Check(game[0xab4] == (round - 1) % 6, "later rounds advance the starting index by one without another shuffle");
            rotate(game.data());
            unchanged[0xab4] = static_cast<BYTE>(round % 6);
            Check(game == unchanged, "native next-round rotation leaves all team data and other settings unchanged");
        }
    }
    Check(std::all_of(firstTeams.begin(), firstTeams.end(), [](bool first) { return first; }),
        "members within each colour remain randomized and every team can start");
    OA::localLaunch = OA::firstRoundLaunch = false;
    Check(MH_RemoveHook(image + 0x9d680) == MH_OK, "restore original native CRT thread access");
}

// Launch order reproduces teams.dat = 01 04 02 02 02 00 00.
static std::vector<AC::MatchTeam> ExampleTeams()
{
    return { { "Helmut", 4 }, { "Donut", 2 }, { "Lenny", 2 }, { "Buddy", 2 } };
}

struct ResultsFixture
{
    // Both native six-team tables, including their inline 17-byte names.
    std::array<BYTE, 0x7780> bytes{};

    int& Field(size_t offset) { return *reinterpret_cast<int*>(bytes.data() + offset); }
    const char* Name(size_t offset) const { return reinterpret_cast<const char*>(bytes.data() + offset); }
    void SetName(size_t offset, const char* name) { strcpy_s(reinterpret_cast<char*>(bytes.data() + offset), 17, name); }

    ResultsFixture()
    {
        Field(4) = 4;
        Field(0x1c) = 3;
        const char* round[] = { "Donut", "Helmut", "Lenny", "Buddy" };
        const char* total[] = { "Buddy", "Lenny", "Donut", "Helmut" };
        for (int row = 0; row < 4; ++row)
        {
            SetName(0x2c24 + row * 0x644, round[row]);
            SetName(0x68c + row * 0x644, total[row]);
        }
        Field(0x2bb8) = 1; // Only Donut survives; its eliminated allies still win.
    }

    void CheckWins(const std::vector<std::string>& winners)
    {
        for (int row = 0; row < Field(4); ++row)
            for (const auto& table : { std::make_pair(size_t(0x25f8), size_t(0x2c24)),
                std::make_pair(size_t(0x60), size_t(0x68c)) })
            {
                const char* name = Name(table.second + row * 0x644);
                const int expected = std::find(winners.begin(), winners.end(), name) != winners.end() ? 1 : 0;
                Check(Field(table.first + row * 0x644) == expected, "round and total win flags follow the winning names");
            }
    }
};

static void ResultsTests()
{
    const auto teams = ExampleTeams();
    ResultsFixture example;
    Check(AC::AwardWinningAlliance(example.bytes.data(), teams), "award alliance in the supplied one-versus-three example");
    example.CheckWins({ "Donut", "Lenny", "Buddy" });
    ResultsFixture expected;
    // Check the entire object: kills, damage, survivor/exclusion fields and all
    // other statistics must stay intact, with exactly these six win flags set.
    for (int row : { 0, 2, 3 }) expected.Field(0x25f8 + row * 0x644) = 1;
    for (int row : { 0, 1, 2 }) expected.Field(0x60 + row * 0x644) = 1;
    Check(example.bytes == expected.bytes, "only winning alliance flags change");
    Check(AC::AwardWinningAlliance(example.bytes.data(), teams) && example.bytes == expected.bytes,
        "awarding again does not increment counters or double-count a round");

    ResultsFixture solo;
    solo.Field(0x2bb8) = 0;
    solo.Field(0x2bb8 + 0x644) = 2;
    Check(AC::AwardWinningAlliance(solo.bytes.data(), teams), "lone opposing team can win instead");
    solo.CheckWins({ "Helmut" });

    ResultsFixture excluded;
    excluded.Field(0x2bb8 + 0x644) = 1;
    excluded.Field(0x2b84 + 0x644) = 1;
    Check(AC::AwardWinningAlliance(excluded.bytes.data(), teams), "excluded opponent is not a surviving winner");
    excluded.CheckWins({ "Donut", "Lenny", "Buddy" });

    ResultsFixture six;
    six.Field(4) = 6;
    six.SetName(0x2c24 + 4 * 0x644, "Fifth"); six.SetName(0x2c24 + 5 * 0x644, "Sixth");
    six.SetName(0x68c + 4 * 0x644, "Sixth"); six.SetName(0x68c + 5 * 0x644, "Fifth");
    auto sixTeams = teams;
    sixTeams.push_back({ "Fifth", 2 }); sixTeams.push_back({ "Sixth", 2 });
    Check(AC::AwardWinningAlliance(six.bytes.data(), sixTeams), "six-team match awards all five allied winners");
    six.CheckWins({ "Donut", "Lenny", "Buddy", "Fifth", "Sixth" });

    const auto unchanged = [](ResultsFixture result, const std::vector<AC::MatchTeam>& context) {
        const auto before = result.bytes;
        Check(!AC::AwardWinningAlliance(result.bytes.data(), context) && result.bytes == before,
            "draw, inactive or invalid context leaves all native result fields unchanged");
    };
    unchanged(ResultsFixture{}, {});
    for (int status : { 0, 1, 2, 4 })
    {
        ResultsFixture result; result.Field(0x1c) = status;
        unchanged(result, teams);
    }
    ResultsFixture draw; draw.Field(0x2bb8) = 0;
    unchanged(draw, teams);
    ResultsFixture conflicting; conflicting.Field(0x2bb8 + 0x644) = 1;
    unchanged(conflicting, teams);
    ResultsFixture missing; missing.SetName(0x2c24 + 3 * 0x644, "Unknown");
    unchanged(missing, teams);
    ResultsFixture duplicate; duplicate.SetName(0x2c24 + 3 * 0x644, "Lenny");
    unchanged(duplicate, teams);
    ResultsFixture missingTotal; missingTotal.SetName(0x68c + 3 * 0x644, "Unknown");
    unchanged(missingTotal, teams);
    ResultsFixture duplicateTotal; duplicateTotal.SetName(0x68c + 3 * 0x644, "Donut");
    unchanged(duplicateTotal, teams);
    ResultsFixture truncated; memset(truncated.bytes.data() + 0x2c24, 'X', 17);
    unchanged(truncated, teams);
    ResultsFixture mismatch; mismatch.Field(4) = 6;
    unchanged(mismatch, teams);
    for (BYTE colour : { BYTE(0), BYTE(7) })
    {
        auto invalid = teams; invalid[0].colour = colour;
        unchanged(ResultsFixture{}, invalid);
    }
}

static void Patch(BYTE* target, const void* bytes, size_t length)
{
    DWORD previous, ignored;
    Check(VirtualProtect(target, length, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "prepare native score fixture");
    memcpy(target, bytes, length);
    FlushInstructionCache(GetCurrentProcess(), target, length);
    VirtualProtect(target, length, previous, &ignored);
}

// Execute the actual score-increment block inside the native result reader,
// supplying its controller and winner locals without invoking its MFC file IO.
__declspec(naked) static void __fastcall IncrementNativeScore(void*, int, BYTE*)
{
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 60h
        mov [ebp-60h], ecx
        mov [ebp-18h], edx
        call dword ptr [ebp+8]
        mov esp, ebp
        pop ebp
        ret 4
    }
}

static BYTE* nativeScoreBlock = nullptr;
static int nativeWinningRow = 0, nativeReadResult = 0;
static int __fastcall ReadNativeFixture(void* object, void*)
{
    auto controller = static_cast<BYTE*>(object);
    if (nativeReadResult == 0 && *reinterpret_cast<int*>(controller + 0xfa2c) == 3)
        IncrementNativeScore(object, nativeWinningRow, nativeScoreBlock);
    return nativeReadResult;
}

static void ScorePipelineTests(BYTE* image)
{
    const BYTE previous = image[0x1ec5a], ret = 0xc3;
    Patch(image + 0x1ec5a, &ret, 1); // End after the native one-team increment.
    nativeScoreBlock = image + 0x1ec32;
    const auto originalRead = AC::originalReadResults;
    AC::originalReadResults = reinterpret_cast<AC::NativeReadResults>(ReadNativeFixture);
    const auto read = reinterpret_cast<AC::NativeReadResults>(image + 0x254a);
    const auto award = reinterpret_cast<OA::NativeVoid>(image + 0x2180);
    using Merge = void (__thiscall*)(void*, const void*);
    const auto merge = reinterpret_cast<Merge>(image + 0x2e220);
    using Winner = int (__thiscall*)(void*);
    const auto gameWinner = reinterpret_cast<Winner>(image + 0x2bc8e);
    const auto statsWinner = reinterpret_cast<Winner>(image + 0x2df18);
    std::vector<BYTE> controller(0x31948);
    BYTE* game = controller.data() + 0x30c58;
    auto teams = std::vector<AC::MatchTeam>{ { "Buddy", 2 }, { "TEAM17MicroProse", 2 }, { "Donut", 4 } };
    const auto prepareGame = [&](const std::vector<int>& order) {
        memset(game, 0, 0xcf0);
        game[0x483] = 3;
        game[0xc6d] = 2; // Two victories required.
        for (int row = 0; row < 3; ++row)
        {
            const auto& team = teams[order[row]];
            strcpy_s(reinterpret_cast<char*>(game + 0x486 + row * 0x108), 17, team.name.c_str());
            game[0x485 + row * 0x108] = team.wins;
        }
        *reinterpret_cast<int*>(controller.data() + 0xfa2c) = 3;
        AC::launchedTeams = teams;
    };
    const auto scoreFor = [&](const std::string& name) {
        for (int row = 0; row < 3; ++row)
            if (name == reinterpret_cast<const char*>(game + 0x486 + row * 0x108)) return game[0x485 + row * 0x108];
        throw std::runtime_error("missing live team in score fixture");
    };
    const auto prepareStats = [&](ResultsFixture& stats, const std::vector<int>& order, const std::string& survivor) {
        stats.bytes.fill(0);
        stats.Field(4) = 3;
        stats.Field(0xc) = 2;
        stats.Field(0x1c) = 3;
        for (int row = 0; row < 3; ++row)
        {
            stats.SetName(0x2c24 + row * 0x644, teams[order[row]].name.c_str());
            stats.SetName(0x68c + row * 0x644, teams[order[2 - row]].name.c_str());
            stats.Field(0x2bb8 + row * 0x644) = teams[order[row]].name == survivor ? 1 : 0;
        }
    };
    const auto checkScores = [&](ResultsFixture& accumulated, int buddy, int partner, int donut) {
        const int expected[] = { buddy, partner, donut };
        for (int team = 0; team < 3; ++team)
        {
            Check(scoreFor(teams[team].name) == expected[team], "Worms2 tab retains every named team's cumulative score");
            for (int row = 0; row < 3; ++row)
                if (teams[team].name == accumulated.Name(0x68c + row * 0x644))
                    Check(accumulated.Field(0x60 + row * 0x644) == expected[team],
                        "native accumulated Game statistics agree with live game scores");
        }
    };

    for (bool alternateWinners : { false, true })
    {
        for (auto& team : teams) team.wins = 0;
        ResultsFixture accumulated;
        prepareStats(accumulated, { 1, 0, 2 }, "");
        for (int round = 0; round < (alternateWinners ? 3 : 2); ++round)
        {
            const std::vector<int> order = round == 0 ? std::vector<int>{ 0, 2, 1 } :
                round == 1 ? std::vector<int>{ 2, 1, 0 } : std::vector<int>{ 1, 0, 2 };
            prepareGame(order);
            const int winningTeam = alternateWinners && round == 1 ? 2 : round == 0 ? 0 : 1;
            const std::string survivor = teams[winningTeam].name;
            nativeWinningRow = static_cast<int>(std::find(order.begin(), order.end(), winningTeam) - order.begin());
            Check(read(controller.data()) == 0, "hooked native result reader succeeds");
            const std::array<BYTE, 3> once{{ scoreFor(teams[0].name), scoreFor(teams[1].name), scoreFor(teams[2].name) }};
            Check(AC::UpdateMatchScores(game, 3, AC::launchedTeams), "score repair accepts an already updated table");
            Check(once == std::array<BYTE, 3>{{ scoreFor(teams[0].name), scoreFor(teams[1].name), scoreFor(teams[2].name) }},
                "a repeated score refresh adds no extra round");
            ResultsFixture result;
            prepareStats(result, order, survivor);
            award(result.bytes.data());
            merge(accumulated.bytes.data(), result.bytes.data());
            if (round == 0) checkScores(accumulated, 1, 1, 0);
            else if (alternateWinners && round == 1) checkScores(accumulated, 1, 1, 1);
            else checkScores(accumulated, 2, 2, alternateWinners ? 1 : 0);
            const bool finished = round == (alternateWinners ? 2 : 1);
            Check((gameWinner(game) >= 0) == finished && (statsWinner(accumulated.bytes.data()) >= 0) == finished,
                "native match-completion checks in both tabs finish on the same round");
            for (auto& team : teams) team.wins = scoreFor(team.name);
        }
    }
    for (auto& team : teams) team.wins = 0;
    prepareGame({ 0, 1, 2 });
    nativeWinningRow = 0;
    nativeReadResult = -1;
    Check(read(controller.data()) == -1 && scoreFor("Buddy") == 0 && scoreFor("TEAM17MicroProse") == 0,
        "failed result read preserves scores and return code");
    nativeReadResult = 0;
    *reinterpret_cast<int*>(controller.data() + 0xfa2c) = 2;
    Check(read(controller.data()) == 0 && scoreFor("Buddy") == 0 && scoreFor("TEAM17MicroProse") == 0,
        "draw result grants no live score");
    *reinterpret_cast<int*>(controller.data() + 0xfa2c) = 3;
    AC::launchedTeams.clear();
    Check(read(controller.data()) == 0 && scoreFor("Buddy") == 1 && scoreFor("TEAM17MicroProse") == 0,
        "non-alliance result reader keeps native one-winner scoring");
    const auto unchanged = [&](const std::vector<AC::MatchTeam>& context, int status = 3) {
        std::array<BYTE, 0xcf0> before{};
        memcpy(before.data(), game, before.size());
        Check(!AC::UpdateMatchScores(game, status, context) && !memcmp(game, before.data(), before.size()),
            "invalid or non-winning score data preserves all native game fields");
    };
    prepareGame({ 0, 1, 2 });
    game[0x485] = 1;
    unchanged({});
    unchanged(teams, 2);
    auto invalid = teams; invalid[0].colour = 0;
    unchanged(invalid);
    strcpy_s(reinterpret_cast<char*>(game + 0x486), 17, "Unknown");
    unchanged(teams);
    strcpy_s(reinterpret_cast<char*>(game + 0x486), 17, teams[1].name.c_str());
    unchanged(teams);
    prepareGame({ 0, 1, 2 });
    game[0x485] = 1; game[0x485 + 2 * 0x108] = 1;
    unchanged(teams);
    game[0x485 + 2 * 0x108] = 0; game[0x485] = 2;
    unchanged(teams);
    AC::originalReadResults = originalRead;
    Patch(image + 0x1ec5a, &previous, 1);
}

static int textDraws = 0;
static void* expectedTextObject = nullptr;
static UINT expectedTextId = 0;

static void __fastcall DrawTextFixture(void* object, void*, UINT stringId, int x, int y, int size, COLORREF colour)
{
    Check(object == expectedTextObject && stringId == expectedTextId && x == 20 && y == 49 && size == 120 && colour == RGB(255, 0, 0),
        "visible statistic strings retain every native drawing argument");
    ++textDraws;
}

static void WinnerTextTests(BYTE* image)
{
    const auto originalStatistics = AC::statistics;
    const auto originalDraw = AC::originalDrawString;
    Check(originalStatistics == image + 0x17eb60, "winner text uses the accumulated statistics table");
    AC::originalDrawString = reinterpret_cast<AC::NativeDrawString>(DrawTextFixture);
    const auto draw = reinterpret_cast<AC::NativeDrawString>(image + 0x2f9f);
    ResultsFixture results;
    results.Field(0xc) = 2;
    expectedTextObject = &results;
    const auto checkDrawing = [&](bool multiple) {
        for (UINT stringId : { 338u, 340u, 337u, 339u, 341u, 344u, 345u })
        {
            const int before = textDraws;
            expectedTextId = stringId;
            draw(expectedTextObject, stringId, 20, 49, 120, RGB(255, 0, 0));
            const bool hidden = multiple && (stringId == 338 || stringId == 340);
            Check(textDraws == before + (hidden ? 0 : 1), "both winner sentences hide only for multiple game winners");
        }
    };
    AC::statistics = results.bytes.data();
    results.Field(0x60) = 1;
    results.Field(0x60 + 0x644) = 1;
    checkDrawing(false); // A tied lead is not a shared game victory.
    results.Field(0x60) = 2;
    checkDrawing(false); // One winner retains the normal sentence.
    results.Field(0x60 + 0x644) = 2;
    checkDrawing(true);
    results.Field(0x60 + 3 * 0x644) = 3;
    checkDrawing(true); // Sorting, position and scores above the limit are immaterial.
    results.Field(4) = 1;
    checkDrawing(false); // Ignore records outside the current team count.
    results.Field(4) = 6;
    for (int row = 0; row < 6; ++row) results.Field(0x60 + row * 0x644) = 2;
    checkDrawing(true);
    for (int invalidCount : { 0, -1, 7 })
    {
        results.Field(4) = invalidCount;
        checkDrawing(false);
    }
    results.Field(4) = 4;
    for (int invalidLimit : { 0, -1 })
    {
        results.Field(0xc) = invalidLimit;
        checkDrawing(false);
    }
    AC::statistics = nullptr;
    checkDrawing(false);
    AC::statistics = originalStatistics;
    AC::originalDrawString = originalDraw;
}

static void HookTests(const char* path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    Check(mapping != nullptr, "map frontend");
    BYTE* image = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(image != nullptr, "view frontend");
    const auto award = reinterpret_cast<OA::NativeVoid>(image + 0x2180);
    ResultsFixture native;
    award(native.bytes.data());
    native.CheckWins({ "Donut" });
    Check(MH_Initialize() == MH_OK, "initialize hooks");
    const size_t sites[] = { 0x78f7b, 0x79343, 0x77da2, 0x2ba3, 0x2df64, 0x1e9bc, 0x7d917, 0x2bcfc, 0x2bd78, 0x96770 };
    for (size_t site : sites)
    {
        DWORD old;
        Check(VirtualProtect(image + site, 1, PAGE_EXECUTE_READWRITE, &old) != FALSE, "prepare signature test");
        const BYTE original = image[site];
        image[site] = 0x90;
        Check(!(site == 0x78f7b || site == 0x79343 || site == 0x77da2 ? OA::InstallInImage(image) : AC::InstallInImage(image)),
            "reject altered signature before installing any hooks in its module");
        image[site] = original;
        VirtualProtect(image + site, 1, old, &old);
    }
    Check(AC::InstallInImage(image) && OA::InstallInImage(image), "install shared and local native alliance hooks");
    Check(image[0x78f7b] == 0xe9 && image[0x79343] == 0xe9 && image[0x77da2] == 0xe9, "activate local setup and round hooks");
    Check(image[0x2df64] == 0xe9, "activate result-awarding hook at native implementation");
    Check(image[0x1e9bc] == 0xe9, "activate live match-score hook before both results tabs");
    Check(image[0x7d917] == 0xe9, "activate statistics winner-sentence drawing hook");
    Check(image[0x2bcfc] == 0xe9, "activate first-round alliance turn-order hook");
    NativeTurnOrderTests(image);
    WinnerTextTests(image);
    ScorePipelineTests(image);
    AC::launchedTeams = ExampleTeams();
    ResultsFixture allied;
    award(allied.bytes.data());
    allied.CheckWins({ "Donut", "Lenny", "Buddy" });
    const auto once = allied.bytes;
    award(allied.bytes.data());
    Check(allied.bytes == once, "native thunk plus alliance hook remains idempotent");
    AC::launchedTeams.clear();
    ResultsFixture inactive;
    award(inactive.bytes.data());
    Check(inactive.bytes == native.bytes, "no launch context preserves exact native single-winner behaviour");
    // NetworkTeams hooks the writer entry after this module hooks its thunk.
    // Execute that chain without invoking the frontend's uninitialized MFC CRT.
    Check(MH_CreateHook(image + 0x26e38, reinterpret_cast<void*>(NativeWriter), nullptr) == MH_OK &&
        MH_EnableHook(image + 0x26e38) == MH_OK, "install second feature's writer hook after the thunk");
    const int before = writes;
    const auto thunk = reinterpret_cast<AC::NativeWrite>(image + 0x2ba3);
    Check(thunk(nullptr, "unrelated.dat") == 1 && writes == before + 1, "thunk still reaches subsequently installed writer hook");
    Check(MH_Uninitialize() == MH_OK, "restore native code");
    UnmapViewOfFile(image); CloseHandle(mapping); CloseHandle(file);
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Usage: OfflineAlliancesTests.exe frontend.exe");
        OA::RegisterLocalHandlers();
        ImageTests(); RosterTests(); EditorTests(); NativeDialogTests(argv[1]); DpiLayoutTests(argv[1]);
        TurnOrderTests(); LaunchTests(); ResultsTests(); HookTests(argv[1]);
        puts("PASS: automatic alliances, rendering and controls, DPI-aligned sprites and squares, interleaved alliance turn order and native round rotation, launch data and scores, consecutive-round native score/aggregation/victory checks, winner text IDs 338/340, failures and native hooks");
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
