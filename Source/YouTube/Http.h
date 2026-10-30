#pragma once

#include <juce_core/juce_core.h>

namespace beatdrop
{

struct HttpRequest
{
    juce::URL url;
    juce::String method { "GET" };
    juce::StringPairArray headers;
    juce::MemoryBlock body;
    int timeoutMs = 30000;
    bool followRedirects = true;

    /** Upload progress; return false to abort the request. */
    std::function<bool (int bytesSent, int totalBytes)> progress;
};

struct HttpResponse
{
    int status = 0;                  // 0 = no response (network failure or aborted)
    juce::StringPairArray headers;   // case-insensitive keys
    juce::String body;

    bool isSuccess() const noexcept  { return status >= 200 && status < 300; }
    juce::var json() const           { return juce::JSON::parse (body); }
};

/** Blocking request; call from a background thread. */
using HttpTransport = std::function<HttpResponse (const HttpRequest&)>;

HttpResponse performHttpRequest (const HttpRequest& request);

/** application/x-www-form-urlencoded body. */
juce::String formEncode (const juce::StringPairArray& fields);

} // namespace beatdrop
