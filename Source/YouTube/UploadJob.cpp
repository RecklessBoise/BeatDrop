#include "UploadJob.h"
#include "Core/Text.h"
#include "Video/VideoEncoder.h"
#include "Video/VideoFrame.h"

namespace beatdrop
{

class UploadController::Worker : public juce::Thread
{
public:
    Worker (UploadController& o, UploadRequest r)
        : juce::Thread ("BeatDrop upload"), owner (o), request (std::move (r))
    {
    }

    void run() override
    {
        UploadStatus s;

        const auto fail = [&] (const juce::String& message)
        {
            s.stage = UploadStatus::Stage::failed;
            s.message = message;
            owner.setStatus (s);
        };

        // 1. Render the MP4
        s.stage = UploadStatus::Stage::rendering;
        s.message = "Rendu de la vidéo..."_t;
        owner.setStatus (s);

        const auto cover = VideoFrame::loadImage (request.coverImage);

        if (! cover.isValid())
            return fail ("Image illisible : " + request.coverImage.getFileName());

        const auto videoFile = request.videoOutputDirectory
                                   .getChildFile (makeSafeFileName (request.metadata.title) + ".mp4")
                                   .getNonexistentSibling();

        VideoEncodeRequest encode;
        encode.audioFile = request.take.file;
        encode.frame = VideoFrame::renderFrame (cover, VideoFrame::videoWidth, VideoFrame::videoHeight);
        encode.outputFile = videoFile;

        const auto rendered = encodeStillImageVideo (encode, [this, &s] (double p)
        {
            s.progress = p;
            owner.setStatus (s);
            return ! threadShouldExit();
        });

        if (rendered.failed())
            return fail (rendered.getErrorMessage());

        s.videoFile = videoFile;

        // 2. Upload
        s.stage = UploadStatus::Stage::uploading;
        s.progress = 0.0;
        s.message = "Envoi sur YouTube...";
        owner.setStatus (s);

        juce::String videoId;
        const auto uploaded = owner.youtube.uploadVideo (videoFile, request.metadata, [this, &s] (double p)
        {
            s.progress = p;
            owner.setStatus (s);
            return ! threadShouldExit();
        }, videoId);

        if (uploaded.failed())
            return fail (uploaded.getErrorMessage());

        s.videoId = videoId;

        // 3. Thumbnail (best effort: unverified channels can't set one)
        s.stage = UploadStatus::Stage::thumbnail;
        s.progress = 0.0;
        s.message = "Ajout de la miniature...";
        owner.setStatus (s);

        const auto jpeg = VideoFrame::makeThumbnailJpeg (cover);
        const auto thumbnail = jpeg.isEmpty() ? juce::Result::fail ("miniature trop lourde")
                                              : owner.youtube.setThumbnail (videoId, jpeg);

        s.stage = UploadStatus::Stage::done;
        s.progress = 1.0;
        s.message = request.metadata.publishAt.isNotEmpty() ? juce::String ("Vidéo programmée sur YouTube."_t)
                                                            : juce::String ("Vidéo en ligne sur YouTube."_t);

        if (thumbnail.failed())
            s.message << " (" << thumbnail.getErrorMessage() << ")";

        owner.setStatus (s);
    }

private:
    UploadController& owner;
    UploadRequest request;
};

//==============================================================================
UploadController::UploadController (YouTubeClient& client)
    : youtube (client)
{
}

UploadController::~UploadController()
{
    if (worker != nullptr)
        worker->stopThread (15000);
}

juce::Result UploadController::validateRequest (const UploadRequest& request)
{
    if (! request.take.isValid())
        return juce::Result::fail ("Aucune prise audio : arme BeatDrop puis lance la lecture (ou exporte depuis ton DAW).");

    if (! request.coverImage.existsAsFile())
        return juce::Result::fail ("Ajoute une image pour la vidéo."_t);

    if (! isVideoEncodingSupported())
        return juce::Result::fail ("Le rendu vidéo n'est pas encore disponible sur cette plateforme."_t);

    return Metadata::validate (request.metadata);
}

juce::Result UploadController::start (const UploadRequest& request)
{
    if (getStatus().isBusy())
        return juce::Result::fail ("Un upload est déjà en cours."_t);

    const auto valid = validateRequest (request);

    if (valid.failed())
        return valid;

    if (worker != nullptr)
        worker->stopThread (1000);

    worker = std::make_unique<Worker> (*this, request);
    worker->startThread();
    return juce::Result::ok();
}

void UploadController::cancel()
{
    if (worker != nullptr)
        worker->signalThreadShouldExit();
}

UploadStatus UploadController::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void UploadController::setStatus (const UploadStatus& newStatus)
{
    {
        const juce::ScopedLock sl (lock);
        status = newStatus;
    }

    sendChangeMessage();
}

juce::String UploadController::makeSafeFileName (const juce::String& title)
{
    auto name = juce::File::createLegalFileName (title.replaceCharacters ("/\\:", "   "));
    name = juce::StringArray::fromTokens (name, " ", "").joinIntoString (" ").trim();

    if (name.length() > 80)
        name = name.substring (0, 80).trim();

    return name.isNotEmpty() ? name : juce::String ("BeatDrop");
}

} // namespace beatdrop
