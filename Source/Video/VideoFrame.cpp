#include "VideoFrame.h"

namespace beatdrop::VideoFrame
{

juce::Image loadImage (const juce::File& file)
{
    return juce::ImageFileFormat::loadFrom (file);
}

juce::Image renderFrame (const juce::Image& cover, int width, int height)
{
    juce::Image frame (juce::Image::ARGB, width, height, true);
    juce::Graphics g (frame);
    g.fillAll (juce::Colours::black);

    if (! cover.isValid())
        return frame;

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    // Cheap blur: squash the cover into a tiny image, then stretch it back up.
    juce::Image tiny (juce::Image::ARGB, 48, 27, true);
    {
        juce::Graphics tg (tiny);
        tg.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        tg.drawImage (cover, tiny.getBounds().toFloat(), juce::RectanglePlacement::fillDestination);
    }

    g.drawImage (tiny, frame.getBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    g.fillAll (juce::Colours::black.withAlpha (0.55f));

    g.drawImage (cover, frame.getBounds().toFloat(), juce::RectanglePlacement::centred);
    return frame;
}

juce::MemoryBlock makeThumbnailJpeg (const juce::Image& cover)
{
    const auto frame = renderFrame (cover, thumbnailWidth, thumbnailHeight);

    for (auto quality = 0.92f; quality > 0.3f; quality -= 0.1f)
    {
        juce::MemoryOutputStream out;
        juce::JPEGImageFormat jpeg;
        jpeg.setQuality (quality);

        if (jpeg.writeImageToStream (frame, out) && out.getDataSize() <= maxThumbnailBytes)
            return out.getMemoryBlock();
    }

    return {};
}

} // namespace beatdrop::VideoFrame
