#pragma once

namespace DirectPlayCompat
{
    // MinHook must already be initialized. Adapts only the IPX wrapper's
    // SPInit export; existing provider callbacks are preserved.
    // XP SP3's validated DirectPlay send path also rejects an absent provider
    // table during shutdown instead of dereferencing it.
    bool Install();
}
