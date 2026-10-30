#include "Settings.h"
#include "Core/Text.h"
#include "Metadata.h"

namespace beatdrop
{

namespace Keys
{
    static const juce::String producerName        { "producerName" };
    static const juce::String titleTemplate       { "titleTemplate" };
    static const juce::String descriptionTemplate { "descriptionTemplate" };
    static const juce::String defaultPrivacy      { "defaultPrivacy" };
    static const juce::String googleClientId      { "googleClientId" };
    static const juce::String googleClientSecret  { "googleClientSecret" };
}

static juce::PropertiesFile::Options makeOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "BeatDrop";
    options.filenameSuffix = ".settings";
    options.folderName = "BeatDrop";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.millisecondsBeforeSaving = 0;
    return options;
}

Settings::Settings()
    : Settings (getSettingsDirectory().getChildFile ("BeatDrop.settings"))
{
}

Settings::Settings (const juce::File& settingsFile)
{
    settingsFile.getParentDirectory().createDirectory();
    properties = std::make_unique<juce::PropertiesFile> (settingsFile, makeOptions());
}

juce::File Settings::getSettingsDirectory()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support/BeatDrop");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("BeatDrop");
   #endif
}

juce::File Settings::getOutputDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("BeatDrop");
}

juce::String Settings::get (const juce::String& key, const juce::String& fallback) const
{
    const juce::ScopedLock sl (lock);
    return properties->getValue (key, fallback);
}

void Settings::set (const juce::String& key, const juce::String& value)
{
    {
        const juce::ScopedLock sl (lock);
        properties->setValue (key, value);
        properties->saveIfNeeded();
    }

    sendChangeMessage();
}

juce::String Settings::getProducerName() const          { return get (Keys::producerName, {}); }
juce::String Settings::getTitleTemplate() const         { return get (Keys::titleTemplate, Metadata::defaultTitleTemplate); }
juce::String Settings::getDescriptionTemplate() const   { return get (Keys::descriptionTemplate, Metadata::defaultDescriptionTemplate); }
juce::String Settings::getDefaultPrivacy() const        { return get (Keys::defaultPrivacy, Privacy::publicVideo); }
juce::String Settings::getGoogleClientId() const        { return get (Keys::googleClientId, {}); }
juce::String Settings::getGoogleClientSecret() const    { return get (Keys::googleClientSecret, {}); }

void Settings::setProducerName (const juce::String& v)          { set (Keys::producerName, v); }
void Settings::setTitleTemplate (const juce::String& v)         { set (Keys::titleTemplate, v); }
void Settings::setDescriptionTemplate (const juce::String& v)   { set (Keys::descriptionTemplate, v); }
void Settings::setDefaultPrivacy (const juce::String& v)        { set (Keys::defaultPrivacy, v); }

void Settings::setGoogleClient (const juce::String& clientId, const juce::String& clientSecret)
{
    {
        const juce::ScopedLock sl (lock);
        properties->setValue (Keys::googleClientId, clientId);
        properties->setValue (Keys::googleClientSecret, clientSecret);
        properties->saveIfNeeded();
    }

    sendChangeMessage();
}

bool Settings::hasGoogleClient() const
{
    return getGoogleClientId().isNotEmpty() && getGoogleClientSecret().isNotEmpty();
}

juce::Result Settings::parseGoogleClientJson (const juce::String& json, juce::String& clientId, juce::String& clientSecret)
{
    juce::var parsed;
    const auto parseResult = juce::JSON::parse (json, parsed);

    if (parseResult.failed())
        return juce::Result::fail ("Fichier JSON invalide : " + parseResult.getErrorMessage());

    // Desktop clients are under "installed"; accept "web" too so a wrong choice gives a clear error later.
    auto client = parsed.getProperty ("installed", {});

    if (! client.isObject())
        client = parsed.getProperty ("web", {});

    if (! client.isObject())
        return juce::Result::fail ("Ce fichier ne ressemble pas à un client OAuth Google (clé \"installed\" absente)."_t);

    clientId = client.getProperty ("client_id", {}).toString();
    clientSecret = client.getProperty ("client_secret", {}).toString();

    if (clientId.isEmpty() || clientSecret.isEmpty())
        return juce::Result::fail ("client_id ou client_secret manquant dans le fichier.");

    return juce::Result::ok();
}

juce::Result Settings::importGoogleClientJson (const juce::File& jsonFile)
{
    juce::String clientId, clientSecret;
    const auto result = parseGoogleClientJson (jsonFile.loadFileAsString(), clientId, clientSecret);

    if (result.wasOk())
        setGoogleClient (clientId, clientSecret);

    return result;
}

} // namespace beatdrop
