#include "TokenStore.h"
#include "Keychain.h"
#include "Settings.h"

#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif

namespace beatdrop
{

TokenStore::TokenStore (juce::File fileOnlyStorage)
    : fallbackFile (std::move (fileOnlyStorage)), useKeychain (false)
{
}

bool TokenStore::canUseKeychain() const
{
    return useKeychain && Keychain::isAvailable();
}

juce::File TokenStore::getFallbackFile() const
{
    if (fallbackFile != juce::File())
        return fallbackFile;

    return Settings::getSettingsDirectory().getChildFile ("token");
}

bool TokenStore::saveRefreshToken (const juce::String& token)
{
    if (canUseKeychain() && Keychain::store (service, account, token))
    {
        getFallbackFile().deleteFile();
        return true;
    }

    // Keychain refused (e.g. a sandboxed host): keep the token in a private file instead.
    const auto file = getFallbackFile();
    file.getParentDirectory().createDirectory();

    if (! file.replaceWithText (token))
        return false;

   #if ! JUCE_WINDOWS
    ::chmod (file.getFullPathName().toRawUTF8(), S_IRUSR | S_IWUSR);
   #endif

    return true;
}

juce::String TokenStore::loadRefreshToken() const
{
    if (canUseKeychain())
        if (auto token = Keychain::load (service, account))
            return *token;

    const auto file = getFallbackFile();
    return file.existsAsFile() ? file.loadFileAsString().trim() : juce::String();
}

void TokenStore::clear()
{
    if (canUseKeychain())
        Keychain::remove (service, account);

    getFallbackFile().deleteFile();
}

} // namespace beatdrop
