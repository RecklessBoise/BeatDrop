#pragma once

#include <juce_core/juce_core.h>

namespace beatdrop::Keychain
{
    /** False on platforms without a native secret store yet (TokenStore then falls back to a file). */
    bool isAvailable();

    bool store (const juce::String& service, const juce::String& account, const juce::String& secret);
    std::optional<juce::String> load (const juce::String& service, const juce::String& account);
    bool remove (const juce::String& service, const juce::String& account);
}
