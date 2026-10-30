#include "YouTube/OAuthClient.h"
#include "Core/Settings.h"
#include "Core/TokenStore.h"

using namespace beatdrop;

class OAuthTests : public juce::UnitTest
{
public:
    OAuthTests() : juce::UnitTest ("OAuth", "BeatDrop") {}

    void runTest() override
    {
        beginTest ("PKCE S256 matches the RFC 7636 example");
        expectEquals (OAuth::makeCodeChallenge ("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
                      juce::String ("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));

        beginTest ("verifier and state are random and well formed");
        {
            const auto a = OAuth::makeCodeVerifier();
            const auto b = OAuth::makeCodeVerifier();
            expectEquals (a.length(), 64);
            expect (a != b);
            expect (a.containsOnly ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"));
            expectEquals (OAuth::makeState().length(), 32);
        }

        beginTest ("base64url has no padding or unsafe characters");
        {
            const unsigned char bytes[] = { 0xfb, 0xff, 0xfe };
            expectEquals (OAuth::base64UrlEncode (bytes, sizeof (bytes)), juce::String ("-__-"));
        }

        beginTest ("authorization URL");
        {
            const auto url = OAuth::buildAuthorizationUrl ("client.apps.googleusercontent.com", "http://127.0.0.1:5555", "CHAL", "STATE");
            expect (url.startsWith (OAuth::authEndpoint + "?"));
            expect (url.contains ("client_id=client.apps.googleusercontent.com"));
            expect (url.contains ("redirect_uri=http%3A%2F%2F127.0.0.1%3A5555"));
            expect (url.contains ("code_challenge=CHAL"));
            expect (url.contains ("code_challenge_method=S256"));
            expect (url.contains ("access_type=offline"));
            expect (url.contains ("youtube.upload"));
        }

        beginTest ("loopback redirect parsing");
        {
            auto result = OAuth::parseRedirectRequest ("GET /?state=xyz&code=4%2F0Abc-d&scope=a+b HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
            expectEquals (result.code, juce::String ("4/0Abc-d"));
            expectEquals (result.state, juce::String ("xyz"));
            expect (result.isOAuthCallback());

            result = OAuth::parseRedirectRequest ("GET /?error=access_denied&state=xyz HTTP/1.1\r\n\r\n");
            expectEquals (result.error, juce::String ("access_denied"));

            expect (! OAuth::parseRedirectRequest ("GET /favicon.ico HTTP/1.1\r\n\r\n").isOAuthCallback());
            expect (! OAuth::parseRedirectRequest ("POST / HTTP/1.1\r\n\r\n").isOAuthCallback());
        }

        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("BeatDropTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));

        beginTest ("file token store");
        {
            TokenStore store (dir.getChildFile ("token"));
            expect (store.loadRefreshToken().isEmpty());
            expect (store.saveRefreshToken ("refresh-1"));
            expectEquals (store.loadRefreshToken(), juce::String ("refresh-1"));
            store.clear();
            expect (store.loadRefreshToken().isEmpty());
        }

        beginTest ("access token refresh and caching");
        {
            Settings settings (dir.getChildFile ("test.settings"));
            settings.setGoogleClient ("client-id", "client-secret");

            TokenStore store (dir.getChildFile ("token"));
            store.saveRefreshToken ("refresh-1");

            int calls = 0;
            juce::String lastBody;

            OAuthClient client (settings, store, [&] (const HttpRequest& request)
            {
                ++calls;
                lastBody = request.body.toString();

                HttpResponse response;
                response.status = 200;
                response.body = R"({"access_token":"access-1","expires_in":3599,"token_type":"Bearer"})";
                return response;
            });

            juce::String token;
            expect (client.getAccessToken (token).wasOk());
            expectEquals (token, juce::String ("access-1"));
            expect (lastBody.contains ("grant_type=refresh_token"));
            expect (lastBody.contains ("refresh_token=refresh-1"));

            expect (client.getAccessToken (token).wasOk());
            expectEquals (calls, 1); // cached

            client.invalidateAccessToken();
            expect (client.getAccessToken (token).wasOk());
            expectEquals (calls, 2);
        }

        beginTest ("revoked refresh token signs the user out");
        {
            Settings settings (dir.getChildFile ("test.settings"));
            TokenStore store (dir.getChildFile ("token"));
            store.saveRefreshToken ("revoked");

            OAuthClient client (settings, store, [] (const HttpRequest&)
            {
                HttpResponse response;
                response.status = 400;
                response.body = R"({"error":"invalid_grant","error_description":"Token has been expired or revoked."})";
                return response;
            });

            juce::String token;
            expect (client.getAccessToken (token).failed());
            expect (! client.isSignedIn());
        }

        dir.deleteRecursively();
    }
};

static OAuthTests oauthTests;
