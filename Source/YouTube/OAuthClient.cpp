#include "OAuthClient.h"
#include "Core/Text.h"
#include "Core/Settings.h"
#include "Core/TokenStore.h"

#include <juce_cryptography/juce_cryptography.h>
#include <random>

namespace beatdrop
{

namespace OAuth
{

juce::String base64UrlEncode (const void* data, size_t numBytes)
{
    return juce::Base64::toBase64 (data, numBytes)
               .replaceCharacter ('+', '-')
               .replaceCharacter ('/', '_')
               .trimCharactersAtEnd ("=");
}

static juce::String randomString (int length, const char* alphabet)
{
    std::random_device device; // backed by the OS CSPRNG on macOS, Windows and Linux
    const auto alphabetSize = (unsigned int) std::strlen (alphabet);

    juce::String result;
    result.preallocateBytes ((size_t) length);

    for (int i = 0; i < length; ++i)
        result << juce::String::charToString ((juce::juce_wchar) alphabet[device() % alphabetSize]);

    return result;
}

juce::String makeCodeVerifier()
{
    return randomString (64, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~");
}

juce::String makeCodeChallenge (const juce::String& verifier)
{
    const juce::SHA256 hash (verifier.toRawUTF8(), verifier.getNumBytesAsUTF8());
    const auto digest = hash.getRawData();
    return base64UrlEncode (digest.getData(), digest.getSize());
}

juce::String makeState()
{
    return randomString (32, "abcdefghijklmnopqrstuvwxyz0123456789");
}

juce::String buildAuthorizationUrl (const juce::String& clientId, const juce::String& redirectUri,
                                    const juce::String& codeChallenge, const juce::String& state)
{
    juce::StringPairArray params;
    params.set ("client_id", clientId);
    params.set ("redirect_uri", redirectUri);
    params.set ("response_type", "code");
    params.set ("scope", scopes);
    params.set ("code_challenge", codeChallenge);
    params.set ("code_challenge_method", "S256");
    params.set ("state", state);
    params.set ("access_type", "offline");   // we need a refresh token
    params.set ("prompt", "consent");        // ...even if the user consented before

    return authEndpoint + "?" + formEncode (params);
}

RedirectResult parseRedirectRequest (const juce::String& requestHead)
{
    RedirectResult result;

    const auto firstLine = requestHead.upToFirstOccurrenceOf ("\r\n", false, false)
                                      .upToFirstOccurrenceOf ("\n", false, false);

    if (! firstLine.startsWith ("GET "))
        return result;

    const auto target = firstLine.fromFirstOccurrenceOf ("GET ", false, false)
                                 .upToLastOccurrenceOf (" HTTP/", false, false);
    const auto query = target.fromFirstOccurrenceOf ("?", false, false);

    for (const auto& pair : juce::StringArray::fromTokens (query, "&", ""))
    {
        const auto key = pair.upToFirstOccurrenceOf ("=", false, false);
        const auto value = juce::URL::removeEscapeChars (pair.fromFirstOccurrenceOf ("=", false, false).replaceCharacter ('+', ' '));

        if (key == "code")       result.code = value;
        else if (key == "state") result.state = value;
        else if (key == "error") result.error = value;
    }

    return result;
}

} // namespace OAuth

//==============================================================================
namespace
{
    constexpr int signInTimeoutMs = 5 * 60 * 1000;

    juce::String escapeHtml (const juce::String& text)
    {
        return text.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;").replace ("\"", "&quot;");
    }

    juce::String makeHtmlPage (bool success, const juce::String& detail)
    {
        const auto title = success ? juce::String ("BeatDrop est connecté à ta chaîne YouTube"_t)
                                   : juce::String ("La connexion a échoué"_t);

        return "<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\"><title>BeatDrop</title>"
               "<style>body{font-family:-apple-system,Helvetica,sans-serif;background:#111;color:#eee;"
               "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
               "main{text-align:center}h1{font-size:22px}p{color:#aaa}</style></head><body><main>"
               "<h1>" + title + "</h1><p>" + escapeHtml (detail)
               + "</p><p>Tu peux fermer cet onglet et revenir dans ton DAW.</p></main></body></html>";
    }

