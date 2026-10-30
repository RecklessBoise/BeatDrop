#pragma once

#include <juce_graphics/juce_graphics.h>

namespace beatdrop::VideoFrame
{
    constexpr int videoWidth = 1920;
    constexpr int videoHeight = 1080;
    constexpr int thumbnailWidth = 1280;
    constexpr int thumbnailHeight = 720;
    constexpr size_t maxThumbnailBytes = 2 * 1024 * 1024;

    juce::Image loadImage (const juce::File& file);

    /** The cover fitted inside a 16:9 frame over a blurred, darkened copy of itself. */
    juce::Image renderFrame (const juce::Image& cover, int width, int height);

    /** 1280x720 JPEG under YouTube's 2 MB thumbnail limit. */
    juce::MemoryBlock makeThumbnailJpeg (const juce::Image& cover);
}
