#pragma once

#include <juce_core/juce_core.h>

namespace beatdrop
{

/** Persists the OAuth refresh token: macOS Keychain, or a 0600 file elsewhere. */
class TokenStore
{
public:
    TokenStore() = default;

    /** File-only storage, used by tests so they never touch the real Keychain. */
    explicit TokenStore (juce::File fileOnlyStorage);

    bool saveRefreshToken (const juce::String& token);
    juce::String loadRefreshToken() const;
    void clear();

private:
    juce::File getFallbackFile() const;
    bool canUseKeychain() const;

    juce::String service { "com.recklessboise.beatdrop" };
    juce::String account { "youtube-refresh-token" };
    juce::File fallbackFile;
    bool useKeychain = true;
};

} // namespace beatdrop