    void sendHttpResponse (juce::StreamingSocket& socket, int status, const juce::String& html)
    {
        const auto body = html.toStdString();
        const auto statusText = status == 200 ? "OK" : "Not Found";

        const auto head = juce::String ("HTTP/1.1 ") + juce::String (status) + " " + statusText + "\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "Content-Length: " + juce::String ((int) body.size()) + "\r\n"
                          "Connection: close\r\n\r\n";

        socket.write (head.toRawUTF8(), (int) head.getNumBytesAsUTF8());
        socket.write (body.data(), (int) body.size());
    }

    juce::String readRequestHead (juce::StreamingSocket& socket)
    {
        juce::MemoryOutputStream received;
        char buffer[2048];

        while (received.getDataSize() < 16384)
        {
            if (socket.waitUntilReady (true, 3000) != 1)
                break;

            const auto numRead = socket.read (buffer, (int) sizeof (buffer), false);

            if (numRead <= 0)
                break;

            received.write (buffer, (size_t) numRead);

            if (received.toString().contains ("\r\n\r\n"))
                break;
        }

        return received.toString();
    }
}

OAuthClient::OAuthClient (Settings& s, TokenStore& t, HttpTransport httpTransport)
    : settings (s), tokenStore (t), transport (std::move (httpTransport))
{
}

bool OAuthClient::isSignedIn() const
{
    return tokenStore.loadRefreshToken().isNotEmpty();
}

juce::Result OAuthClient::signIn (const std::function<bool()>& shouldAbort)
{
    if (! settings.hasGoogleClient())
        return juce::Result::fail ("Importe d'abord ton fichier client_secret.json Google dans les réglages."_t);

    juce::StreamingSocket listener;

    if (! listener.createListener (0, "127.0.0.1"))
        return juce::Result::fail ("Impossible d'ouvrir un port local pour la connexion Google.");

    const auto redirectUri = "http://127.0.0.1:" + juce::String (listener.getBoundPort());
    const auto verifier = OAuth::makeCodeVerifier();
    const auto state = OAuth::makeState();
    const auto authUrl = OAuth::buildAuthorizationUrl (settings.getGoogleClientId(), redirectUri,
                                                       OAuth::makeCodeChallenge (verifier), state);

    if (! juce::Process::openDocument (authUrl, {}))
        return juce::Result::fail ("Impossible d'ouvrir le navigateur.");

    const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) signInTimeoutMs;

    while (juce::Time::getMillisecondCounter() < deadline)
    {
        if (shouldAbort != nullptr && shouldAbort())
            return juce::Result::fail ("Connexion annulée."_t);

        if (listener.waitUntilReady (true, 250) != 1)
            continue;

        std::unique_ptr<juce::StreamingSocket> connection (listener.waitForNextConnection());

        if (connection == nullptr)
            continue;

        const auto redirect = OAuth::parseRedirectRequest (readRequestHead (*connection));

        if (! redirect.isOAuthCallback())
        {
            // favicon.ico and other browser noise
            sendHttpResponse (*connection, 404, makeHtmlPage (false, "Page inconnue."));
            continue;
        }

        if (redirect.error.isNotEmpty())
        {
            sendHttpResponse (*connection, 200, makeHtmlPage (false, "Google a répondu : "_t + redirect.error));
            return juce::Result::fail (redirect.error == "access_denied" ? juce::String ("Autorisation refusée sur la page Google."_t)
                                                                         : "Erreur Google : " + redirect.error);
        }

        if (redirect.state != state)
        {
            sendHttpResponse (*connection, 200, makeHtmlPage (false, "Réponse inattendue (state invalide)."_t));
            return juce::Result::fail ("Réponse OAuth invalide (state différent). Réessaie."_t);
        }

        const auto result = exchangeCode (redirect.code, verifier, redirectUri);
        sendHttpResponse (*connection, 200, makeHtmlPage (result.wasOk(), result.wasOk() ? juce::String ("Tout est prêt."_t)
                                                                                         : result.getErrorMessage()));
        return result;
    }

    return juce::Result::fail ("Délai dépassé : la page Google n'a pas été validée."_t);
}

static HttpRequest makeTokenRequest (const juce::StringPairArray& fields)
{
    HttpRequest request;
    request.url = juce::URL (OAuth::tokenEndpoint);
    request.method = "POST";
    request.headers.set ("Content-Type", "application/x-www-form-urlencoded");

    const auto body = formEncode (fields);
    request.body.append (body.toRawUTF8(), body.getNumBytesAsUTF8());
    return request;
}

