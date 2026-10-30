#include "YouTubeClient.h"
#include "Core/Text.h"

namespace beatdrop
{

YouTubeClient::YouTubeClient (TokenProvider tokens, HttpTransport httpTransport)
    : tokenProvider (std::move (tokens)), transport (std::move (httpTransport))
{
}

HttpResponse YouTubeClient::authorisedRequest (HttpRequest request, juce::Result& tokenResult)
{
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        juce::String token;
        tokenResult = tokenProvider (token, attempt > 0);

        if (tokenResult.failed())
            return {};

        request.headers.set ("Authorization", "Bearer " + token);
        auto response = transport (request);

        if (response.status != 401)
            return response;
    }

    tokenResult = juce::Result::fail ("YouTube a refusé l'autorisation. Reconnecte ta chaîne."_t);
    return {};
}

juce::Result YouTubeClient::fetchMyChannel (ChannelInfo& channel)
{
    HttpRequest request;
    request.url = juce::URL (channelsUrl);

    auto tokenResult = juce::Result::ok();
    const auto response = authorisedRequest (request, tokenResult);

    if (tokenResult.failed())
        return tokenResult;

    if (! response.isSuccess())
        return juce::Result::fail (describeApiError (response));

    const auto items = response.json().getProperty ("items", {});

    if (! items.isArray() || items.size() == 0)
        return juce::Result::fail ("Ce compte Google n'a pas de chaîne YouTube."_t);

    channel.id = items[0].getProperty ("id", {}).toString();
    channel.title = items[0].getProperty ("snippet", {}).getProperty ("title", {}).toString();
    return juce::Result::ok();
}

juce::Result YouTubeClient::uploadVideo (const juce::File& videoFile, const UploadMetadata& metadata,
                                         const std::function<bool (double)>& progress, juce::String& videoId)
{
    juce::MemoryBlock data;

    if (! videoFile.existsAsFile() || ! videoFile.loadFileAsData (data) || data.isEmpty())
        return juce::Result::fail ("Vidéo introuvable : "_t + videoFile.getFullPathName());

    const auto total = (juce::int64) data.getSize();
    auto lastProgress = 0.0;

    const auto reportProgress = [&] (double value)
    {
        lastProgress = value;
        return progress == nullptr || progress (value);
    };

    // 1. Open a resumable session carrying the metadata.
    HttpRequest init;
    init.url = juce::URL (uploadInitUrl);
    init.method = "POST";
    init.headers.set ("Content-Type", "application/json; charset=UTF-8");
    init.headers.set ("X-Upload-Content-Length", juce::String (total));
    init.headers.set ("X-Upload-Content-Type", "video/mp4");

    const auto json = juce::JSON::toString (Metadata::toVideoResource (metadata), true);
    init.body.append (json.toRawUTF8(), json.getNumBytesAsUTF8());

    auto tokenResult = juce::Result::ok();
    const auto initResponse = authorisedRequest (init, tokenResult);

    if (tokenResult.failed())
        return tokenResult;

    if (! initResponse.isSuccess())
        return juce::Result::fail (describeApiError (initResponse));

    const auto sessionUrl = initResponse.headers["Location"];

    if (sessionUrl.isEmpty())
        return juce::Result::fail ("YouTube n'a pas ouvert de session d'upload.");

    // 2. Send the bytes, resuming where YouTube stopped after a failure.
    juce::int64 offset = 0;

    const auto parseVideoId = [&videoId] (const HttpResponse& response)
    {
        videoId = response.json().getProperty ("id", {}).toString();
        return videoId.isNotEmpty() ? juce::Result::ok()
                                    : juce::Result::fail ("YouTube n'a pas renvoyé d'identifiant de vidéo."_t);
    };

    for (int attempt = 0; attempt < maxUploadAttempts; ++attempt)
    {
        bool cancelled = false;

        HttpRequest put;
        put.url = juce::URL (sessionUrl);
        put.method = "PUT";
        put.followRedirects = false;
        put.timeoutMs = 60000;
        put.headers.set ("Content-Type", "video/mp4");

        if (offset > 0)
            put.headers.set ("Content-Range", "bytes " + juce::String (offset) + "-" + juce::String (total - 1) + "/" + juce::String (total));

        put.body.append (static_cast<const char*> (data.getData()) + offset, (size_t) (total - offset));
        put.progress = [&] (int sent, int)
        {
            if (! reportProgress ((double) (offset + sent) / (double) total))
                cancelled = true;

            return ! cancelled;
        };

        const auto response = authorisedRequest (put, tokenResult);

        if (cancelled)
            return juce::Result::fail ("Upload annulé."_t);

        if (tokenResult.failed())
            return tokenResult;

        if (response.isSuccess())
        {
            reportProgress (1.0);
            return parseVideoId (response);
        }

        const auto retryable = response.status == 0 || response.status == 308 || response.status >= 500;

        if (! retryable)
            return juce::Result::fail (describeApiError (response));

        // Exponential back-off, still cancellable.
        const auto delay = retryBaseDelayMs * (1 << juce::jmin (attempt, 5));

        for (int waited = 0; waited < delay; waited += 100)
        {
            if (! reportProgress (lastProgress))
                return juce::Result::fail ("Upload annulé."_t);

            juce::Thread::sleep (juce::jmin (100, delay));
        }

        // Ask YouTube how much it already has.
        HttpRequest status;
        status.url = juce::URL (sessionUrl);
        status.method = "PUT";
        status.followRedirects = false;
        status.headers.set ("Content-Range", "bytes */" + juce::String (total));
        status.headers.set ("Content-Length", "0");

        const auto statusResponse = authorisedRequest (status, tokenResult);

        if (tokenResult.failed())
            return tokenResult;

        if (statusResponse.isSuccess())
        {
            reportProgress (1.0);
            return parseVideoId (statusResponse);
        }

        if (statusResponse.status == 308)
            offset = parseRangeHeader (statusResponse.headers["Range"]).value_or (0);
        else if (statusResponse.status == 404 || statusResponse.status == 410)
            return juce::Result::fail ("La session d'upload a expiré. Relance l'upload."_t);
    }

    return juce::Result::fail ("Upload interrompu après plusieurs tentatives. Vérifie ta connexion."_t);
}

