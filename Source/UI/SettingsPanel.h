#pragma once

#include "BeatDropLookAndFeel.h"
#include "Core/Text.h"
#include "Core/Settings.h"

namespace beatdrop
{

/** Overlay with the global preferences: producer name, templates, Google client. */
class SettingsPanel : public juce::Component,
                      private juce::ChangeListener
{
public:
    explicit SettingsPanel (Settings&);
    ~SettingsPanel() override;

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    static inline const juce::String setupGuideUrl {
        "https://github.com/RecklessBoise/BeatDrop/blob/main/docs/GOOGLE_SETUP.md" };

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    void importGoogleClient();
    juce::Rectangle<int> getCardBounds() const;

    Settings& settings;

    FieldLabel producerLabel            { "Ton nom de producteur ({producer})" };
    FieldLabel titleTemplateLabel       { "Modèle de titre"_t };
    FieldLabel descriptionTemplateLabel { "Modèle de description"_t };
    FieldLabel privacyLabel             { "Visibilité par défaut"_t };
    FieldLabel placeholdersLabel        { "Variables : {name} {artist} {artisttag} {bpm} {key} {producer} {year}" };
    FieldLabel googleLabel              { "Connexion Google (API YouTube Data v3)" };

    juce::TextEditor producerEditor, titleTemplateEditor, descriptionTemplateEditor;
    juce::ComboBox privacyBox;
    juce::Label googleStatus;

    juce::TextButton importGoogleButton { "Importer client_secret.json..." };
    juce::TextButton guideButton { "Guide d'installation" };
    juce::TextButton resetTemplatesButton { "Modèles par défaut"_t };
    juce::TextButton openFolderButton { "Ouvrir le dossier BeatDrop" };
    juce::TextButton closeButton { "Fermer" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    bool updatingFromCode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};

} // namespace beatdrop