juce::Result OAuthClient::exchangeCode (const juce::String& code, const juce::String& verifier, const juce::String& redirectUri)
{
    juce::StringPairArray fields;
    fields.set ("code", code);
    fields.set ("client_id", settings.getGoogleClientId());
    fields.set ("client_secret", settings.getGoogleClientSecret());
    fields.set ("redirect_uri", redirectUri);
    fields.set ("grant_type", "authorization_code");
    fields.set ("code_verifier", verifier);

    const auto response = transport (makeTokenRequest (fields));

    if (! response.isSuccess())
        return juce::Result::fail ("Échange du code refusé : "_t + describeTokenError (response));

    if (response.json().getProperty ("refresh_token", {}).toString().isEmpty())
        return juce::Result::fail ("Google n'a pas renvoyé de refresh token. Retire l'accès BeatDrop sur "_t
                                   "myaccount.google.com/permissions puis reconnecte-toi.");

    return storeTokenResponse (response);
}

juce::Result OAuthClient::storeTokenResponse (const HttpResponse& response)
{
    const auto json = response.json();
    const auto accessToken = json.getProperty ("access_token", {}).toString();
    const auto expiresIn = (int) json.getProperty ("expires_in", 3600);
    const auto refreshToken = json.getProperty ("refresh_token", {}).toString();

    if (accessToken.isEmpty())
        return juce::Result::fail ("Réponse de Google sans access_token."_t);

    if (refreshToken.isNotEmpty() && ! tokenStore.saveRefreshToken (refreshToken))
        return juce::Result::fail ("Impossible d'enregistrer la session (Trousseau inaccessible).");

    const juce::ScopedLock sl (lock);
    cachedAccessToken = accessToken;
    accessTokenExpiry = juce::Time::getCurrentTime() + juce::RelativeTime::seconds (expiresIn);
    return juce::Result::ok();
}

juce::Result OAuthClient::getAccessToken (juce::String& accessToken)
{
    {
        const juce::ScopedLock sl (lock);

        if (cachedAccessToken.isNotEmpty()
            && accessTokenExpiry > juce::Time::getCurrentTime() + juce::RelativeTime::seconds (60))
        {
            accessToken = cachedAccessToken;
            return juce::Result::ok();
        }
    }

    const auto refreshToken = tokenStore.loadRefreshToken();

    if (refreshToken.isEmpty())
        return juce::Result::fail ("Connecte d'abord ta chaîne YouTube."_t);

    if (! settings.hasGoogleClient())
        return juce::Result::fail ("Identifiants Google manquants dans les réglages."_t);

    juce::StringPairArray fields;
    fields.set ("client_id", settings.getGoogleClientId());
    fields.set ("client_secret", settings.getGoogleClientSecret());
    fields.set ("refresh_token", refreshToken);
    fields.set ("grant_type", "refresh_token");

    const auto response = transport (makeTokenRequest (fields));

    if (response.status == 0)
        return juce::Result::fail ("Pas de connexion internet.");

    if (! response.isSuccess())
    {
        if (response.json().getProperty ("error", {}).toString() == "invalid_grant")
        {
            // Revoked, expired (7 days for apps in "Testing" mode) or password changed.
            tokenStore.clear();
            return juce::Result::fail ("Ta session YouTube a expiré. Reconnecte ta chaîne."_t);
        }

        return juce::Result::fail ("Rafraîchissement du jeton refusé : "_t + describeTokenError (response));
    }

    const auto result = storeTokenResponse (response);

    if (result.wasOk())
    {
        const juce::ScopedLock sl (lock);
        accessToken = cachedAccessToken;
    }

    return result;
}

void OAuthClient::invalidateAccessToken()
{
    const juce::ScopedLock sl (lock);
    cachedAccessToken.clear();
}

void OAuthClient::signOut()
{
    invalidateAccessToken();
    tokenStore.clear();
}

juce::String OAuthClient::describeTokenError (const HttpResponse& response)
{
    if (response.status == 0)
        return "pas de réponse du serveur."_t;

    const auto json = response.json();
    const auto error = json.getProperty ("error", {}).toString();
    const auto description = json.getProperty ("error_description", {}).toString();

    if (error.isNotEmpty())
        return error + (description.isNotEmpty() ? " (" + description + ")" : juce::String());

    return "HTTP " + juce::String (response.status);
}

} // namespace beatdrop
