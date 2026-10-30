#include "BeatDropLookAndFeel.h"

namespace beatdrop
{

BeatDropLookAndFeel::BeatDropLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);

    setColour (juce::TextButton::buttonColourId, Palette::field);
    setColour (juce::TextButton::buttonOnColourId, Palette::accent);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);

    setColour (juce::TextEditor::backgroundColourId, Palette::field);
    setColour (juce::TextEditor::textColourId, Palette::text);
    setColour (juce::TextEditor::outlineColourId, Palette::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent.withAlpha (0.8f));
    setColour (juce::TextEditor::highlightColourId, Palette::accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, Palette::accent);

    setColour (juce::Label::textColourId, Palette::text);

    setColour (juce::ComboBox::backgroundColourId, Palette::field);
    setColour (juce::ComboBox::outlineColourId, Palette::outline);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::arrowColourId, Palette::muted);
    setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent);

    setColour (juce::ProgressBar::backgroundColourId, Palette::field);
    setColour (juce::ProgressBar::foregroundColourId, Palette::accent);

    setColour (juce::HyperlinkButton::textColourId, Palette::success);
}

void BeatDropLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                                bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour;

    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.4f);
    else if (isButtonDown)
        colour = colour.darker (0.2f);
    else if (isMouseOverButton)
        colour = colour.brighter (0.08f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 6.0f);

    if (! button.getToggleState())
    {
        g.setColour (Palette::outline);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }
}

void BeatDropLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<int> (width, height).toFloat(), 6.0f);
}

void BeatDropLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const auto focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
    g.setColour (editor.findColour (focused ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<int> (width, height).toFloat().reduced (0.5f), 6.0f, focused ? 1.5f : 1.0f);
}

void BeatDropLookAndFeel::drawProgressBar (juce::Graphics& g, juce::ProgressBar& bar, int width, int height,
                                           double progress, const juce::String& textToShow)
{
    const auto bounds = juce::Rectangle<int> (width, height).toFloat();

    g.setColour (bar.findColour (juce::ProgressBar::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);

    if (progress >= 0.0 && progress <= 1.0)
    {
        g.setColour (bar.findColour (juce::ProgressBar::foregroundColourId));
        g.fillRoundedRectangle (bounds.withWidth ((float) (bounds.getWidth() * progress)), 4.0f);
    }

    if (textToShow.isNotEmpty())
    {
        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (textToShow, bounds, juce::Justification::centred);
    }
}

void BeatDropLookAndFeel::drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    g.setColour (Palette::muted);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (text.toUpperCase(), area, juce::Justification::centredLeft);
}

FieldLabel::FieldLabel (const juce::String& text)
    : juce::Label ({}, text)
{
    setFont (juce::FontOptions (12.0f));
    setColour (juce::Label::textColourId, Palette::muted);
    setBorderSize ({});
}

} // namespace beatdrop
