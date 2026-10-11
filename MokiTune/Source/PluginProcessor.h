#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class MokiTuneAudioProcessor final : public juce::AudioProcessor
{
public:
    MokiTuneAudioProcessor();
    ~MokiTuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    juce::String getPitchDisplayText() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    bool estimateFundamental (float& frequencyHz) const;
    void updatePitchTarget (float amount, int key, int scale);
    static int findNearestScaleNote (float midiNote, int key, int scale);
    float readDelayLine (int channel, float delaySamples) const noexcept;

    std::atomic<float>* keyParameter = nullptr;
    std::atomic<float>* scaleParameter = nullptr;
    std::atomic<float>* amountParameter = nullptr;
    std::atomic<float>* speedParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* bypassParameter = nullptr;

    double currentSampleRate = 48000.0;
    int decimationFactor = 2;
    int decimationCounter = 0;
    float decimationSum = 0.0f;
    int analysisSamplesSincePitchUpdate = 0;
    int analysisFilledSamples = 0;
    int analysisWritePosition = 0;
    std::array<float, 1024> analysisRing {};

    std::array<std::vector<float>, 2> delayLines;
    int delayLineSize = 16384;
    int delayWritePosition = 0;
    float centreDelaySamples = 960.0f;
    float modulationRangeSamples = 576.0f;
    float shifterPhase = 0.0f;

    float targetPitchRatio = 1.0f;
    float smoothedPitchRatio = 1.0f;
    std::atomic<float> detectedFrequencyHz { 0.0f };
    std::atomic<int> displayMidiNote { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MokiTuneAudioProcessor)
};
