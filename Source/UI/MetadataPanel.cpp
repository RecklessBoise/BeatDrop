#include "MetadataPanel.h"
#include "Core/Text.h"
#include "Plugin/PluginProcessor.h"

namespace beatdrop
{

namespace
{
    enum PrivacyId
    {
        publicId = 1,
        unlistedId,
        privateId,
        scheduledId
    };

    int privacyToId (const juce::String& privacy)
    {
        if (privacy == Privacy::unlisted)     return unlistedId;
        if (privacy == Privacy::privateVideo) return privateId;
        if (privacy == "scheduled")           return scheduledId;
        return publicId;
    }

    juce::String idToPrivacy (int id)
    {
        switch (id)
        {
            case unlistedId:  return Privacy::unlisted;
            case privateId:   return Privacy::privateVideo;
            case scheduledId: return "scheduled";
            default:          return Privacy::publicVideo;
        }
    }
}

MetadataPanel::MetadataPanel (BeatDropProcessor& p)
    : processor (p)
{
    for (auto* label : { &nameLabel, &artistLabel, &bpmLabel, &keyLabel, &privacyLabel, &publishAtLabel,
                         &titleLabel, &descriptionLabel, &tagsLabel, &titleCounter })
        addAndMakeVisible (label);

    titleCounter.setJustificationType (juce::Justification::centredRight);

    setupEditor (nameEditor, StateIds::beatName);
    setupEditor (artistEditor, StateIds::artist);
    setupEditor (bpmEditor, StateIds::bpm);
    setupEditor (keyEditor, StateIds::key);
    setupEditor (publishAtEditor, StateIds::publishAt);
    setupEditor (titleEditor, StateIds::title);
    setupEditor (descriptionEditor, StateIds::description, true);
    setupEditor (tagsEditor, StateIds::tags);

    nameEditor.setTextToShowWhenEmpty ("Midnight", Palette::muted);
    artistEditor.setTextToShowWhenEmpty ("Drake", Palette::muted);
    bpmEditor.setTextToShowWhenEmpty ("140", Palette::muted);
    bpmEditor.setInputRestrictions (6, "0123456789.");
    keyEditor.setTextToShowWhenEmpty ("C# min", Palette::muted);
    publishAtEditor.setTextToShowWhenEmpty (juce::Time::getCurrentTime().formatted ("%Y-%m-%d 18:00"), Palette::muted);
    tagsEditor.setTextToShowWhenEmpty ("drake type beat, type beat 2026", Palette::muted);

    resetTitleButton.setTooltip ("Régénère le titre depuis ton modèle (Réglages)."_t);
    resetDescriptionButton.setTooltip ("Régénère la description depuis ton modèle (Réglages)."_t);

    resetTitleButton.onClick = [this]
    {
        processor.getState().setProperty (StateIds::titleEdited, false, nullptr);
        regenerate (false);
    };

    resetDescriptionButton.onClick = [this]
    {
        processor.getState().setProperty (StateIds::descriptionEdited, false, nullptr);
        regenerate (false);
    };

    suggestTagsButton.onClick = [this]
    {
        processor.getState().setProperty (StateIds::tagsEdited, false, nullptr);
        regenerate (false);
    };

    for (auto* button : { &resetTitleButton, &resetDescriptionButton, &suggestTagsButton })
        addAndMakeVisible (button);

    privacyBox.addItem ("Public", publicId);
    privacyBox.addItem ("Non répertorié"_t, unlistedId);
    privacyBox.addItem ("Privé"_t, privateId);
    privacyBox.addItem ("Programmé"_t, scheduledId);
    privacyBox.onChange = [this]
    {
        if (! updatingFromCode)
            processor.getState().setProperty (StateIds::privacy, idToPrivacy (privacyBox.getSelectedId()), nullptr);

        updatePrivacyUi();
    };
    addAndMakeVisible (privacyBox);

    uploadButton.setColour (juce::TextButton::buttonColourId, Palette::accent);
    uploadButton.onClick = [this] { startUpload(); };
    addAndMakeVisible (uploadButton);

    cancelButton.onClick = [this] { processor.getUploads().cancel(); };
    addChildComponent (cancelButton);

    addChildComponent (progressBar);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (statusLabel);

    videoLink.setFont (juce::FontOptions (13.0f, juce::Font::bold), false, juce::Justification::centredLeft);
    addChildComponent (videoLink);

    revealButton.onClick = [this]
    {
        const auto file = processor.getUploads().getStatus().videoFile;

        if (file.existsAsFile())
            file.revealToUser();
    };
    addChildComponent (revealButton);

    processor.getUploads().addChangeListener (this);
    processor.getServices().settings.addChangeListener (this);

    loadFromState();
    updateUploadUi();
    startTimerHz (2);
}

MetadataPanel::~MetadataPanel()
{
    processor.getUploads().removeChangeListener (this);
    processor.getServices().settings.removeChangeListener (this);
}

void MetadataPanel::setupEditor (juce::TextEditor& editor, const juce::Identifier& property, bool multiLine)
{
    editor.setMultiLine (multiLine, true);
    editor.setReturnKeyStartsNewLine (multiLine);
    editor.setScrollbarsShown (multiLine);
    editor.setFont (juce::FontOptions (14.0f));
    editor.setIndents (8, multiLine ? 6 : 7);

    editor.onTextChange = [this, &editor, property]
    {
        if (updatingFromCode)
            return;

        auto& state = processor.getState();
        state.setProperty (property, editor.getText(), nullptr);

        if (property == StateIds::title)
        {
            state.setProperty (StateIds::titleEdited, true, nullptr);
            updateTitleCounter();
        }
        else if (property == StateIds::description)
        {
            state.setProperty (StateIds::descriptionEdited, true, nullptr);
        }
        else if (property == StateIds::tags)
        {
            state.setProperty (StateIds::tagsEdited, true, nullptr);
        }
        else if (property != StateIds::publishAt)
        {
            regenerate (false);
        }
    };

    addAndMakeVisible (editor);
}

void MetadataPanel::loadFromState()
{
    const auto& state = processor.getState();
    const juce::ScopedValueSetter<bool> svs (updatingFromCode, true);

    nameEditor.setText (state[StateIds::beatName].toString(), false);
    artistEditor.setText (state[StateIds::artist].toString(), false);
    bpmEditor.setText (state[StateIds::bpm].toString(), false);
    keyEditor.setText (state[StateIds::key].toString(), false);
    publishAtEditor.setText (state[StateIds::publishAt].toString(), false);
    titleEditor.setText (state[StateIds::title].toString(), false);
    descriptionEditor.setText (state[StateIds::description].toString(), false);
    tagsEditor.setText (state[StateIds::tags].toString(), false);

    const auto privacy = state.getProperty (StateIds::privacy, processor.getServices().settings.getDefaultPrivacy()).toString();
    privacyBox.setSelectedId (privacyToId (privacy), juce::dontSendNotification);

    updatePrivacyUi();
    updateTitleCounter();

    if (titleEditor.isEmpty())
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MetadataPanel> (this)]
        {
            if (safeThis != nullptr)
                safeThis->regenerate (false);
        });
}

