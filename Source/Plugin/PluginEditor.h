#pragma once

#include "PluginProcessor.h"
#include "Core/Text.h"
#include "UI/BeatDropLookAndFeel.h"
#include "UI/CapturePanel.h"
#include "UI/ImageDropZone.h"
#include "UI/MetadataPanel.h"
#include "UI/SettingsPanel.h"

class BeatDropEditor : public juce::AudioProcessorEditor,
                       private juce::ChangeListener
{
public:
    explicit BeatDropEditor (BeatDropProcessor&);
    ~BeatDropEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateAuthUi();
    void onConnectClicked();
    void showSettings (bool shouldShow);

    beatdrop::BeatDropLookAndFeel lookAndFeel; // must outlive the child components
    BeatDropProcessor& beatDrop;

    juce::Label channelLabel;
    juce::TextButton connectButton;
    juce::TextButton settingsButton { "Réglages"_t };

    beatdrop::CapturePanel capturePanel;
    beatdrop::ImageDropZone imageZone;
    beatdrop::MetadataPanel metadataPanel;
    beatdrop::SettingsPanel settingsPanel;

    juce::TooltipWindow tooltips { this, 600 };
    juce::String lastShownError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatDropEditor)
};
