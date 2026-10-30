#pragma once

#include "YouTubeClient.h"
#include "Core/MasterRecorder.h"

#include <juce_events/juce_events.h>

namespace beatdrop
{

struct UploadRequest
{
    Take take;
    juce::File coverImage;
    UploadMetadata metadata;
    juce::File videoOutputDirectory;
};

struct UploadStatus
{
    enum class Stage
    {
        idle,
        rendering,
        uploading,
        thumbnail,
        done,
        failed
    };

    Stage stage = Stage::idle;
    double progress = 0.0;  // 0..1 for the current stage
    juce::String message;
    juce::String videoId;
    juce::File videoFile;

    bool isBusy() const noexcept   { return stage == Stage::rendering || stage == Stage::uploading || stage == Stage::thumbnail; }
    juce::String getVideoUrl() const { return videoId.isNotEmpty() ? "https://youtu.be/" + videoId : juce::String(); }
};

/**
    Runs render -> upload -> thumbnail on a background thread.
    Owned by the processor so closing the plugin window doesn't stop an upload.
    Change messages are delivered on the message thread.
*/
class UploadController : public juce::ChangeBroadcaster
{
public:
    explicit UploadController (YouTubeClient& client);
    ~UploadController() override;

    /** Checks the request and starts the job; fails straight away on invalid input. */
    juce::Result start (const UploadRequest& request);
    void cancel();

    UploadStatus getStatus() const;

    /** Validation shared with the UI so the Upload button can explain what's missing. */
    static juce::Result validateRequest (const UploadRequest& request);

    /** "My Beat: v2/final" -> "My Beat v2 final" */
    static juce::String makeSafeFileName (const juce::String& title);

private:
    class Worker;
    friend class Worker;

    void setStatus (const UploadStatus& newStatus);

    YouTubeClient& youtube;
    std::unique_ptr<Worker> worker;

    mutable juce::CriticalSection lock;
    UploadStatus status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UploadController)
};

} // namespace beatdrop
