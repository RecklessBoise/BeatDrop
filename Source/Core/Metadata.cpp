#include "Metadata.h"
#include "Core/Text.h"

#include <ctime>

namespace beatdrop::Metadata
{

namespace
{
    juce::String collapseSpaces (const juce::String& text)
    {
        auto tokens = juce::StringArray::fromTokens (text, " \t", "");
        tokens.removeEmptyStrings();
        return tokens.joinIntoString (" ");
    }

    juce::String stripSeparators (juce::String line)
    {
        const juce::String separators (" -|,:/~");

        line = line.trim();

        while (line.isNotEmpty() && separators.containsChar (line.getLastCharacter()))
            line = line.dropLastCharacters (1).trimEnd();

        // Leading '-' is left alone so bullet lists in descriptions survive.
        while (line.isNotEmpty() && juce::String ("|,:/~").containsChar (line[0]))
            line = line.substring (1).trimStart();

        return line;
    }

    /** "BPM: 140 | Key: " -> "BPM: 140" */
    juce::String dropEmptyLabelledSegments (const juce::String& line)
    {
        if (! line.containsChar ('|'))
            return line.trim().endsWithChar (':') ? juce::String() : line;

        auto segments = juce::StringArray::fromTokens (line, "|", "");
        juce::StringArray kept;

        for (auto segment : segments)
        {
            segment = segment.trim();

            if (segment.isNotEmpty() && ! segment.endsWithChar (':'))
                kept.add (segment);
        }

        return kept.joinIntoString (" | ");
    }

    /** "#typebeat #" -> "#typebeat" */
    juce::String dropEmptyHashtags (const juce::String& line)
    {
        if (! line.containsChar ('#'))
            return line;

        auto tokens = juce::StringArray::fromTokens (line, " ", "");
        tokens.removeString ("#");
        return tokens.joinIntoString (" ");
    }

    juce::String makeArtistTag (const juce::String& artist)
    {
        return artist.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");
    }

    juce::String removeAngleBrackets (const juce::String& text)
    {
        return text.removeCharacters ("<>");
    }
}

juce::String renderTemplate (const juce::String& templateText, const BeatInfo& info)
{
    const auto year = juce::String (juce::Time::getCurrentTime().getYear());

    auto text = templateText.replace ("{name}", info.beatName.trim())
                            .replace ("{artisttag}", makeArtistTag (info.artist))
                            .replace ("{artist}", info.artist.trim())
                            .replace ("{bpm}", info.bpm.trim())
                            .replace ("{key}", info.key.trim())
                            .replace ("{producer}", info.producer.trim())
                            .replace ("{year}", year)
                            .replace ("\"\"", "")
                            .replace ("()", "")
                            .replace ("[]", "");

    juce::StringArray lines;
    lines.addLines (text);

    juce::StringArray result;

    for (const auto& line : lines)
    {
        const auto cleaned = stripSeparators (collapseSpaces (dropEmptyHashtags (dropEmptyLabelledSegments (line))));

        // Keep intentional blank lines, but drop lines that only held empty placeholders.
        if (cleaned.isNotEmpty() || line.trim().isEmpty())
            result.add (cleaned);
    }

    return result.joinIntoString ("\n").trim();
}

juce::StringArray suggestTags (const BeatInfo& info)
{
    const auto year = juce::String (juce::Time::getCurrentTime().getYear());
    const auto artist = info.artist.trim().toLowerCase();

    juce::StringArray tags;

    if (artist.isNotEmpty())
    {
        tags.add (artist + " type beat");
        tags.add (artist + " type beat " + year);
        tags.add ("free " + artist + " type beat");
        tags.add (artist + " instrumental");
        tags.add (artist);
    }

    if (info.beatName.trim().isNotEmpty())
        tags.add (info.beatName.trim().toLowerCase());

    tags.add ("type beat");
    tags.add ("type beat " + year);
    tags.add ("free type beat");
    tags.add ("instrumental");

    tags.removeDuplicates (true);
    return tags;
}

juce::StringArray parseTagList (const juce::String& text)
{
    auto tokens = juce::StringArray::fromTokens (text, ",\n", "");
    juce::StringArray tags;

    for (const auto& token : tokens)
    {
        const auto cleaned = collapseSpaces (token.trim());

        if (cleaned.isNotEmpty())
            tags.add (cleaned);
    }

    tags.removeDuplicates (true);
    return tags;
}

juce::String sanitiseTitle (const juce::String& title)
{
    auto cleaned = collapseSpaces (removeAngleBrackets (title).replaceCharacters ("\r\n\t", "   ")).trim();

    if (cleaned.length() > maxTitleLength)
        cleaned = cleaned.substring (0, maxTitleLength).trimEnd();

    return cleaned;
}

juce::String sanitiseDescription (const juce::String& description)
{
    auto cleaned = removeAngleBrackets (description).trim();

    while ((int) cleaned.getNumBytesAsUTF8() > maxDescriptionBytes)
        cleaned = cleaned.dropLastCharacters (juce::jmax (1, ((int) cleaned.getNumBytesAsUTF8() - maxDescriptionBytes) / 4));

    return cleaned;
}

int computeTagsLength (const juce::StringArray& tags)
{
    int total = 0;

    for (const auto& tag : tags)
        total += tag.length() + (tag.containsChar (' ') ? 2 : 0);

    return total + juce::jmax (0, tags.size() - 1);
}

juce::StringArray sanitiseTags (const juce::StringArray& tags)
{
    juce::StringArray cleaned;

    for (const auto& tag : tags)
    {
        const auto t = collapseSpaces (removeAngleBrackets (tag).removeCharacters (",\"").trim());

        if (t.isNotEmpty() && t.length() <= 100)
            cleaned.add (t);
    }

    cleaned.removeDuplicates (true);

    while (cleaned.size() > 0 && computeTagsLength (cleaned) > maxTagsLength)
        cleaned.remove (cleaned.size() - 1);

    return cleaned;
}

juce::String localDateTimeToRfc3339 (const juce::String& text)
{
    const auto trimmed = text.trim().replaceCharacter ('T', ' ');
    const auto datePart = trimmed.upToFirstOccurrenceOf (" ", false, false);
    const auto timePart = trimmed.fromFirstOccurrenceOf (" ", false, false).trim();

    auto dateFields = juce::StringArray::fromTokens (datePart, "-", "");
    auto timeFields = juce::StringArray::fromTokens (timePart, ":", "");

    if (dateFields.size() != 3 || timeFields.size() < 2
        || ! dateFields.joinIntoString ("").containsOnly ("0123456789")
        || ! timeFields.joinIntoString ("").containsOnly ("0123456789"))
        return {};

    const auto year = dateFields[0].getIntValue();
    const auto month = dateFields[1].getIntValue();
    const auto day = dateFields[2].getIntValue();
    const auto hours = timeFields[0].getIntValue();
    const auto minutes = timeFields[1].getIntValue();

    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31
        || hours < 0 || hours > 23 || minutes < 0 || minutes > 59)
        return {};

