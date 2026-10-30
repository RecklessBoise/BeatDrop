#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace beatdrop
{

/** Drop or pick the cover image; shows a preview of the video frame. */
class ImageDropZone : public juce::Component,
                      public juce::FileDragAndDropTarget
{
public:
    ImageDropZone();

    void setImageFile (const juce::File& file);
    juce::File getImageFile() const               { return imageFile; }

    std::function<void (const juce::File&)> onImageChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    static bool isSupportedImage (const juce::File& file);

private:
    void choose();
    void rebuildPreview();

    juce::File imageFile;
    juce::Image preview;
    juce::Rectangle<int> previewArea;
    bool dragHover = false;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImageDropZone)
};

} // namespace beatdrop
