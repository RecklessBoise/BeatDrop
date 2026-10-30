#include "PluginProcessor.h"
#include "PluginEditor.h"

BeatDropProcessor::BeatDropProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    recorder.addListener (this);
}

BeatDropProcessor::~BeatDropProcessor()
{
    recorder.removeListener (this);
}

void BeatDropProcessor::prepareToPlay (double sampleRate, int)
{
    recorder.prepare (sampleRate, getTotalNumInputChannels());
}

void BeatDropProcessor::releaseResources()
{
}

bool BeatDropProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void BeatDropProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    bool playing = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            playing = position->getIsPlaying();

            if (const auto bpm = position->getBpm())
                hostBpm = *bpm;
        }
    }

    // Audio passes through untouched; we only listen.
    recorder.process (buffer, playing, isNonRealtime());
}

void BeatDropProcessor::setNonRealtime (bool isNonRealtime) noexcept
{
    AudioProcessor::setNonRealtime (isNonRealtime);
    recorder.setNonRealtime (isNonRealtime);
}

juce::AudioProcessorEditor* BeatDropProcessor::createEditor()
{
    return new BeatDropEditor (*this);
}

//==============================================================================
void BeatDropProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.createCopy();
    copy.setProperty (StateIds::captureMode, (int) recorder.getMode(), nullptr);
    copy.setProperty (StateIds::armed, recorder.isArmed(), nullptr);

    if (const auto take = recorder.getLastTake(); take.isValid())
        copy.setProperty (StateIds::lastTakePath, take.file.getFullPathName(), nullptr);

    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void BeatDropProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (StateIds::BeatDropState.toString()))
        return;

    const auto restored = juce::ValueTree::fromXml (*xml);

    const auto apply = [this, restored]
    {
        state.copyPropertiesAndChildrenFrom (restored, nullptr);

        recorder.setMode ((int) restored.getProperty (StateIds::captureMode, 0) == 1 ? beatdrop::CaptureMode::bounce
                                                                                       : beatdrop::CaptureMode::playback);
        recorder.setArmed ((bool) restored.getProperty (StateIds::armed, false));

        const juce::File lastTake (restored.getProperty (StateIds::lastTakePath).toString());

        if (lastTake.existsAsFile())
            recorder.loadTakeFromFile (lastTake);
    };

    // Writer creation and listeners expect the message thread.
    if (juce::MessageManager::existsAndIsCurrentThread())
        apply();
    else
        juce::MessageManager::callAsync (apply);
}

void BeatDropProcessor::takeFinished (const beatdrop::Take& take)
{
    state.setProperty (StateIds::lastTakePath, take.file.getFullPathName(), nullptr);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeatDropProcessor();
}