BeatInfo MetadataPanel::getBeatInfo() const
{
    BeatInfo info;
    info.beatName = nameEditor.getText();
    info.artist = artistEditor.getText();
    info.bpm = bpmEditor.getText();
    info.key = keyEditor.getText();
    info.producer = processor.getServices().settings.getProducerName();
    return info;
}

void MetadataPanel::regenerate (bool force)
{
    auto& state = processor.getState();
    const auto& settings = processor.getServices().settings;
    const auto info = getBeatInfo();

    const juce::ScopedValueSetter<bool> svs (updatingFromCode, true);

    if (force || ! (bool) state[StateIds::titleEdited])
    {
        const auto title = Metadata::sanitiseTitle (Metadata::renderTemplate (settings.getTitleTemplate(), info));
        titleEditor.setText (title, false);
        state.setProperty (StateIds::title, title, nullptr);
    }

    if (force || ! (bool) state[StateIds::descriptionEdited])
    {
        const auto description = Metadata::sanitiseDescription (Metadata::renderTemplate (settings.getDescriptionTemplate(), info));
        descriptionEditor.setText (description, false);
        state.setProperty (StateIds::description, description, nullptr);
    }

    if (force || ! (bool) state[StateIds::tagsEdited])
    {
        const auto tags = Metadata::sanitiseTags (Metadata::suggestTags (info)).joinIntoString (", ");
        tagsEditor.setText (tags, false);
        state.setProperty (StateIds::tags, tags, nullptr);
    }

    updateTitleCounter();
}