juce::Result YouTubeClient::setThumbnail (const juce::String& videoId, const juce::MemoryBlock& jpegData)
{
    HttpRequest request;
    request.url = juce::URL (thumbnailUrl + juce::URL::addEscapeChars (videoId, true));
    request.method = "POST";
    request.headers.set ("Content-Type", "image/jpeg");
    request.body = jpegData;

    auto tokenResult = juce::Result::ok();
    const auto response = authorisedRequest (request, tokenResult);

    if (tokenResult.failed())
        return tokenResult;

    if (! response.isSuccess())
        return juce::Result::fail (describeApiError (response));

    return juce::Result::ok();
}

juce::String YouTubeClient::describeApiError (const HttpResponse& response)
{
    if (response.status == 0)
        return "Pas de réponse de YouTube (connexion internet ?)."_t;

    const auto error = response.json().getProperty ("error", {});
    const auto message = error.getProperty ("message", {}).toString();
    const auto errors = error.getProperty ("errors", {});
    const auto reason = errors.isArray() && errors.size() > 0 ? errors[0].getProperty ("reason", {}).toString()
                                                              : juce::String();

    if (reason == "quotaExceeded")
        return "Quota de l'API YouTube épuisé pour aujourd'hui (remise à zéro à minuit, heure du Pacifique)."_t;

    if (reason == "uploadLimitExceeded")
        return "Ta chaîne a atteint sa limite d'uploads YouTube pour le moment."_t;

    if (reason == "youtubeSignupRequired")
        return "Ce compte Google n'a pas encore de chaîne YouTube."_t;

    if (reason == "invalidTitle")       return "YouTube refuse ce titre.";
    if (reason == "invalidDescription") return "YouTube refuse cette description.";
    if (reason == "invalidTags")        return "YouTube refuse ces tags.";
    if (reason == "invalidPublishAt")   return "Date de publication programmée refusée par YouTube."_t;

    if (response.status == 403 && message.containsIgnoreCase ("thumbnail"))
        return "Miniature refusée : vérifie ta chaîne sur youtube.com/verify pour activer les miniatures personnalisées."_t;

    return "Erreur YouTube (HTTP " + juce::String (response.status) + ")"
           + (message.isNotEmpty() ? " : " + message : juce::String());
}

std::optional<juce::int64> YouTubeClient::parseRangeHeader (const juce::String& rangeHeader)
{
    const auto range = rangeHeader.trim();

    if (! range.startsWith ("bytes=") || ! range.containsChar ('-'))
        return std::nullopt;

    const auto lastByte = range.fromFirstOccurrenceOf ("-", false, false).trim();

    if (lastByte.isEmpty() || ! lastByte.containsOnly ("0123456789"))
        return std::nullopt;

    return lastByte.getLargeIntValue() + 1;
}

} // namespace beatdrop
