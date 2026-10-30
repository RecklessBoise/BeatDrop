#include "Http.h"

namespace beatdrop
{

HttpResponse performHttpRequest (const HttpRequest& request)
{
    auto url = request.url;

    if (! request.body.isEmpty())
        url = url.withPOSTData (request.body);

    juce::String headerText;

    for (const auto& key : request.headers.getAllKeys())
        headerText << key << ": " << request.headers[key] << "\r\n";

    HttpResponse response;

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withHttpRequestCmd (request.method)
                             .withExtraHeaders (headerText)
                             .withConnectionTimeoutMs (request.timeoutMs)
                             .withStatusCode (&response.status)
                             .withResponseHeaders (&response.headers)
                             .withNumRedirectsToFollow (request.followRedirects ? 5 : 0)
                             .withProgressCallback (request.progress);

    if (auto stream = url.createInputStream (options))
        response.body = stream->readEntireStreamAsString();

    return response;
}

juce::String formEncode (const juce::StringPairArray& fields)
{
    juce::StringArray pairs;

    for (const auto& key : fields.getAllKeys())
        pairs.add (juce::URL::addEscapeChars (key, true) + "=" + juce::URL::addEscapeChars (fields[key], true));

    return pairs.joinIntoString ("&");
}

} // namespace beatdrop
