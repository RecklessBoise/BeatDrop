#include "CapturePanel.h"
#include "Core/Text.h"
#include "BeatDropLookAndFeel.h"

namespace beatdrop
{

namespace
{
    constexpr int modeRadioGroup = 4201;

    juce::String formatDuration (double seconds)
    {
        const auto total = (int) seconds;
        return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
    }
}

CapturePanel::CapturePanel (MasterRecorder& r)
    : recorder (r)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener (this);

    for (auto* button : { &playbackModeButton, &bounceModeButton })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (modeRadioGroup);
        addAndMakeVisible (button);
    }

    playbackModeButton.setTooltip ("Enregistre le master de Play à Stop. Lance la lecture depuis le début du beat."_t);
    bounceModeButton.setTooltip ("Enregistre pendant l'export de ton DAW (Fichier > Exporter), plus rapide que la lecture.");

    playbackModeButton.onClick = [this] { if (playbackModeButton.getToggleState()) recorder.setMode (CaptureMode::playback); refresh(); };
    bounceModeButton.onClick   = [this] { if (bounceModeButton.getToggleState())   recorder.setMode (CaptureMode::bounce);   refresh(); };

    armButton.setClickingTogglesState (true);
    armButton.onClick = [this] { recorder.setArmed (armButton.getToggleState()); refresh(); };
    addAndMakeVisible (armButton);

    stopButton.onClick = [this] { recorder.stopNow(); };
    addAndMakeVisible (stopButton);

    importButton.onClick = [this] { chooseAudioFile(); };
    addAndMakeVisible (importButton);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    takeLabel.setFont (juce::FontOptions (12.0f));
    takeLabel.setColour (juce::Label::textColourId, Palette::muted);
    takeLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (takeLabel);

    recorder.addListener (this);

    if (const auto take = recorder.getLastTake(); take.isValid())
        showTake (take);

    refresh();
    startTimerHz (30);
}

CapturePanel::~CapturePanel()
{
    recorder.removeListener (this);
    thumbnail.removeChangeListener (this);
}

void CapturePanel::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    BeatDropLookAndFeel::drawSectionTitle (g, { 16, 10, getWidth() - 32, 18 }, "Capture du master");

    // Level meter
    g.setColour (Palette::field);
    g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);

    const auto levelDb = juce::Decibels::gainToDecibels (displayedLevel, -60.0f);
    const auto proportion = juce::jmap (levelDb, -60.0f, 0.0f, 0.0f, 1.0f);
    g.setColour (displayedLevel >= 0.99f ? Palette::accent : Palette::success);
    g.fillRoundedRectangle (meterArea.toFloat().withWidth ((float) meterArea.getWidth() * proportion), 3.0f);

    // Waveform of the last take
    g.setColour (Palette::field);
    g.fillRoundedRectangle (waveformArea.toFloat(), 6.0f);

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour (Palette::accent.withAlpha (0.85f));
        thumbnail.drawChannels (g, waveformArea.reduced (6, 4), 0.0, thumbnail.getTotalLength(), 1.0f);
    }
    else
    {
        g.setColour (Palette::muted);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Aucune prise pour l'instant", waveformArea, juce::Justification::centred);
    }
}

void CapturePanel::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (24);

    auto modeRow = area.removeFromTop (30);
    playbackModeButton.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2).withTrimmedRight (4));
    bounceModeButton.setBounds (modeRow.withTrimmedLeft (4));

    area.removeFromTop (10);
    auto armRow = area.removeFromTop (44);
    stopButton.setBounds (armRow.removeFromRight (72));
    armRow.removeFromRight (8);
    armButton.setBounds (armRow);

    area.removeFromTop (8);
    statusLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);
    meterArea = area.removeFromTop (6);

    area.removeFromTop (12);
    auto bottom = area.removeFromBottom (28);
    importButton.setBounds (bottom.removeFromRight (150));
    takeLabel.setBounds (bottom);

    area.removeFromBottom (6);
    waveformArea = area;
}

void CapturePanel::recorderStateChanged()
{
    refresh();
}

void CapturePanel::takeFinished (const Take& take)
{
    showTake (take);
    refresh();
}

void CapturePanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void CapturePanel::timerCallback()
{
    const auto level = recorder.getPeakLevel();
    displayedLevel = level > displayedLevel ? level : displayedLevel * 0.85f;
    repaint (meterArea);

    if (recorder.getState() == MasterRecorder::State::recording)
        refresh();
}

void CapturePanel::refresh()
{
    const auto mode = recorder.getMode();
    playbackModeButton.setToggleState (mode == CaptureMode::playback, juce::dontSendNotification);
    bounceModeButton.setToggleState (mode == CaptureMode::bounce, juce::dontSendNotification);

    const auto state = recorder.getState();
    armButton.setToggleState (recorder.isArmed(), juce::dontSendNotification);
    armButton.setButtonText (recorder.isArmed() ? juce::String ("ARMÉ"_t) : juce::String ("ARMER"));
    stopButton.setEnabled (state == MasterRecorder::State::recording);

    juce::String status;
    auto colour = Palette::muted;

    switch (state)
    {
        case MasterRecorder::State::idle:
            status = "Clique sur ARMER pour capturer ton beat.";
            break;

        case MasterRecorder::State::armed:
            status = mode == CaptureMode::playback ? juce::String ("Prêt : lance la lecture depuis le début."_t)
                                                   : juce::String ("Prêt : lance l'export audio de ton DAW."_t);
            colour = Palette::warning;
            break;

        case MasterRecorder::State::recording:
            status = "Enregistrement... " + formatDuration (recorder.getRecordedSeconds());
            colour = Palette::accent;
            break;

        case MasterRecorder::State::finalising:
            status = "Finalisation de la prise...";
            colour = Palette::warning;
            break;
    }

    statusLabel.setText (status, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, colour);
}

void CapturePanel::showTake (const Take& take)
{
    thumbnail.setSource (new juce::FileInputSource (take.file));
    takeLabel.setText (take.file.getFileNameWithoutExtension() + "  -  " + formatDuration (take.getLengthSeconds()),
                       juce::dontSendNotification);
    repaint();
}

void CapturePanel::chooseAudioFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choisis un fichier audio",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       formatManager.getWildcardForAllFormats());

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<CapturePanel> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr || chooser.getResult() == juce::File())
            return;

        const auto result = safeThis->recorder.loadTakeFromFile (chooser.getResult());

        if (result.failed())
            juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon,
                                                                                  "BeatDrop", result.getErrorMessage()),
                                          nullptr);
    });
}

} // namespace beatdrop
