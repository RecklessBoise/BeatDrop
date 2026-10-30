#pragma once

#include <juce_core/juce_core.h>

/** UTF-8 string literal: "Réglages"_t.
    juce::String treats plain char literals as ASCII, which would garble accents. */
inline juce::String operator""_t (const char* text, std::size_t length)
{
    return juce::String::fromUTF8 (text, (int) length);
}