void MetadataPanel::updateTitleCounter()
{
    const auto length = titleEditor.getText().length();
    titleCounter.setText (juce::String (length) + "/" + juce::String (Metadata::maxTitleLength), juce::dontSendNotification);
    titleCounter.setColour (juce::Label::textColourId, length > Metadata::maxTitleLength ? Palette::accent : Palette::muted);
}

void MetadataPanel::updatePrivacyUi()
{
    const auto scheduled = privacyBox.getSelectedId() == scheduledId;
    publishAtLabel.setVisible (scheduled);
    publishAtEditor.setVisible (scheduled);
}

void MetadataPanel::showStatus (const juce::String& message, juce::Colour colour)
{
    statusLabel.setText (message, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, colour);
}

void MetadataPanel::startUpload()
{
    if (processor.getServices().auth.getState() != AuthController::State::signedIn)
    {
        showStatus ("Connecte d'abord ta chaîne YouTube (bouton en haut à droite)."_t, Palette::accent);
        return;
    }

    const auto recorderState = processor.getRecorder().getState();

    if (recorderState == MasterRecorder::State::recording || recorderState == MasterRecorder::State::finalising)
    {
        showStatus ("Attends la fin de l'enregistrement avant de publier.", Palette::warning);
        return;
    }

    UploadRequest request;
    request.take = processor.getRecorder().getLastTake();
    request.coverImage = juce::File (processor.getState()[StateIds::imagePath].toString());
    request.videoOutputDirectory = Settings::getOutputDirectory().getChildFile ("Videos");

    auto& metadata = request.metadata;
    metadata.title = Metadata::sanitiseTitle (titleEditor.getText());
    metadata.description = Metadata::sanitiseDescription (descriptionEditor.getText());
    metadata.tags = Metadata::sanitiseTags (Metadata::parseTagList (tagsEditor.getText()));

    const auto privacy = idToPrivacy (privacyBox.getSelectedId());

    if (privacy == "scheduled")
    {
        metadata.privacyStatus = Privacy::privateVideo;
        metadata.publishAt = Metadata::localDateTimeToRfc3339 (publishAtEditor.getText());

        if (metadata.publishAt.isEmpty())
        {
            showStatus ("Date de programmation invalide (format AAAA-MM-JJ HH:MM).", Palette::accent);
            return;
        }
    }
    else
    {
        metadata.privacyStatus = privacy;
    }

    const auto result = processor.getUploads().start (request);

    if (result.failed())
        showStatus (result.getErrorMessage(), Palette::accent);

    updateUploadUi();
}

void MetadataPanel::updateUploadUi()
{
    const auto status = processor.getUploads().getStatus();
    const auto busy = status.isBusy();

    uploadButton.setEnabled (! busy);
    cancelButton.setVisible (busy);
    progressBar.setVisible (busy);

    progressValue = status.progress;

    if (busy)
    {
        const auto stage = status.stage == UploadStatus::Stage::rendering ? juce::String ("Rendu vidéo"_t)
                         : status.stage == UploadStatus::Stage::uploading ? juce::String ("Envoi sur YouTube")
                                                                          : juce::String ("Miniature");
        progressBar.setTextToDisplay (stage + "  " + juce::String (juce::roundToInt (status.progress * 100.0)) + " %");
        showStatus (status.message, Palette::muted);
    }
    else if (status.stage == UploadStatus::Stage::done)
    {
        showStatus (status.message, Palette::success);
    }
    else if (status.stage == UploadStatus::Stage::failed)
    {
        showStatus (status.message, Palette::accent);
    }

    const auto url = status.getVideoUrl();
    videoLink.setVisible (status.stage == UploadStatus::Stage::done && url.isNotEmpty());
    videoLink.setButtonText (url);
    videoLink.setURL (juce::URL (url));

    revealButton.setVisible (! busy && status.videoFile.existsAsFile());
    resized();
}

