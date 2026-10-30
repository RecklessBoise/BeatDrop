#pragma once

#include "Http.h"

namespace beatdrop
{

class Settings;
class TokenStore;

/** Pure OAuth helpers, exposed for tests. */
namespace OAuth
{
    inline const juce::String authEndpoint  { "https://accounts.google.com/o/oauth2/v2/auth" };
    inline const juce::String tokenEndpoint { "https://oauth2.googleapis.com/token" };
    inline const juce::String revokeEndpoint { "https://oauth2.googleapis.com/revoke" };
    inline const juce::String scopes {
        "https://www.googleapis.com/auth/youtube.upload "
        "https://www.googleapis.com/auth/youtube.readonly" };

    juce::String base64UrlEncode (const void* data, size_t numBytes);

    /** 64 characters from the RFC 7636 unreserved set. */
    juce::String makeCodeVerifier();

    /** BASE64URL(SHA256(verifier)) - the S256 challenge. */
    juce::String makeCodeChallenge (const juce::String& verifier);

    juce::String makeState();

    juce::String buildAuthorizationUrl (const juce::String& clientId, const juce::String& redirectUri,
                                        const juce::String& codeChallenge, const juce::String& state);

    struct RedirectResult
    {
        juce::String code, state, error;
        bool isOAuthCallback() const   { return code.isNotEmpty() || error.isNotEmpty(); }
    };

    /** Parses "GET /?code=...&state=... HTTP/1.1" sent by the browser to the loopback server. */
    RedirectResult parseRedirectRequest (const juce::String& requestHead);
}

/**
    Google OAuth 2.0 for installed apps: loopback redirect + PKCE.
    All methods block; call them from a background thread.
*/
class OAuthClient
{
public:
    OAuthClient (Settings&, TokenStore&, HttpTransport transport = performHttpRequest);

    bool isSignedIn() const;

    /** Opens the browser and waits (up to 5 minutes) for Google to redirect back. */
    juce::Result signIn (const std::function<bool()>& shouldAbort);

    /** Returns a valid access token, refreshing it when needed. */
    juce::Result getAccessToken (juce::String& accessToken);

    /** Forgets the cached access token (after a 401). */
    void invalidateAccessToken();

    void signOut();

private:
    juce::Result exchangeCode (const juce::String& code, const juce::String& verifier, const juce::String& redirectUri);
    juce::Result storeTokenResponse (const HttpResponse& response);
    static juce::String describeTokenError (const HttpResponse& response);

    Settings& settings;
    TokenStore& tokenStore;
    HttpTransport transport;

    juce::CriticalSection lock;
    juce::String cachedAccessToken;
    juce::Time accessTokenExpiry;
};

} // namespace beatdrop
