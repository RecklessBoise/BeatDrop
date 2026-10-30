#include "VideoEncoder.h"
#include "Core/Text.h"

// TODO(windows): implement with Media Foundation (IMFSinkWriter, H.264 + AAC).
namespace beatdrop
{

bool isVideoEncodingSupported()
{
    return false;
}

juce::Result encodeStillImageVideo (const VideoEncodeRequest&, const std::function<bool (double)>&)
{
    return juce::Result::fail ("Le rendu vidéo n'est pas encore disponible sur cette plateforme."_t);
}

} // namespace beatdrop