void MetadataPanel::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor.getUploads())
        updateUploadUi();
    else
        regenerate (false); // templates or producer name changed
}

void MetadataPanel::timerCallback()
{
    // Fill the BPM from the host tempo until the user types their own.
    if (bpmEditor.isEmpty() && ! bpmEditor.hasKeyboardFocus (false))
    {
        const auto hostBpm = processor.getHostBpm();

        if (hostBpm > 0.0)
        {
            bpmEditor.setText (juce::String (juce::roundToInt (hostBpm)), false);
            processor.getState().setProperty (StateIds::bpm, bpmEditor.getText(), nullptr);
            regenerate (false);
        }
    }
}

void MetadataPanel::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    BeatDropLookAndFeel::drawSectionTitle (g, { 16, 10, getWidth() - 32, 18 }, "Publication YouTube");
}

void MetadataPanel::resized()
{
    constexpr int labelHeight = 18, fieldHeight = 30, gap = 10, columnGap = 12;

    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (26);

    const auto placeField = [&] (juce::Rectangle<int> column, juce::Label& label, juce::Component& field)
    {
        label.setBounds (column.removeFromTop (labelHeight));
        field.setBounds (column.removeFromTop (fieldHeight));
    };

    // Row 1: beat name | artist
    {
        auto row = area.removeFromTop (labelHeight + fieldHeight);
        const auto half = (row.getWidth() - columnGap) / 2;
        placeField (row.removeFromLeft (half), nameLabel, nameEditor);
        row.removeFromLeft (columnGap);
        placeField (row, artistLabel, artistEditor);
    }

    area.removeFromTop (gap);

    // Row 2: bpm | key | visibility | date
    {
        auto row = area.removeFromTop (labelHeight + fieldHeight);
        placeField (row.removeFromLeft (80), bpmLabel, bpmEditor);
        row.removeFromLeft (columnGap);
        placeField (row.removeFromLeft (100), keyLabel, keyEditor);
        row.removeFromLeft (columnGap);
        placeField (row.removeFromLeft (150), privacyLabel, privacyBox);
        row.removeFromLeft (columnGap);
        placeField (row, publishAtLabel, publishAtEditor);
    }

    area.removeFromTop (gap);

    // Bottom: publish button, progress, status
    {
        auto bottom = area.removeFromBottom (44 + 8 + 16 + 6 + 22);

        auto buttons = bottom.removeFromTop (44);

        if (cancelButton.isVisible())
        {
            cancelButton.setBounds (buttons.removeFromRight (100));
            buttons.removeFromRight (8);
        }

        uploadButton.setBounds (buttons);

        bottom.removeFromTop (8);
        progressBar.setBounds (bottom.removeFromTop (16));
        bottom.removeFromTop (6);

        auto statusRow = bottom.removeFromTop (22);

        if (revealButton.isVisible())
        {
            revealButton.setBounds (statusRow.removeFromRight (100));
            statusRow.removeFromRight (8);
        }

        if (videoLink.isVisible())
            videoLink.setBounds (statusRow.removeFromRight (200));

        statusLabel.setBounds (statusRow);

        area.removeFromBottom (gap);
    }

    // Title
    {
        auto labelRow = area.removeFromTop (24);
        resetTitleButton.setBounds (labelRow.removeFromRight (70).reduced (0, 1));
        titleCounter.setBounds (labelRow.removeFromRight (60));
        titleLabel.setBounds (labelRow);
        titleEditor.setBounds (area.removeFromTop (fieldHeight));
    }

    area.removeFromTop (gap);

    // Tags at the bottom of what's left, description takes the rest
    {
        auto tagsArea = area.removeFromBottom (24 + fieldHeight);
        auto labelRow = tagsArea.removeFromTop (24);
        suggestTagsButton.setBounds (labelRow.removeFromRight (80).reduced (0, 1));
        tagsLabel.setBounds (labelRow);
        tagsEditor.setBounds (tagsArea);
        area.removeFromBottom (gap);
    }

    {
        auto labelRow = area.removeFromTop (24);
        resetDescriptionButton.setBounds (labelRow.removeFromRight (70).reduced (0, 1));
        descriptionLabel.setBounds (labelRow);
        descriptionEditor.setBounds (area);
    }
}

} // namespace beatdrop
