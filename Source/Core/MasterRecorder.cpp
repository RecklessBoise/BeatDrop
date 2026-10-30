#include "MasterRecorder.h"
#include "Core/Text.h"

namespace beatdrop
{

namespace
{
    constexpr juce::uint32 hostSilentTimeoutMs = 2000;
    constexpr int numWriterChannels = 2;
    constexpr int bitsPerSample = 24;

    bool compareExchange (std::atomic<int>& value, int expected, int desired) noexcept
    {
        return value.compare_exchange_strong (expected, desired);
    }

    juce::Result copyTrimmed (const juce::File& source, const juce::File& dest, juce::int64 numSamplesToKeep,
                              bool applyFadeOut, double sampleRate, int numChannels)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (source));

        if (reader == nullptr)
            return juce::Result::fail ("Impossible de relire l'enregistrement brut.");

        std::unique_ptr<juce::OutputStream> stream = dest.createOutputStream();

        if (stream == nullptr)
            return juce::Result::fail ("Impossible de créer "_t + dest.getFullPathName());

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                      .withSampleRate (sampleRate)
                                                      .withNumChannels (numChannels)
                                                      .withBitsPerSample (bitsPerSample));

        if (writer == nullptr)
            return juce::Result::fail ("Impossible d'écrire le fichier WAV."_t);

        const auto fadeLength = applyFadeOut ? juce::jmin ((juce::int64) (sampleRate * 0.02), numSamplesToKeep) : (juce::int64) 0;
        const auto fadeStart = numSamplesToKeep - fadeLength;

        juce::AudioBuffer<float> buffer (numChannels, 32768);

        for (juce::int64 pos = 0; pos < numSamplesToKeep;)
        {
            const auto numThisTime = (int) juce::jmin ((juce::int64) buffer.getNumSamples(), numSamplesToKeep - pos);
            reader->read (&buffer, 0, numThisTime, pos, true, true);

            if (fadeLength > 0 && pos + numThisTime > fadeStart)
            {
                for (int i = 0; i < numThisTime; ++i)
                {
                    const auto absolute = pos + i;

                    if (absolute >= fadeStart)
                    {
                        const auto gain = 1.0f - (float) (absolute - fadeStart) / (float) fadeLength;

                        for (int ch = 0; ch < numChannels; ++ch)
                            buffer.getWritePointer (ch)[i] *= gain;
                    }
                }
            }

            if (! writer->writeFromAudioSampleBuffer (buffer, 0, numThisTime))
                return juce::Result::fail ("Erreur d'écriture disque."_t);

            pos += numThisTime;
        }

        return juce::Result::ok();
    }
}

//==============================================================================
MasterRecorder::MasterRecorder (juce::File dir)
    : takesDirectory (std::move (dir))
{
    writerThread.startThread();
    startTimerHz (20);
}

MasterRecorder::~MasterRecorder()
{
    stopTimer();
    armed = false;

    {
        const juce::SpinLock::ScopedLockType lock (writerLock);
        threadedWriter.reset();
    }

    if (rawFile.existsAsFile())
        rawFile.deleteFile();

    finaliserPool.removeAllJobs (false, 30000);
}

//==============================================================================
void MasterRecorder::prepare (double sampleRate, int numChannels)
{
    const auto rateChanged = ! juce::approximatelyEqual (sampleRate, currentSampleRate.load());

    currentSampleRate = sampleRate;
    currentNumChannels = juce::jmax (1, numChannels);

    if (rateChanged)
    {
        // A writer prepared for the old rate would produce a wrongly-pitched file.
        compareExchange (audioState, audioWaitingForStart, audioIdle);
        compareExchange (audioState, audioRecording, audioFinished);

        if (audioState.load() == audioIdle)
        {
            {
                const juce::SpinLock::ScopedLockType lock (writerLock);
                threadedWriter.reset();
            }

            rawFile.deleteFile();

            if (juce::MessageManager::existsAndIsCurrentThread())
                createWriterIfNeeded();
        }
    }
}

void MasterRecorder::setNonRealtime (bool isNonRealtime) noexcept
{
    nonRealtimeFlag = isNonRealtime;
}

