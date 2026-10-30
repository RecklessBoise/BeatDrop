#pragma once

#include <juce_core/juce_core.h>

namespace beatdrop
{

/** What the producer types about the beat; feeds the templates. */
struct BeatInfo
{
    juce::String beatName;   // e.g. "Midnight"
    juce::String artist;     // e.g. "Drake" -> "Drake Type Beat"
    juce::String bpm;
    juce::String key;
    juce::String producer;
};

/** YouTube privacy values accepted by the Data API. */
namespace Privacy
{
    inline const juce::String publicVideo { "public" };
    inline const juce::String unlisted    { "unlisted" };
    inline const juce::String privateVideo { "private" };
}

/** The final metadata sent with videos.insert. */
struct UploadMetadata
{
    juce::String title;
    juce::String description;
    juce::StringArray tags;
    juce::String privacyStatus { Privacy::publicVideo };
    juce::String publishAt;          // RFC 3339 UTC; when set the video is private until then
    juce::String categoryId { "10" }; // Music
    bool madeForKids = false;
};

namespace Metadata
{
    // YouTube limits (https://developers.google.com/youtube/v3/docs/videos)
    constexpr int maxTitleLength = 100;
    constexpr int maxDescriptionBytes = 5000;
    constexpr int maxTagsLength = 500;

    inline const juce::String defaultTitleTemplate { "[FREE] {artist} Type Beat - \"{name}\"" };
    inline const juce::String defaultDescriptionTemplate {
        "{artist} Type Beat - \"{name}\"\n"
        "Prod. by {producer}\n"
        "\n"
        "BPM: {bpm} | Key: {key}\n"
        "\n"
        "#{artisttag}typebeat #typebeat #{artisttag}" };

    /** Replaces {name} {artist} {artisttag} {bpm} {key} {producer} {year}
        and tidies up separators left dangling by empty values. */
    juce::String renderTemplate (const juce::String& templateText, const BeatInfo& info);

    /** Typical type-beat tags derived from the beat info. */
    juce::StringArray suggestTags (const BeatInfo& info);

    /** Splits "a, b,c" into tags. */
    juce::StringArray parseTagList (const juce::String& text);

    /** YouTube rejects '<' and '>' in titles and descriptions. */
    juce::String sanitiseTitle (const juce::String& title);
    juce::String sanitiseDescription (const juce::String& description);

    /** Drops tags until the YouTube length budget fits. */
    juce::StringArray sanitiseTags (const juce::StringArray& tags);

    /** Length as counted by YouTube: quoted when containing a space, comma separated. */
    int computeTagsLength (const juce::StringArray& tags);

    /** Parses "YYYY-MM-DD HH:MM" in local time and returns RFC 3339 UTC, or an empty string. */
    juce::String localDateTimeToRfc3339 (const juce::String& text);

    /** Builds the JSON body for videos.insert (part=snippet,status). */
    juce::var toVideoResource (const UploadMetadata& metadata);

    /** Checks everything YouTube would reject, so we fail before uploading. */
    juce::Result validate (const UploadMetadata& metadata);
}

} // namespace beatdrop
