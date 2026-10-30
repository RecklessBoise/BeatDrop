#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace beatdrop
{

namespace Palette
{
    inline const juce::Colour background  { 0xff0e0e11 };
    inline const juce::Colour panel       { 0xff17171c };
    inline const juce::Colour field       { 0xff202027 };
    inline const juce::Colour outline     { 0xff2c2c35 };
    inline const juce::Colour text        { 0xffededf0 };
    inline const juce::Colour muted       { 0xff8b8b96 };
    inline const juce::Colour accent      { 0xffff3b3b };
    inline const juce::Colour success     { 0xff3ddc84 };
    inline const juce::Colour warning     { 0xffffb020 };
}

class BeatDropLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BeatDropLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawProgressBar (juce::Graphics&, juce::ProgressBar&, int width, int height,
                          double progress, const juce::String& textToShow) override;

    /** Section title drawn above each group of controls. */
    static void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);
};

/** Small caption above an input field. */
class FieldLabel : public juce::Label
{
public:
    explicit FieldLabel (const juce::String& text);
};

} // namespace beatdrop
