#pragma once

#include "BeatDropLookAndFeel.h"
#include "Core/Text.h"
#include "Core/Metadata.h"

class BeatDropProcessor;

namespace beatdrop
{

/** Beat info, YouTube title/description/tags, visibility and the publish button. */
class MetadataPanel : public juce::Component,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit MetadataPanel (BeatDropProcessor&);
    ~MetadataPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void setupEditor (juce::TextEditor& editor, const juce::Identifier& property, bool multiLine = false);
    void loadFromState();
    void regenerate (bool force);
    BeatInfo getBeatInfo() const;

    void startUpload();
    void updateUploadUi();
    void updateTitleCounter();
    void updatePrivacyUi();
    void showStatus (const juce::String& message, juce::Colour colour);

    BeatDropProcessor& processor;

    FieldLabel nameLabel        { "Nom du beat" };
    FieldLabel artistLabel      { "Artiste (type beat)" };
    FieldLabel bpmLabel         { "BPM" };
    FieldLabel keyLabel         { "Tonalité"_t };
    FieldLabel privacyLabel     { "Visibilité"_t };
    FieldLabel publishAtLabel   { "Date (AAAA-MM-JJ HH:MM)" };
    FieldLabel titleLabel       { "Titre YouTube" };
    FieldLabel descriptionLabel { "Description" };
    FieldLabel tagsLabel        { "Tags (séparés par des virgules)"_t };
    FieldLabel titleCounter     { "0/100" };

    juce::TextEditor nameEditor, artistEditor, bpmEditor, keyEditor, publishAtEditor,
                     titleEditor, descriptionEditor, tagsEditor;

    juce::TextButton resetTitleButton { "Modèle"_t };
    juce::TextButton resetDescriptionButton { "Modèle"_t };
    juce::TextButton suggestTagsButton { "Suggérer"_t };
    juce::ComboBox privacyBox;

    juce::TextButton uploadButton { "Publier sur YouTube" };
    juce::TextButton cancelButton { "Annuler" };
    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };
    juce::Label statusLabel;
    juce::HyperlinkButton videoLink;
    juce::TextButton revealButton { "Voir le MP4" };

    bool updatingFromCode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetadataPanel)
};

} // namespace beatdrop
