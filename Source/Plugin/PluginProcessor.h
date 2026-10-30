#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Core/AppServices.h"
#include "Core/MasterRecorder.h"
#include "YouTube/UploadJob.h"

namespace StateIds
{
    #define BEATDROP_ID(name) inline const juce::Identifier name { #name };
    BEATDROP_ID (BeatDropState)
    BEATDROP_ID (beatName)
    BEATDROP_ID (artist)
    BEATDROP_ID (bpm)
    BEATDROP_ID (key)
    BEATDROP_ID (title)
    BEATDROP_ID (titleEdited)
    BEATDROP_ID (description)
    BEATDROP_ID (descriptionEdited)
    BEATDROP_ID (tags)
    BEATDROP_ID (tagsEdited)
    BEATDROP_ID (privacy)
    BEATDROP_ID (publishAt)
    BEATDROP_ID (imagePath)
    BEATDROP_ID (captureMode)
    BEATDROP_ID (armed)
    BEATDROP_ID (lastTakePath)
    #undef BEATDROP_ID
}

/**
    Pass-through effect for the end of the master chain. It records the master
    (while playing or while the DAW exports) and hands the take to the uploader.
*/
class BeatDropProcessor : public juce::AudioProcessor,
                          private beatdrop::MasterRecorder::Listener
{
public:
    BeatDropProcessor();
    ~BeatDropProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void setNonRealtime (bool isNonRealtime) noexcept override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return JucePlugin_Name; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    beatdrop::AppServices& getServices()                 { return *services; }
    beatdrop::MasterRecorder& getRecorder()              { return recorder; }
    beatdrop::UploadController& getUploads()             { return uploads; }

    /** Beat metadata typed in the UI; saved with the DAW project. Message thread only. */
    juce::ValueTree& getState()                          { return state; }

    double getHostBpm() const noexcept                   { return hostBpm.load(); }

private:
    void takeFinished (const beatdrop::Take&) override;

    juce::SharedResourcePointer<beatdrop::AppServices> services;
    beatdrop::MasterRecorder recorder { beatdrop::Settings::getOutputDirectory().getChildFile ("Takes") };
    beatdrop::UploadController uploads { services->youtube };

    juce::ValueTree state { StateIds::BeatDropState };
    std::atomic<double> hostBpm { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatDropProcessor)
};