void MasterRecorder::process (const juce::AudioBuffer<float>& buffer, bool transportPlaying, bool nonRealtime) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    float peak = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

    peakLevel = peak;
    lastBlockMillis = juce::Time::getMillisecondCounter();

    if (! armed.load())
        return;

    const auto isRendering = nonRealtime || nonRealtimeFlag.load();
    const auto active = mode.load() == CaptureMode::playback ? (transportPlaying && ! isRendering)
                                                             : isRendering;

    auto state = audioState.load();

    if (state == audioFinished)
        return;

    if (state == audioIdle)
    {
        if (! active || ! compareExchange (audioState, audioIdle, audioWaitingForStart))
            return;

        state = audioWaitingForStart;
    }

    if (! active)
    {
        // Nothing heard yet: go back to waiting rather than saving an empty take.
        compareExchange (audioState, state, state == audioWaitingForStart ? audioIdle : audioFinished);
        return;
    }

    if (state == audioWaitingForStart)
    {
        if (peak < leadingSilenceThreshold)
            return;

        if (! compareExchange (audioState, audioWaitingForStart, audioRecording))
            return;
    }

    const juce::SpinLock::ScopedTryLockType lock (writerLock);

    if (! lock.isLocked() || threadedWriter == nullptr)
        return;

    const float* channels[numWriterChannels] = { buffer.getReadPointer (0),
                                                 buffer.getReadPointer (juce::jmin (1, numChannels - 1)) };

    auto written = threadedWriter->write (channels, numSamples);

    // Offline renders can run far faster than the disk; it's fine to wait there.
    for (int attempt = 0; ! written && isRendering && attempt < 2000; ++attempt)
    {
        juce::Thread::sleep (1);
        written = threadedWriter->write (channels, numSamples);
    }

    if (! written)
        return;

    const auto before = samplesWritten.load();

    if (peak >= trailingSilenceThreshold)
        lastLoudSample = before + numSamples - 1;

    samplesWritten = before + numSamples;

    if ((double) samplesWritten.load() > maxTakeSeconds * currentSampleRate.load())
        compareExchange (audioState, audioRecording, audioFinished);
}

//==============================================================================
void MasterRecorder::setMode (CaptureMode newMode)
{
    if (mode.exchange (newMode) != newMode)
    {
        // Switching modes mid-take ends the take rather than mixing two sources.
        compareExchange (audioState, audioWaitingForStart, audioIdle);
        compareExchange (audioState, audioRecording, audioFinished);
        notifyStateChanged();
    }
}

void MasterRecorder::setArmed (bool shouldBeArmed)
{
    armed = shouldBeArmed;

    if (shouldBeArmed)
    {
        createWriterIfNeeded();
    }
    else
    {
        compareExchange (audioState, audioWaitingForStart, audioIdle);
        compareExchange (audioState, audioRecording, audioFinished);

        if (audioState.load() == audioIdle)
        {
            {
                const juce::SpinLock::ScopedLockType lock (writerLock);
                threadedWriter.reset();
            }

            rawFile.deleteFile();
        }
    }

    notifyStateChanged();
}

void MasterRecorder::stopNow()
{
    compareExchange (audioState, audioWaitingForStart, audioIdle);
    compareExchange (audioState, audioRecording, audioFinished);
    timerCallback();
}

MasterRecorder::State MasterRecorder::getState() const noexcept
{
    if (finalising.load())
        return State::finalising;

    const auto state = audioState.load();

    if (state == audioRecording || state == audioFinished)
        return State::recording;

    return armed.load() ? State::armed : State::idle;
}

double MasterRecorder::getRecordedSeconds() const noexcept
{
    const auto state = audioState.load();

    if (state != audioRecording && state != audioFinished)
        return 0.0;

    return (double) samplesWritten.load() / currentSampleRate.load();
}

Take MasterRecorder::getLastTake() const
{
    const juce::ScopedLock sl (takeLock);
    return lastTake;
}

juce::Result MasterRecorder::loadTakeFromFile (const juce::File& audioFile)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));

    if (reader == nullptr)
        return juce::Result::fail ("Format audio non reconnu : " + audioFile.getFileName());

    Take take;
    take.file = audioFile;
    take.sampleRate = reader->sampleRate;
    take.lengthInSamples = reader->lengthInSamples;
    take.numChannels = (int) reader->numChannels;

    if (take.lengthInSamples <= 0)
        return juce::Result::fail ("Le fichier audio est vide.");

    {
        const juce::ScopedLock sl (takeLock);
        lastTake = take;
    }

    listeners.call ([&take] (Listener& l) { l.takeFinished (take); });
    return juce::Result::ok();
}

