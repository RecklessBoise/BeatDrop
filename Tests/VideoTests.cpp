#include "Video/VideoEncoder.h"
#include "Video/VideoFrame.h"

#include <juce_audio_formats/juce_audio_formats.h>

using namespace beatdrop;

class VideoTests : public juce::UnitTest
{
public:
    VideoTests() : juce::UnitTest ("Video", "BeatDrop") {}

    static juce::File writeTestWav (const juce::File& file, double seconds, double sampleRate)
    {
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                      .withSampleRate (sampleRate)
                                                      .withNumChannels (2)
                                                      .withBitsPerSample (24));

        juce::AudioBuffer<float> buffer (2, (int) (seconds * sampleRate));

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sampleRate));

        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        return file;
    }

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("BeatDropTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        dir.createDirectory();

        // A square cover: the frame must letterbox it rather than stretch it.
        juce::Image cover (juce::Image::RGB, 800, 800, true);
        {
            juce::Graphics g (cover);
            g.fillAll (juce::Colours::red);
        }

        beginTest ("frame is 16:9 with the cover centred");
        {
            const auto frame = VideoFrame::renderFrame (cover, VideoFrame::videoWidth, VideoFrame::videoHeight);
            expectEquals (frame.getWidth(), 1920);
            expectEquals (frame.getHeight(), 1080);

            const auto centre = frame.getPixelAt (960, 540);
            expect (centre.getRed() > 200 && centre.getGreen() < 50);

            const auto edge = frame.getPixelAt (5, 540); // blurred, darkened backdrop
            expect (edge.getRed() < centre.getRed());
        }

        beginTest ("thumbnail is a JPEG under 2 MB");
        {
            const auto jpeg = VideoFrame::makeThumbnailJpeg (cover);
            expect (! jpeg.isEmpty());
            expect (jpeg.getSize() <= VideoFrame::maxThumbnailBytes);
            expect (jpeg.getSize() > 2 && static_cast<const juce::uint8*> (jpeg.getData())[0] == 0xff
                                       && static_cast<const juce::uint8*> (jpeg.getData())[1] == 0xd8);
        }

        if (isVideoEncodingSupported())
        {
            beginTest ("MP4 encoding of a still image + WAV");

            const auto wav = writeTestWav (dir.getChildFile ("beat.wav"), 3.0, 44100.0);
            const auto mp4 = dir.getChildFile ("beat.mp4");

            VideoEncodeRequest request;
            request.audioFile = wav;
            request.frame = VideoFrame::renderFrame (cover, 1280, 720);
            request.outputFile = mp4;

            double lastProgress = 0.0;
            const auto result = encodeStillImageVideo (request, [&] (double p) { lastProgress = p; return true; });

            expect (result.wasOk(), result.getErrorMessage());
            expect (mp4.existsAsFile());
            expect (mp4.getSize() > 10000);
            expectWithinAbsoluteError (lastProgress, 1.0, 0.001);

            // CoreAudio can read the AAC track back out of the MP4.
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();

            if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (mp4)); reader != nullptr)
                expectWithinAbsoluteError ((double) reader->lengthInSamples / reader->sampleRate, 3.0, 0.2);

            beginTest ("MP4 encoding can be cancelled");
            const auto cancelled = encodeStillImageVideo (request, [] (double) { return false; });
            expect (cancelled.failed());
            expect (! mp4.existsAsFile());
        }

        dir.deleteRecursively();
    }
};

static VideoTests videoTests;
