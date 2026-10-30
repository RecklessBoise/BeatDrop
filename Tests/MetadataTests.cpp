#include "Core/Metadata.h"
#include "Core/Settings.h"

using namespace beatdrop;

class MetadataTests : public juce::UnitTest
{
public:
    MetadataTests() : juce::UnitTest ("Metadata", "BeatDrop") {}

    void runTest() override
    {
        const auto year = juce::String (juce::Time::getCurrentTime().getYear());

        BeatInfo full;
        full.beatName = "Midnight";
        full.artist = "Drake";
        full.bpm = "140";
        full.key = "C# min";
        full.producer = "Reckless";

        beginTest ("title template with every field");
        expectEquals (Metadata::renderTemplate (Metadata::defaultTitleTemplate, full),
                      juce::String ("[FREE] Drake Type Beat - \"Midnight\""));

        beginTest ("title template tidies empty fields");
        {
            BeatInfo noName = full;
            noName.beatName = {};
            expectEquals (Metadata::renderTemplate (Metadata::defaultTitleTemplate, noName),
                          juce::String ("[FREE] Drake Type Beat"));

            BeatInfo noArtist = full;
            noArtist.artist = {};
            expectEquals (Metadata::renderTemplate (Metadata::defaultTitleTemplate, noArtist),
                          juce::String ("[FREE] Type Beat - \"Midnight\""));
        }

        beginTest ("description template");
        {
            const auto description = Metadata::renderTemplate (Metadata::defaultDescriptionTemplate, full);
            expect (description.contains ("Prod. by Reckless"));
            expect (description.contains ("BPM: 140 | Key: C# min"));
            expect (description.contains ("#draketypebeat #typebeat #drake"));

            BeatInfo partial = full;
            partial.key = {};
            partial.artist = {};
            const auto partialDescription = Metadata::renderTemplate (Metadata::defaultDescriptionTemplate, partial);
            expect (partialDescription.contains ("BPM: 140"));
            expect (! partialDescription.contains ("Key:"));
            expect (! partialDescription.contains ("# "));
            expect (! partialDescription.endsWith ("#"));
        }

        beginTest ("year placeholder and bullet lists survive");
        expectEquals (Metadata::renderTemplate ("{name} {year}\n- Instagram: @me", full),
                      "Midnight " + year + "\n- Instagram: @me");

        beginTest ("tag suggestions");
        {
            const auto tags = Metadata::suggestTags (full);
            expect (tags.contains ("drake type beat"));
            expect (tags.contains ("drake type beat " + year));
            expect (tags.contains ("free drake type beat"));
            expect (tags.contains ("type beat"));
            expect (tags.contains ("midnight"));
        }

        beginTest ("tag list parsing and YouTube length budget");
        {
            expectEquals (Metadata::parseTagList ("a, b ,, c\nd, a").joinIntoString ("|"), juce::String ("a|b|c|d"));
            expectEquals (Metadata::computeTagsLength ({ "ab", "c d" }), 2 + 5 + 1);

            juce::StringArray many;

            for (int i = 0; i < 100; ++i)
                many.add ("tag number " + juce::String (i));

            const auto trimmed = Metadata::sanitiseTags (many);
            expect (Metadata::computeTagsLength (trimmed) <= Metadata::maxTagsLength);
            expect (trimmed.size() > 0);
            expectEquals (Metadata::sanitiseTags ({ "<bad>", "o,k" }).joinIntoString ("|"), juce::String ("bad|ok"));
        }

        beginTest ("title and description sanitising");
        {
            expectEquals (Metadata::sanitiseTitle ("  a <b>\nc  "), juce::String ("a b c"));
            expectEquals (Metadata::sanitiseTitle (juce::String::repeatedString ("x", 150)).length(), Metadata::maxTitleLength);

            const auto accented = juce::String::fromUTF8 ("\xc3\xa9"); // 2 bytes in UTF-8
            const auto longDescription = juce::String::repeatedString (accented, 4000);
            expect ((int) Metadata::sanitiseDescription (longDescription).getNumBytesAsUTF8() <= Metadata::maxDescriptionBytes);
        }

        beginTest ("local date to RFC 3339");
        {
            const auto rfc = Metadata::localDateTimeToRfc3339 ("2030-06-15 18:30");
            expect (rfc.endsWith ("Z"));
            expect (juce::Time::fromISO8601 (rfc) == juce::Time (2030, 5, 15, 18, 30, 0, 0, true));

            expect (Metadata::localDateTimeToRfc3339 ("2030-02-31 10:00").isEmpty());
            expect (Metadata::localDateTimeToRfc3339 ("demain").isEmpty());
            expect (Metadata::localDateTimeToRfc3339 ("2030-13-01 10:00").isEmpty());
            expect (Metadata::localDateTimeToRfc3339 ("2030-01-01T09:05").isNotEmpty());
        }

        beginTest ("video resource JSON");
        {
            UploadMetadata metadata;
            metadata.title = "T";
            metadata.description = "D";
            metadata.tags = { "x", "y" };
            metadata.privacyStatus = Privacy::unlisted;

            auto resource = Metadata::toVideoResource (metadata);
            expectEquals (resource["snippet"]["title"].toString(), juce::String ("T"));
            expectEquals (resource["snippet"]["categoryId"].toString(), juce::String ("10"));
            expectEquals (resource["snippet"]["tags"].size(), 2);
            expectEquals (resource["status"]["privacyStatus"].toString(), juce::String ("unlisted"));
            expect (! resource["status"].hasProperty ("publishAt"));

            metadata.publishAt = "2030-01-01T10:00:00Z";
            resource = Metadata::toVideoResource (metadata);
            expectEquals (resource["status"]["privacyStatus"].toString(), juce::String ("private"));
            expectEquals (resource["status"]["publishAt"].toString(), metadata.publishAt);
        }

        beginTest ("validation");
        {
            UploadMetadata metadata;
            expect (Metadata::validate (metadata).failed());

            metadata.title = "Ok";
            expect (Metadata::validate (metadata).wasOk());

            metadata.title = "a < b";
            expect (Metadata::validate (metadata).failed());

            metadata.title = "Ok";
            metadata.publishAt = "2001-01-01T00:00:00Z";
            expect (Metadata::validate (metadata).failed());

            metadata.publishAt = "2099-01-01T00:00:00Z";
            expect (Metadata::validate (metadata).wasOk());
        }

        beginTest ("Google client JSON");
        {
            juce::String id, secret;
            expect (Settings::parseGoogleClientJson (R"({"installed":{"client_id":"abc.apps.googleusercontent.com","client_secret":"s3cret"}})",
                                                     id, secret).wasOk());
            expectEquals (id, juce::String ("abc.apps.googleusercontent.com"));
            expectEquals (secret, juce::String ("s3cret"));

            expect (Settings::parseGoogleClientJson ("{}", id, secret).failed());
            expect (Settings::parseGoogleClientJson ("not json", id, secret).failed());
        }

        beginTest ("settings persist to disk");
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("BeatDropTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
            const auto file = dir.getChildFile ("test.settings");
            {
                Settings settings (file);
                expectEquals (settings.getTitleTemplate(), Metadata::defaultTitleTemplate);
                settings.setProducerName ("Reckless");
                settings.setGoogleClient ("id", "secret");
            }

            Settings reloaded (file);
            expectEquals (reloaded.getProducerName(), juce::String ("Reckless"));
            expect (reloaded.hasGoogleClient());
            dir.deleteRecursively();
        }
    }
};

static MetadataTests metadataTests;
