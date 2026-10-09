#pragma once

namespace DirectPlayCompat
{
    // MinHook must already be initialized. Only the IPX wrapper's SPInit
    // export is adapted; existing provider callbacks are preserved.
    bool Install();
}