    const juce::Time local (year, month - 1, day, hours, minutes, 0, 0, true);

    // Rejects dates like 31 February, which juce::Time silently rolls over.
    if (local.getDayOfMonth() != day || local.getMonth() != month - 1)
        return {};

    const auto seconds = (std::time_t) (local.toMilliseconds() / 1000);
    std::tm utc {};

   #if JUCE_WINDOWS
    gmtime_s (&utc, &seconds);
   #else
    gmtime_r (&seconds, &utc);
   #endif

    char buffer[32] = {};
    std::strftime (buffer, sizeof (buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return juce::String (buffer);
}

juce::var toVideoResource (const UploadMetadata& metadata)
{
    auto* snippet = new juce::DynamicObject();
    snippet->setProperty ("title", metadata.title);
    snippet->setProperty ("description", metadata.description);
    snippet->setProperty ("categoryId", metadata.categoryId);

    juce::Array<juce::var> tagArray;

    for (const auto& tag : metadata.tags)
        tagArray.add (tag);

    snippet->setProperty ("tags", tagArray);

    auto* status = new juce::DynamicObject();
    status->setProperty ("selfDeclaredMadeForKids", metadata.madeForKids);

    if (metadata.publishAt.isNotEmpty())
    {
        // The API only schedules private videos; YouTube flips them public at publishAt.
        status->setProperty ("privacyStatus", Privacy::privateVideo);
        status->setProperty ("publishAt", metadata.publishAt);
    }
    else
    {
        status->setProperty ("privacyStatus", metadata.privacyStatus);
    }

    auto* resource = new juce::DynamicObject();
    resource->setProperty ("snippet", juce::var (snippet));
    resource->setProperty ("status", juce::var (status));
    return juce::var (resource);
}

juce::Result validate (const UploadMetadata& metadata)
{
    if (metadata.title.trim().isEmpty())
        return juce::Result::fail ("Le titre de la vidéo est vide."_t);

    if (metadata.title.length() > maxTitleLength)
        return juce::Result::fail ("Le titre dépasse 100 caractères."_t);

    if (metadata.title.containsAnyOf ("<>") || metadata.description.containsAnyOf ("<>"))
        return juce::Result::fail ("YouTube refuse les caractères < et > dans le titre et la description."_t);

    if ((int) metadata.description.getNumBytesAsUTF8() > maxDescriptionBytes)
        return juce::Result::fail ("La description dépasse 5000 octets."_t);

    if (computeTagsLength (metadata.tags) > maxTagsLength)
        return juce::Result::fail ("Les tags dépassent 500 caractères au total."_t);

    if (metadata.privacyStatus != Privacy::publicVideo
        && metadata.privacyStatus != Privacy::unlisted
        && metadata.privacyStatus != Privacy::privateVideo)
        return juce::Result::fail ("Visibilité inconnue : "_t + metadata.privacyStatus);

    if (metadata.publishAt.isNotEmpty())
    {
        const auto when = juce::Time::fromISO8601 (metadata.publishAt);

        if (when.toMilliseconds() == 0)
            return juce::Result::fail ("Date de publication invalide.");

        if (when <= juce::Time::getCurrentTime())
            return juce::Result::fail ("La date de publication programmée doit être dans le futur."_t);
    }

    return juce::Result::ok();
}

} // namespace beatdrop::Metadata
