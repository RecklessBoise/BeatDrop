#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace beatdrop
{

/**
    Global preferences shared by every BeatDrop instance
    (~/Library/Application Support/BeatDrop/BeatDrop.settings).
*/
class Settings : public juce::ChangeBroadcaster
{
public:
    Settings();
    explicit Settings (const juce::File& settingsFile);

    juce::String getProducerName() const;
    juce::String getTitleTemplate() const;
    juce::String getDescriptionTemplate() const;
    juce::String getDefaultPrivacy() const;
    juce::String getGoogleClientId() const;
    juce::String getGoogleClientSecret() const;

    void setProducerName (const juce::String&);
    void setTitleTemplate (const juce::String&);
    void setDescriptionTemplate (const juce::String&);
    void setDefaultPrivacy (const juce::String&);
    void setGoogleClient (const juce::String& clientId, const juce::String& clientSecret);

    bool hasGoogleClient() const;

    /** Reads the client_secret_*.json file downloaded from Google Cloud Console. */
    juce::Result importGoogleClientJson (const juce::File& jsonFile);

    static juce::Result parseGoogleClientJson (const juce::String& json, juce::String& clientId, juce::String& clientSecret);

    /** ~/Music/BeatDrop : takes, rendered videos. */
    static juce::File getOutputDirectory();
    static juce::File getSettingsDirectory();

private:
    juce::String get (const juce::String& key, const juce::String& fallback) const;
    void set (const juce::String& key, const juce::String& value);

    mutable juce::CriticalSection lock;
    std::unique_ptr<juce::PropertiesFile> properties;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Settings)
};

} // namespace beatdrop
