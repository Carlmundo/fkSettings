#pragma once
#include <vector>

typedef struct IUnknown IUnknown;
#include <Windows.h>

namespace TabOrder
{
    constexpr char LayoutEndPattern[] = "E8 1C F8 09 00 C7 45 FC FF FF FF FF 8B 45 A0";

    class Batch
    {
        std::vector<HWND> controls;
        HWND page = nullptr;
        bool batching = false;

    public:
        void Enable(bool enabled) { batching = enabled; }

        void Reset(HWND window)
        {
            controls.clear();
            page = window;
        }

        void Add(HWND window)
        {
            if (!window || (batching && GetParent(window) != page)) return;
            controls.push_back(window);
            // Keep the original repair if the layout-completion call could not be patched.
            if (!batching)
                for (size_t i = 0; i < controls.size(); ++i)
                    SetWindowPos(controls[i], i == 0 ? HWND_TOP : controls[i - 1], 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        void Apply(HWND window)
        {
            if (!batching || window != page) return;
            // Window positioning sends synchronous messages; drain before applying.
            std::vector<HWND> pending;
            pending.swap(controls);
            HWND insertAfter = HWND_TOP;
            if (!pending.empty() && IsWindow(pending.back()) && GetParent(pending.back()) == window)
                insertAfter = GetWindow(pending.back(), GW_HWNDPREV);
            for (size_t i = 0; i < pending.size(); ++i)
            {
                // The native layout moves its final input to HWND_TOP after the old
                // repair, then positions its readout. Keep any trailing readout above
                // the inputs, preserving sibling order as well as keyboard traversal.
                HWND control = pending[i == 0 ? pending.size() - 1 : i - 1];
                if (!IsWindow(control) || GetParent(control) != window) continue;
                SetWindowPos(control, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                insertAfter = control;
            }
        }
    };
}
