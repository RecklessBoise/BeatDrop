#include "Keychain.h"

// TODO(windows): use the Windows Credential Manager (CredWriteW / CredReadW).
namespace beatdrop::Keychain
{
    bool isAvailable()                                                          { return false; }
    bool store (const juce::String&, const juce::String&, const juce::String&)  { return false; }
    std::optional<juce::String> load (const juce::String&, const juce::String&) { return std::nullopt; }
    bool remove (const juce::String&, const juce::String&)                      { return false; }
}