juce::int64 MasterRecorder::computeTrimmedLength (juce::int64 lastLoud, juce::int64 total, double sampleRate)
{
    if (total <= 0)
        return 0;

    if (lastLoud < 0)
        return total;

    const auto tail = (juce::int64) (tailSeconds * sampleRate);
    return juce::jlimit ((juce::int64) 1, total, lastLoud + 1 + tail);
}

//==============================================================================
void MasterRecorder::timerCallback()
{
    // Hosts such as Logic stop calling process() when playback stops, and some
    // never call setNonRealtime(false) after an export: treat silence from the
    // host as the end of the take.
    if (audioState.load() == audioRecording
        && juce::Time::getMillisecondCounter() - lastBlockMillis.load() > hostSilentTimeoutMs)
    {
        compareExchange (audioState, audioRecording, audioFinished);
    }

    if (audioState.load() == audioFinished && ! finalising.load())
        finishCurrentWriter();

    std::optional<Take> finished;

    {
        const juce::ScopedLock sl (takeLock);
        std::swap (finished, pendingFinishedTake);
    }

    if (finished.has_value())
        listeners.call ([&finished] (Listener& l) { l.takeFinished (*finished); });

    notifyStateChanged();
}

void MasterRecorder::createWriterIfNeeded()
{
    if (! armed.load())
        return;

    {
        const juce::SpinLock::ScopedLockType lock (writerLock);

        if (threadedWriter != nullptr)
            return;
    }

    if (! takesDirectory.createDirectory())
        return;

    rawFile = takesDirectory.getChildFile (".recording-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()) + ".wav");

    std::unique_ptr<juce::OutputStream> stream = rawFile.createOutputStream();

    if (stream == nullptr)
        return;

    const auto sampleRate = currentSampleRate.load();

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                  .withSampleRate (sampleRate)
                                                  .withNumChannels (numWriterChannels)
                                                  .withBitsPerSample (bitsPerSample));

    if (writer == nullptr)
        return;

    auto newWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread,
                                                                                (int) (sampleRate * 10.0));

    const juce::SpinLock::ScopedLockType lock (writerLock);
    threadedWriter = std::move (newWriter);
    samplesWritten = 0;
    lastLoudSample = -1;
}

void MasterRecorder::finishCurrentWriter()
{
    std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::int64 written = 0, lastLoud = -1;

    {
        const juce::SpinLock::ScopedLockType lock (writerLock);
        writer = std::move (threadedWriter);
        written = samplesWritten.load();
        lastLoud = lastLoudSample.load();
        samplesWritten = 0;
        lastLoudSample = -1;
    }

    const auto file = rawFile;
    rawFile = juce::File();
    audioState = audioIdle;

    if (writer == nullptr || written <= 0)
    {
        writer.reset();
        file.deleteFile();
        createWriterIfNeeded();
        return;
    }

    finalising = true;

    const auto sampleRate = currentSampleRate.load();
    const auto dest = takesDirectory.getChildFile ("Take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav")
                                    .getNonexistentSibling();

    // The job must hold the only reference: the raw file is only complete once the writer is destroyed.
    finaliserPool.addJob ([this, writer = std::move (writer), file, dest, written, lastLoud, sampleRate]() mutable
    {
        jassert (writer.use_count() == 1);
        writer.reset(); // flushes the FIFO and closes the raw file

        const auto keep = computeTrimmedLength (lastLoud, written, sampleRate);
        const auto result = copyTrimmed (file, dest, keep, keep < written, sampleRate, numWriterChannels);
        file.deleteFile();

        const juce::ScopedLock sl (takeLock);

        if (result.wasOk())
        {
            Take take;
            take.file = dest;
            take.sampleRate = sampleRate;
            take.lengthInSamples = keep;
            take.numChannels = numWriterChannels;

            lastTake = take;
            pendingFinishedTake = take;
        }

        finalising = false;
    });

    // Prepare the next take straight away so the user can play again.
    createWriterIfNeeded();
}

void MasterRecorder::notifyStateChanged()
{
    const auto state = getState();

    if (state != lastNotifiedState)
    {
        lastNotifiedState = state;
        listeners.call ([] (Listener& l) { l.recorderStateChanged(); });
    }
}

} // namespace beatdrop
