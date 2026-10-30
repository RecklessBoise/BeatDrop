#include "PluginEditor.h"
#include "Core/Text.h"

using namespace beatdrop;

BeatDropEditor::BeatDropEditor (BeatDropProcessor& p)
    : AudioProcessorEditor (p),
      beatDrop (p),
      capturePanel (p.getRecorder()),
      metadataPanel (p),
      settingsPanel (p.getServices().settings)
{
    setLookAndFeel (&lookAndFeel);

    channelLabel.setFont (juce::FontOptions (13.0f));
    channelLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (channelLabel);

    connectButton.onClick = [this] { onConnectClicked(); };
    addAndMakeVisible (connectButton);

    settingsButton.onClick = [this] { showSettings (true); };
    addAndMakeVisible (settingsButton);

    addAndMakeVisible (capturePanel);
    addAndMakeVisible (imageZone);
    addAndMakeVisible (metadataPanel);

    imageZone.setImageFile (juce::File (beatDrop.getState()[StateIds::imagePath].toString()));
    imageZone.onImageChanged = [this] (const juce::File& file)
    {
        beatDrop.getState().setProperty (StateIds::imagePath, file.getFullPathName(), nullptr);
    };

    settingsPanel.onClose = [this] { showSettings (false); };
    addChildComponent (settingsPanel);

    auto& auth = beatDrop.getServices().auth;
    auth.addChangeListener (this);
    auth.loadChannelIfNeeded();
    updateAuthUi();

    setSize (1000, 660);
}

BeatDropEditor::~BeatDropEditor()
{
    beatDrop.getServices().auth.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void BeatDropEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto header = getLocalBounds().removeFromTop (60).reduced (20, 0);

    g.setColour (Palette::accent);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) header.getX(), 21.0f, 18.0f, 18.0f), 4.0f);

    juce::Path play;
    play.addTriangle ((float) header.getX() + 6.0f, 25.0f, (float) header.getX() + 6.0f, 35.0f, (float) header.getX() + 14.0f, 30.0f);
    g.setColour (juce::Colours::white);
    g.fillPath (play);

    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("BeatDrop", header.withTrimmedLeft (28), juce::Justification::centredLeft);

    g.setColour (Palette::muted);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" BEATDROP_VERSION, header.withTrimmedLeft (130), juce::Justification::centredLeft);
}

void BeatDropEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (60).reduced (20, 14);
    settingsButton.setBounds (header.removeFromRight (100));
    header.removeFromRight (8);
    connectButton.setBounds (header.removeFromRight (170));
    header.removeFromRight (12);
    channelLabel.setBounds (header.removeFromRight (320));

    area = area.reduced (20, 0).withTrimmedBottom (20);

    auto left = area.removeFromLeft (380);
    capturePanel.setBounds (left.removeFromTop (300));
    left.removeFromTop (16);
    imageZone.setBounds (left);

    area.removeFromLeft (16);
    metadataPanel.setBounds (area);

    settingsPanel.setBounds (getLocalBounds());
}

void BeatDropEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateAuthUi();
}

void BeatDropEditor::updateAuthUi()
{
    auto& auth = beatDrop.getServices().auth;

    switch (auth.getState())
    {
        case AuthController::State::signedOut:
            channelLabel.setText ("Chaîne YouTube non connectée"_t, juce::dontSendNotification);
            channelLabel.setColour (juce::Label::textColourId, Palette::muted);
            connectButton.setButtonText ("Connecter YouTube");
            break;

        case AuthController::State::signingIn:
            channelLabel.setText ("Valide l'accès dans ton navigateur..."_t, juce::dontSendNotification);
            channelLabel.setColour (juce::Label::textColourId, Palette::warning);
            connectButton.setButtonText ("Annuler");
            break;

        case AuthController::State::signedIn:
        {
            const auto title = auth.getChannelTitle();
            channelLabel.setText (title.isNotEmpty() ? juce::String::fromUTF8 ("\xe2\x97\x8f ") + title
                                                     : juce::String ("Chaîne connectée"_t),
                                  juce::dontSendNotification);
            channelLabel.setColour (juce::Label::textColourId, Palette::success);
            connectButton.setButtonText ("Déconnecter"_t);
            break;
        }
    }

    const auto error = auth.getLastError();
    channelLabel.setTooltip (error);

    if (error.isNotEmpty() && error != lastShownError)
    {
        lastShownError = error;
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon,
                                                                              "Connexion YouTube", error),
                                      nullptr);
    }
}

void BeatDropEditor::onConnectClicked()
{
    auto& services = beatDrop.getServices();

    switch (services.auth.getState())
    {
        case AuthController::State::signedOut:
            if (! services.settings.hasGoogleClient())
            {
                showSettings (true);
                juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (
                                                  juce::MessageBoxIconType::InfoIcon, "Configurer Google",
                                                  "Avant de connecter ta chaîne, importe ton fichier client_secret.json "_t
                                                  "(bouton \"Guide d'installation\" pour le créer en 5 minutes)."_t),
                                              nullptr);
                return;
            }

            lastShownError.clear();
            services.auth.signIn();
            break;

        case AuthController::State::signingIn:
            services.auth.cancelSignIn();
            break;

        case AuthController::State::signedIn:
            services.auth.signOut();
            break;
    }
}

void BeatDropEditor::showSettings (bool shouldShow)
{
    settingsPanel.setVisible (shouldShow);

    if (shouldShow)
        settingsPanel.toFront (true);
}
