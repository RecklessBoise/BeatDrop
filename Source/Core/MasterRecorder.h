#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

namespace beatdrop
{

/** How the recorder decides when a take starts and ends. */
enum class CaptureMode
{
    playback = 0, // records from transport start to transport stop (real-time)
    bounce   = 1  // records while the DAW renders/exports offline
};

/** A finished recording on disk. */
struct Take
{
    juce::File file;
    double sampleRate = 0.0;
    juce::int64 lengthInSamples = 0;
    int numChannels = 0;

    double getLengthSeconds() const noexcept   { return sampleRate > 0.0 ? (double) lengthInSamples / sampleRate : 0.0; }
    bool isValid() const                       { return lengthInSamples > 0 && file.existsAsFile(); }
};

/**
    Captures the master bus into a WAV file without ever blocking or allocating
    on the audio thread.

    The audio thread only pushes samples into an AudioFormatWriter::ThreadedWriter
    FIFO. Writer creation, take finalisation (silence trimming) and listener
    callbacks all happen on the message thread or a background thread.
*/
class MasterRecorder : private juce::Timer
{
public:
    enum class State
    {
        idle,       // not armed
        armed,      // waiting for the transport / an offline render
        recording,  // currently writing samples
        finalising  // trimming and saving the take
    };

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void recorderStateChanged() {}
        virtual void takeFinished (const Take&) {}
    };

    explicit MasterRecorder (juce::File takesDirectory);
    ~MasterRecorder() override;

    //==============================================================================
    /** Called from prepareToPlay. Must not run concurrently with process(). */
    void prepare (double sampleRate, int numChannels);

    /** Audio thread. */
    void process (const juce::AudioBuffer<float>& buffer, bool transportPlaying, bool nonRealtime) noexcept;

    /** Forwarded from AudioProcessor::setNonRealtime(). */
    void setNonRealtime (bool isNonRealtime) noexcept;

    //==============================================================================
    // Message thread
    void setMode (CaptureMode newMode);
    CaptureMode getMode() const noexcept                { return mode.load(); }

    void setArmed (bool shouldBeArmed);
    bool isArmed() const noexcept                       { return armed.load(); }

    /** Ends the current recording immediately (manual stop). */
    void stopNow();

    State getState() const noexcept;
    double getRecordedSeconds() const noexcept;
    float getPeakLevel() const noexcept                 { return peakLevel.load(); }

    Take getLastTake() const;

    /** Uses an existing audio file as the current take. */
    juce::Result loadTakeFromFile (const juce::File& audioFile);

    juce::File getTakesDirectory() const                { return takesDirectory; }

    void addListener (Listener* l)                      { listeners.add (l); }
    void removeListener (Listener* l)                   { listeners.remove (l); }

    //==============================================================================
    /** Pure helper, exposed for tests: number of samples to keep once trailing
        silence is removed (keeps a short tail after the last loud sample). */
    static juce::int64 computeTrimmedLength (juce::int64 lastLoudSample, juce::int64 totalSamples, double sampleRate);

    static constexpr float leadingSilenceThreshold  = 1.0e-4f;  // about -80 dBFS
    static constexpr float trailingSilenceThreshold = 1.0e-3f;  // about -60 dBFS
    static constexpr double tailSeconds = 1.0;
    static constexpr double maxTakeSeconds = 30.0 * 60.0;

private:
    enum AudioState : int
    {
        audioIdle,
        audioWaitingForStart,
        audioRecording,
        audioFinished
    };

    void timerCallback() override;
    void createWriterIfNeeded();
    void finishCurrentWriter();
    void notifyStateChanged();

    juce::File takesDirectory;
    juce::TimeSliceThread writerThread { "BeatDrop disk writer" };
    juce::ThreadPool finaliserPool { juce::ThreadPoolOptions{}.withThreadName ("BeatDrop finaliser").withNumberOfThreads (1) };

    juce::SpinLock writerLock;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::File rawFile;

    std::atomic<CaptureMode> mode { CaptureMode::playback };
    std::atomic<bool> armed { false };
    std::atomic<bool> nonRealtimeFlag { false };
    std::atomic<bool> stopRequested { false };
    std::atomic<int> audioState { audioIdle };
    std::atomic<bool> finalising { false };

    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentNumChannels { 2 };

    std::atomic<juce::int64> samplesWritten { 0 };
    std::atomic<juce::int64> lastLoudSample { -1 };
    std::atomic<juce::uint32> lastBlockMillis { 0 };
    std::atomic<float> peakLevel { 0.0f };

    mutable juce::CriticalSection takeLock;
    Take lastTake;
    std::optional<Take> pendingFinishedTake;

    State lastNotifiedState = State::idle;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterRecorder)
};

} // namespace beatdrop
