#pragma once

namespace FrontendNetwork
{
    // Install after SecretWeapons; MinHook must already be initialized.
    // Owns the shared lobby send/receive hooks for every packet extension.
    bool Install();
}
