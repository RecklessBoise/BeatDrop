#include "SettingsPanel.h"
#include "Core/Text.h"
#include "Core/Metadata.h"

namespace beatdrop
{

SettingsPanel::SettingsPanel (Settings& s)
    : settings (s)
{
    for (auto* label : { &producerLabel, &titleTemplateLabel, &descriptionTemplateLabel, &privacyLabel,
                         &placeholdersLabel, &googleLabel })
        addAndMakeVisible (label);

    for (auto* editor : { &producerEditor, &titleTemplateEditor, &descriptionTemplateEditor })
    {
        editor->setFont (juce::FontOptions (14.0f));
        editor->setIndents (8, 7);
        addAndMakeVisible (editor);
    }

    descriptionTemplateEditor.setMultiLine (true, true);
    descriptionTemplateEditor.setReturnKeyStartsNewLine (true);
    descriptionTemplateEditor.setScrollbarsShown (true);

    producerEditor.onTextChange = [this] { if (! updatingFromCode) settings.setProducerName (producerEditor.getText()); };
    titleTemplateEditor.onTextChange = [this] { if (! updatingFromCode) settings.setTitleTemplate (titleTemplateEditor.getText()); };
    descriptionTemplateEditor.onTextChange = [this] { if (! updatingFromCode) settings.setDescriptionTemplate (descriptionTemplateEditor.getText()); };

    privacyBox.addItem ("Public", 1);
    privacyBox.addItem ("Non répertorié"_t, 2);
    privacyBox.addItem ("Privé"_t, 3);
    privacyBox.onChange = [this]
    {
        if (updatingFromCode)
            return;

        const auto id = privacyBox.getSelectedId();
        settings.setDefaultPrivacy (id == 2 ? Privacy::unlisted : id == 3 ? Privacy::privateVideo : Privacy::publicVideo);
    };
    addAndMakeVisible (privacyBox);

    googleStatus.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (googleStatus);

    importGoogleButton.onClick = [this] { importGoogleClient(); };
    guideButton.onClick = [] { juce::URL (setupGuideUrl).launchInDefaultBrowser(); };

    resetTemplatesButton.onClick = [this]
    {
        settings.setTitleTemplate (Metadata::defaultTitleTemplate);
        settings.setDescriptionTemplate (Metadata::defaultDescriptionTemplate);
    };

    openFolderButton.onClick = []
    {
        const auto folder = Settings::getOutputDirectory();
        folder.createDirectory();
        folder.startAsProcess();
    };

    closeButton.setColour (juce::TextButton::buttonColourId, Palette::accent);
    closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };

    for (auto* button : { &importGoogleButton, &guideButton, &resetTemplatesButton, &openFolderButton, &closeButton })
        addAndMakeVisible (button);

    settings.addChangeListener (this);
    refresh();
}

SettingsPanel::~SettingsPanel()
{
    settings.removeChangeListener (this);
}

void SettingsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void SettingsPanel::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updatingFromCode, true);

    // Don't fight the user's cursor while they type.
    if (! producerEditor.hasKeyboardFocus (false))
        producerEditor.setText (settings.getProducerName(), false);

    if (! titleTemplateEditor.hasKeyboardFocus (false))
        titleTemplateEditor.setText (settings.getTitleTemplate(), false);

    if (! descriptionTemplateEditor.hasKeyboardFocus (false))
        descriptionTemplateEditor.setText (settings.getDescriptionTemplate(), false);

    const auto privacy = settings.getDefaultPrivacy();
    privacyBox.setSelectedId (privacy == Privacy::unlisted ? 2 : privacy == Privacy::privateVideo ? 3 : 1, juce::dontSendNotification);

    if (settings.hasGoogleClient())
    {
        googleStatus.setText ("Client Google importé ("_t + settings.getGoogleClientId().substring (0, 12) + "...)", juce::dontSendNotification);
        googleStatus.setColour (juce::Label::textColourId, Palette::success);
    }
    else
    {
        googleStatus.setText ("Aucun client Google : suis le guide puis importe ton fichier JSON.", juce::dontSendNotification);
        googleStatus.setColour (juce::Label::textColourId, Palette::warning);
    }
}

void SettingsPanel::importGoogleClient()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choisis le fichier client_secret_....json",
                                                       juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads"),
                                                       "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<SettingsPanel> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr || ! chooser.getResult().existsAsFile())
            return;

        const auto result = safeThis->settings.importGoogleClientJson (chooser.getResult());

        if (result.failed())
        {
            safeThis->googleStatus.setText (result.getErrorMessage(), juce::dontSendNotification);
            safeThis->googleStatus.setColour (juce::Label::textColourId, Palette::accent);
        }
    });
}

juce::Rectangle<int> SettingsPanel::getCardBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (juce::jmin (680, getWidth() - 40), juce::jmin (580, getHeight() - 40));
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));

    const auto card = getCardBounds();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (card.toFloat(), 12.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 12.0f, 1.0f);

    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("Réglages"_t, card.reduced (20).removeFromTop (26), juce::Justification::centredLeft);
}

void SettingsPanel::resized()
{
    constexpr int labelHeight = 18, fieldHeight = 30, gap = 12;

    auto area = getCardBounds().reduced (20);
    area.removeFromTop (36);

    const auto place = [&] (juce::Label& label, juce::Component& field, int height)
    {
        label.setBounds (area.removeFromTop (labelHeight));
        field.setBounds (area.removeFromTop (height));
        area.removeFromTop (gap);
    };

    {
        auto row = area.removeFromTop (labelHeight + fieldHeight);
        auto left = row.removeFromLeft (row.getWidth() / 2 - 6);
        producerLabel.setBounds (left.removeFromTop (labelHeight));
        producerEditor.setBounds (left);
        row.removeFromLeft (12);
        privacyLabel.setBounds (row.removeFromTop (labelHeight));
        privacyBox.setBounds (row);
        area.removeFromTop (gap);
    }

    place (titleTemplateLabel, titleTemplateEditor, fieldHeight);
    place (descriptionTemplateLabel, descriptionTemplateEditor, 110);

    {
        auto row = area.removeFromTop (28);
        resetTemplatesButton.setBounds (row.removeFromRight (160));
        placeholdersLabel.setBounds (row);
        area.removeFromTop (gap + 4);
    }

    googleLabel.setBounds (area.removeFromTop (labelHeight));
    googleStatus.setBounds (area.removeFromTop (22));
    area.removeFromTop (6);

    {
        auto row = area.removeFromTop (32);
        importGoogleButton.setBounds (row.removeFromLeft (240));
        row.removeFromLeft (8);
        guideButton.setBounds (row.removeFromLeft (180));
    }

    auto bottom = area.removeFromBottom (34);
    closeButton.setBounds (bottom.removeFromRight (120));
    openFolderButton.setBounds (bottom.removeFromLeft (220));
}

void SettingsPanel::mouseUp (const juce::MouseEvent& e)
{
    // Clicking the dimmed backdrop closes the panel.
    if (! getCardBounds().contains (e.getPosition()) && onClose != nullptr)
        onClose();
}

} // namespace beatdrop
