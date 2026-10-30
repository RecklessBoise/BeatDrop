#pragma once

#include "Http.h"
#include "Core/Metadata.h"

namespace beatdrop
{

struct ChannelInfo
{
    juce::String id;
    juce::String title;
};

/** YouTube Data API v3 calls. Blocking; use from a background thread. */
class YouTubeClient
{
public:
    /** Provides a valid OAuth access token; `forceRefresh` is set after a 401. */
    using TokenProvider = std::function<juce::Result (juce::String& token, bool forceRefresh)>;

    explicit YouTubeClient (TokenProvider tokens, HttpTransport transport = performHttpRequest);

    juce::Result fetchMyChannel (ChannelInfo& channel);

    /** Resumable upload. `progress` receives 0..1 and returns false to cancel. */
    juce::Result uploadVideo (const juce::File& videoFile, const UploadMetadata& metadata,
                              const std::function<bool (double)>& progress, juce::String& videoId);

    juce::Result setThumbnail (const juce::String& videoId, const juce::MemoryBlock& jpegData);

    //==============================================================================
    static inline const juce::String uploadInitUrl {
        "https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&part=snippet,status" };
    static inline const juce::String channelsUrl {
        "https://www.googleapis.com/youtube/v3/channels?part=snippet&mine=true" };
    static inline const juce::String thumbnailUrl {
        "https://www.googleapis.com/upload/youtube/v3/thumbnails/set?videoId=" };

    /** Human readable (French) message for a failed API response. */
    static juce::String describeApiError (const HttpResponse& response);

    /** "bytes=0-524287" -> 524288 (the next byte to send); nullopt when absent/invalid. */
    static std::optional<juce::int64> parseRangeHeader (const juce::String& rangeHeader);

    static inline int maxUploadAttempts = 6;
    static inline int retryBaseDelayMs = 1000;

private:
    HttpResponse authorisedRequest (HttpRequest request, juce::Result& tokenResult);

    TokenProvider tokenProvider;
    HttpTransport transport;
};

} // namespace beatdrop
