#pragma once
#include <string>

namespace SecretWeapons
{
    // MinHook must already be initialized. Unsupported frontends are left alone.
    bool Install();
    void SetLanguage(const std::string& language);
}
