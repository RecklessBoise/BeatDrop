#include "YouTube/YouTubeClient.h"
#include "YouTube/UploadJob.h"

using namespace beatdrop;

class YouTubeRequestTests : public juce::UnitTest
{
public:
    YouTubeRequestTests() : juce::UnitTest ("YouTube API", "BeatDrop") {}

    static HttpResponse makeResponse (int status, const juce::String& body = {}, juce::StringPairArray headers = {})
    {
        HttpResponse response;
        response.status = status;
        response.body = body;
        response.headers = headers;
        return response;
    }

    static juce::StringPairArray header (const juce::String& key, const juce::String& value)
    {
        juce::StringPairArray headers;
        headers.set (key, value);
        return headers;
    }

    void runTest() override
    {
        YouTubeClient::retryBaseDelayMs = 1;

        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("BeatDropTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        dir.createDirectory();

        const auto video = dir.getChildFile ("video.mp4");
        juce::MemoryBlock videoBytes (1000, true);
        video.replaceWithData (videoBytes.getData(), videoBytes.getSize());

        UploadMetadata metadata;
        metadata.title = "Midnight";
        metadata.tags = { "type beat" };

        const auto tokens = [] (juce::String& token, bool) { token = "ya29.test"; return juce::Result::ok(); };

        beginTest ("Range header parsing");
        expect (YouTubeClient::parseRangeHeader ("bytes=0-524287") == std::optional<juce::int64> (524288));
        expect (! YouTubeClient::parseRangeHeader ("").has_value());
        expect (! YouTubeClient::parseRangeHeader ("bytes=0-").has_value());

        beginTest ("resumable upload in one go");
        {
            std::vector<HttpRequest> requests;

            YouTubeClient client (tokens, [&] (const HttpRequest& request)
            {
                requests.push_back (request);

                if (requests.size() == 1)
                    return makeResponse (200, {}, header ("Location", "https://upload.example/session-1"));

                if (request.progress != nullptr)
                    request.progress ((int) request.body.getSize(), (int) request.body.getSize());

                return makeResponse (200, R"({"id":"VIDEO123","kind":"youtube#video"})");
            });

            juce::String videoId;
            double progress = 0.0;
            const auto result = client.uploadVideo (video, metadata, [&] (double p) { progress = p; return true; }, videoId);

            expect (result.wasOk(), result.getErrorMessage());
            expectEquals (videoId, juce::String ("VIDEO123"));
            expectWithinAbsoluteError (progress, 1.0, 0.0001);
            expectEquals ((int) requests.size(), 2);

            const auto& init = requests[0];
            expectEquals (init.method, juce::String ("POST"));
            expect (init.url.toString (true).contains ("uploadType=resumable"));
            expectEquals (init.headers["Authorization"], juce::String ("Bearer ya29.test"));
            expectEquals (init.headers["X-Upload-Content-Length"], juce::String (1000));

            const auto json = juce::JSON::parse (init.body.toString());
            expectEquals (json["snippet"]["title"].toString(), juce::String ("Midnight"));

            const auto& put = requests[1];
            expectEquals (put.method, juce::String ("PUT"));
            expectEquals (put.url.toString (true), juce::String ("https://upload.example/session-1"));
            expectEquals ((int) put.body.getSize(), 1000);
            expect (! put.headers.containsKey ("Content-Range"));
        }

        beginTest ("upload resumes after a server error");
        {
            std::vector<HttpRequest> requests;

            YouTubeClient client (tokens, [&] (const HttpRequest& request)
            {
                requests.push_back (request);

                switch (requests.size())
                {
                    case 1:  return makeResponse (200, {}, header ("Location", "https://upload.example/session-2"));
                    case 2:  return makeResponse (503);                                     // first PUT fails
                    case 3:  return makeResponse (308, {}, header ("Range", "bytes=0-399")); // 400 bytes arrived
                    default: return makeResponse (201, R"({"id":"RESUMED"})");
                }
            });

            juce::String videoId;
            const auto result = client.uploadVideo (video, metadata, nullptr, videoId);

            expect (result.wasOk(), result.getErrorMessage());
            expectEquals (videoId, juce::String ("RESUMED"));
            expectEquals ((int) requests.size(), 4);

            if (requests.size() == 4)
            {
                expectEquals (requests[2].headers["Content-Range"], juce::String ("bytes */1000"));
                expectEquals (requests[3].headers["Content-Range"], juce::String ("bytes 400-999/1000"));
                expectEquals ((int) requests[3].body.getSize(), 600);
            }
        }

        beginTest ("401 forces a token refresh once");
        {
            int forcedRefreshes = 0, calls = 0;

            YouTubeClient client ([&] (juce::String& token, bool force)
                                  {
                                      forcedRefreshes += force ? 1 : 0;
                                      token = force ? "fresh" : "stale";
                                      return juce::Result::ok();
                                  },
                                  [&] (const HttpRequest& request)
                                  {
                                      ++calls;

                                      if (request.headers["Authorization"] == "Bearer stale")
                                          return makeResponse (401);

                                      return makeResponse (200, R"({"items":[{"id":"UC1","snippet":{"title":"Reckless Beats"}}]})");
                                  });

            ChannelInfo channel;
            expect (client.fetchMyChannel (channel).wasOk());
            expectEquals (channel.title, juce::String ("Reckless Beats"));
            expectEquals (forcedRefreshes, 1);
            expectEquals (calls, 2);
        }

        beginTest ("API errors are explained");
        {
            const auto quota = makeResponse (403, R"({"error":{"code":403,"message":"quota","errors":[{"reason":"quotaExceeded"}]}})");
            expect (YouTubeClient::describeApiError (quota).containsIgnoreCase ("quota"));

            YouTubeClient client (tokens, [&] (const HttpRequest&) { return quota; });
            juce::String videoId;
            const auto result = client.uploadVideo (video, metadata, nullptr, videoId);
            expect (result.failed());
            expect (result.getErrorMessage().containsIgnoreCase ("quota"));

            expect (YouTubeClient::describeApiError (makeResponse (0)).isNotEmpty());
        }

        beginTest ("cancelling stops the upload");
        {
            YouTubeClient client (tokens, [&] (const HttpRequest& request)
            {
                if (request.method == "POST")
                    return makeResponse (200, {}, header ("Location", "https://upload.example/session-3"));

                if (request.progress != nullptr && ! request.progress (10, 1000))
                    return makeResponse (0);

                return makeResponse (200, R"({"id":"X"})");
            });

            juce::String videoId;
            const auto result = client.uploadVideo (video, metadata, [] (double) { return false; }, videoId);
            expect (result.failed());
            expect (videoId.isEmpty());
        }

        beginTest ("thumbnail request");
        {
            HttpRequest captured;
            YouTubeClient client (tokens, [&] (const HttpRequest& request) { captured = request; return makeResponse (200, "{}"); });

            const unsigned char jpegBytes[] = { 0xff, 0xd8, 0xff };
            const juce::MemoryBlock jpeg (jpegBytes, sizeof (jpegBytes));
            expect (client.setThumbnail ("VID", jpeg).wasOk());
            expect (captured.url.toString (true).endsWith ("videoId=VID"));
            expectEquals (captured.headers["Content-Type"], juce::String ("image/jpeg"));
            expectEquals ((int) captured.body.getSize(), 3);
        }

        beginTest ("upload request validation");
        {
            UploadRequest request;
            request.metadata = metadata;
            expect (UploadController::validateRequest (request).failed()); // no take

            expectEquals (UploadController::makeSafeFileName ("[FREE] Drake Type Beat - \"Mid/night\""),
                          juce::String ("[FREE] Drake Type Beat - Mid night"));
            expectEquals (UploadController::makeSafeFileName ("   "), juce::String ("BeatDrop"));
        }

        dir.deleteRecursively();
    }
};

static YouTubeRequestTests youtubeRequestTests;
