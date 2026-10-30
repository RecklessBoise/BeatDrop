#pragma once

#include <juce_graphics/juce_graphics.h>

namespace beatdrop
{

struct VideoEncodeRequest
{
    juce::File audioFile;
    juce::Image frame;            // already rendered at the output size
    juce::File outputFile;        // .mp4
    int framesPerSecond = 2;      // a still image needs very few frames
};

bool isVideoEncodingSupported();

/** Blocking. `progress` receives 0..1 and returns false to cancel. */
juce::Result encodeStillImageVideo (const VideoEncodeRequest& request, const std::function<bool (double)>& progress);

} // namespace beatdrop
