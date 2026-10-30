#include "ImageDropZone.h"
#include "Core/Text.h"
#include "BeatDropLookAndFeel.h"
#include "Video/VideoFrame.h"

namespace beatdrop
{

ImageDropZone::ImageDropZone()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Image de la vidéo"_t);
}

bool ImageDropZone::isSupportedImage (const juce::File& file)
{
    return file.hasFileExtension ("png;jpg;jpeg");
}

void ImageDropZone::setImageFile (const juce::File& file)
{
    imageFile = file;
    rebuildPreview();

    if (onImageChanged != nullptr)
        onImageChanged (imageFile);
}

void ImageDropZone::rebuildPreview()
{
    preview = {};

    if (imageFile.existsAsFile() && previewArea.getWidth() > 0)
        if (const auto image = VideoFrame::loadImage (imageFile); image.isValid())
            preview = VideoFrame::renderFrame (image, previewArea.getWidth() * 2, previewArea.getHeight() * 2);

    repaint();
}

void ImageDropZone::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    BeatDropLookAndFeel::drawSectionTitle (g, { 16, 10, getWidth() - 32, 18 }, "Image de la vidéo"_t);

    const auto area = previewArea.toFloat();

    if (preview.isValid())
    {
        juce::Path clip;
        clip.addRoundedRectangle (area, 6.0f);
        g.saveState();
        g.reduceClipRegion (clip);
        g.drawImage (preview, area, juce::RectanglePlacement::stretchToFit);
        g.restoreState();
    }
    else
    {
        g.setColour (Palette::field);
        g.fillRoundedRectangle (area, 6.0f);

        g.setColour (Palette::muted);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText ("Glisse une image ici\nou clique pour en choisir une (PNG / JPG)",
                          previewArea.reduced (12), juce::Justification::centred, 3);
    }

    juce::Path border;
    border.addRoundedRectangle (area.reduced (0.5f), 6.0f);

    if (dragHover)
    {
        g.setColour (Palette::accent);
        g.strokePath (border, juce::PathStrokeType (2.0f));
    }
    else if (! preview.isValid())
    {
        const float dashes[] = { 6.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (Palette::outline.brighter (0.3f));
        g.fillPath (dashed);
    }
}

void ImageDropZone::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (24);

    // 16:9 preview, as large as fits
    const auto width = juce::jmin (area.getWidth(), area.getHeight() * 16 / 9);
    previewArea = area.withSizeKeepingCentre (width, width * 9 / 16);
    rebuildPreview();
}

void ImageDropZone::mouseUp (const juce::MouseEvent& e)
{
    if (previewArea.contains (e.getPosition()))
        choose();
}

bool ImageDropZone::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isSupportedImage (juce::File (files[0]));
}

void ImageDropZone::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void ImageDropZone::fileDragExit (const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void ImageDropZone::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    setImageFile (juce::File (files[0]));
}

void ImageDropZone::choose()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choisis l'image de la vidéo"_t,
                                                       juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                                                       "*.png;*.jpg;*.jpeg");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<ImageDropZone> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis != nullptr && chooser.getResult().existsAsFile())
            safeThis->setImageFile (chooser.getResult());
    });
}

} // namespace beatdrop
