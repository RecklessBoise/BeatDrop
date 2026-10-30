#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Core/MasterRecorder.h"

namespace beatdrop
{

/** Capture mode, arm button, level meter and the current take's waveform. */
class CapturePanel : public juce::Component,
                     private MasterRecorder::Listener,
                     private juce::ChangeListener,
                     private juce::Timer
{
public:
    explicit CapturePanel (MasterRecorder&);
    ~CapturePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void recorderStateChanged() override;
    void takeFinished (const Take&) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void refresh();
    void showTake (const Take&);
    void chooseAudioFile();

    MasterRecorder& recorder;

    juce::TextButton playbackModeButton { "Lecture" };
    juce::TextButton bounceModeButton { "Export DAW" };
    juce::TextButton armButton { "ARMER" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton importButton { "Importer un audio..." };
    juce::Label statusLabel, takeLabel;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 4 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Rectangle<int> meterArea, waveformArea;
    float displayedLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapturePanel)
};

} // namespace beatdrop
