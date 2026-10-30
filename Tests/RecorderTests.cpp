#include "Core/MasterRecorder.h"

using namespace beatdrop;

class RecorderTests : public juce::UnitTest
{
public:
    RecorderTests() : juce::UnitTest ("MasterRecorder", "BeatDrop") {}

    static constexpr double sampleRate = 48000.0;
    static constexpr int blockSize = 512;

    static void fillSine (juce::AudioBuffer<float>& buffer, float amplitude)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, amplitude * std::sin ((float) i * 0.05f));
    }

    Take waitForTake (MasterRecorder& recorder)
    {
        for (int i = 0; i < 500; ++i)
        {
            const auto take = recorder.getLastTake();

            if (take.isValid() && recorder.getState() != MasterRecorder::State::finalising)
                return take;

            juce::Thread::sleep (10);
        }

        return {};
    }

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("BeatDropTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));

        juce::AudioBuffer<float> silence (2, blockSize), loud (2, blockSize);
        silence.clear();
        fillSine (loud, 0.5f);

        beginTest ("trim length helper");
        expectEquals (MasterRecorder::computeTrimmedLength (-1, 1000, sampleRate), (juce::int64) 1000);
        expectEquals (MasterRecorder::computeTrimmedLength (99, 1000000, sampleRate), (juce::int64) (100 + 48000));
        expectEquals (MasterRecorder::computeTrimmedLength (999, 1000, sampleRate), (juce::int64) 1000);
        expectEquals (MasterRecorder::computeTrimmedLength (0, 0, sampleRate), (juce::int64) 0);

        beginTest ("playback mode: play -> stop, silence trimmed");
        {
            MasterRecorder recorder (dir);
            recorder.prepare (sampleRate, 2);
            recorder.setMode (CaptureMode::playback);
            recorder.setArmed (true);
            expect (recorder.getState() == MasterRecorder::State::armed);

            // Nothing is captured while stopped.
            for (int i = 0; i < 20; ++i)
                recorder.process (loud, false, false);

            expect (recorder.getState() == MasterRecorder::State::armed);

            // Leading silence is skipped, then 100 loud blocks, then 200 silent blocks.
            for (int i = 0; i < 10; ++i)  recorder.process (silence, true, false);
            for (int i = 0; i < 100; ++i) recorder.process (loud, true, false);
            for (int i = 0; i < 200; ++i) recorder.process (silence, true, false);

            expect (recorder.getState() == MasterRecorder::State::recording);

            recorder.process (silence, false, false); // transport stops
            recorder.stopNow();                       // runs the timer logic synchronously

            const auto take = waitForTake (recorder);
            expect (take.isValid());

            const auto expectedLength = (juce::int64) (100 * blockSize) + (juce::int64) sampleRate;
            expectEquals (take.lengthInSamples, expectedLength);

            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (take.file));
            expect (reader != nullptr);

            if (reader != nullptr)
            {
                expectEquals (reader->lengthInSamples, expectedLength);
                expectEquals ((int) reader->numChannels, 2);
                expectEquals (reader->sampleRate, sampleRate);
            }

            expect (recorder.isArmed()); // ready for the next take
        }

        beginTest ("playback mode ignores offline renders");
        {
            MasterRecorder recorder (dir);
            recorder.prepare (sampleRate, 2);
            recorder.setArmed (true);

            for (int i = 0; i < 50; ++i)
                recorder.process (loud, true, true);

            expect (recorder.getState() == MasterRecorder::State::armed);
        }

        beginTest ("bounce mode: records during the DAW export only");
        {
            MasterRecorder recorder (dir);
            recorder.prepare (sampleRate, 2);
            recorder.setMode (CaptureMode::bounce);
            recorder.setArmed (true);

            for (int i = 0; i < 50; ++i)
                recorder.process (loud, true, false); // normal playback: ignored

            expect (recorder.getState() == MasterRecorder::State::armed);

            recorder.setNonRealtime (true);

            for (int i = 0; i < 300; ++i)
                recorder.process (loud, false, true); // hosts don't always report "playing" while rendering

            expect (recorder.getState() == MasterRecorder::State::recording);

            recorder.setNonRealtime (false);
            recorder.process (silence, false, false);
            recorder.stopNow();

            const auto take = waitForTake (recorder);
            expect (take.isValid());
            expectEquals (take.lengthInSamples, (juce::int64) (300 * blockSize));
        }

        beginTest ("mono input is written as stereo");
        {
            MasterRecorder recorder (dir);
            recorder.prepare (sampleRate, 1);
            recorder.setArmed (true);

            juce::AudioBuffer<float> mono (1, blockSize);
            fillSine (mono, 0.5f);

            for (int i = 0; i < 20; ++i)
                recorder.process (mono, true, false);

            expect (recorder.getState() == MasterRecorder::State::recording,
                    "state " + juce::String ((int) recorder.getState()) + " after " + juce::String (recorder.getRecordedSeconds()));

            recorder.process (silence, false, false);
            recorder.stopNow();

            const auto take = waitForTake (recorder);
            expect (take.isValid());
            expectEquals (take.numChannels, 2);
        }

        beginTest ("disarming without audio leaves no files behind");
        {
            const auto emptyDir = dir.getChildFile ("empty");
            {
                MasterRecorder recorder (emptyDir);
                recorder.prepare (sampleRate, 2);
                recorder.setArmed (true);
                recorder.setArmed (false);
            }

            expectEquals (emptyDir.getNumberOfChildFiles (juce::File::findFiles, "*"), 0);
        }

        beginTest ("loading an existing audio file as the take");
        {
            MasterRecorder recorder (dir);
            const auto existing = recorder.getTakesDirectory().findChildFiles (juce::File::findFiles, false, "Take*.wav");
            expect (! existing.isEmpty());

            if (! existing.isEmpty())
            {
                expect (recorder.loadTakeFromFile (existing.getFirst()).wasOk());
                expect (recorder.getLastTake().isValid());
            }

            expect (recorder.loadTakeFromFile (dir.getChildFile ("missing.wav")).failed());
        }

        dir.deleteRecursively();
    }
};

static RecorderTests recorderTests;
