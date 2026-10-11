#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

namespace
{
    constexpr int analysisSize = 1024;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    const juce::StringArray keyNames { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };

    bool pitchClassBelongsToScale (int pitchClass, int root, int scale)
    {
        if (scale == 0) // Chromatic
            return true;

        // Scale 1 = major, scale 2 = natural minor.
        static constexpr std::array<int, 7> majorNotes { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr std::array<int, 7> minorNotes { 0, 2, 3, 5, 7, 8, 10 };
        const int relative = (pitchClass - root + 12) % 12;
        const auto& notes = (scale == 1 ? majorNotes : minorNotes);

        for (const auto note : notes)
            if (note == relative)
                return true;

        return false;
    }
}

MokiTuneAudioProcessor::MokiTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, juce::Identifier ("MokiTuneState"), createParameterLayout())
{
    keyParameter = parameters.getRawParameterValue ("KEY");
    scaleParameter = parameters.getRawParameterValue ("SCALE");
    amountParameter = parameters.getRawParameterValue ("AMOUNT");
    speedParameter = parameters.getRawParameterValue ("SPEED");
    mixParameter = parameters.getRawParameterValue ("MIX");
    bypassParameter = parameters.getRawParameterValue ("BYPASS");
}

juce::AudioProcessorValueTreeState::ParameterLayout MokiTuneAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "KEY", 1 }, "Key", keyNames, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "SCALE", 1 }, "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "AMOUNT", 1 }, "Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "SPEED", 1 }, "Retune Speed",
        juce::NormalisableRange<float> (1.0f, 100.0f, 0.1f), 8.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "MIX", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "BYPASS", 1 }, "Bypass", false));

    return layout;
}

void MokiTuneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    decimationFactor = juce::jmax (2, static_cast<int> (std::ceil (sampleRate / 24000.0)));

    centreDelaySamples = static_cast<float> (sampleRate * 0.020);
    modulationRangeSamples = juce::jmax (16.0f, static_cast<float> (sampleRate * 0.012));
    setLatencySamples (juce::roundToInt (centreDelaySamples));

    delayLineSize = juce::nextPowerOfTwo (juce::jmax (16384, static_cast<int> (sampleRate * 0.08)));
    for (auto& line : delayLines)
        line.assign (static_cast<size_t> (delayLineSize), 0.0f);

    analysisRing.fill (0.0f);
    analysisWritePosition = 0;
    analysisFilledSamples = 0;
    analysisSamplesSincePitchUpdate = 0;
    decimationCounter = 0;
    decimationSum = 0.0f;
    delayWritePosition = 0;
    shifterPhase = 0.0f;
    targetPitchRatio = 1.0f;
    smoothedPitchRatio = 1.0f;
    detectedFrequencyHz.store (0.0f);
    displayMidiNote.store (-1);
}

void MokiTuneAudioProcessor::releaseResources()
{
}

bool MokiTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    const auto input = layouts.getMainInputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && output == input;
}

float MokiTuneAudioProcessor::readDelayLine (int channel, float delaySamples) const noexcept
{
    const auto& line = delayLines[static_cast<size_t> (channel)];
    float readPosition = static_cast<float> (delayWritePosition) - delaySamples;
    if (readPosition < 0.0f)
        readPosition += static_cast<float> (delayLineSize);
    if (readPosition >= static_cast<float> (delayLineSize))
        readPosition -= static_cast<float> (delayLineSize);

    const int index0 = juce::jlimit (0, delayLineSize - 1, static_cast<int> (readPosition));
    const int index1 = (index0 + 1) % delayLineSize;
    const float fraction = readPosition - static_cast<float> (index0);
    return line[static_cast<size_t> (index0)]
         + fraction * (line[static_cast<size_t> (index1)] - line[static_cast<size_t> (index0)]);
}

bool MokiTuneAudioProcessor::estimateFundamental (float& frequencyHz) const
{
    std::array<float, analysisSize> frame {};
    for (int i = 0; i < analysisSize; ++i)
        frame[static_cast<size_t> (i)] = analysisRing[static_cast<size_t> ((analysisWritePosition + i) % analysisSize)];

    float energy = 0.0f;
    for (const auto sample : frame)
        energy += sample * sample;

    const float rms = std::sqrt (energy / static_cast<float> (analysisSize));
    if (rms < 0.0045f)
        return false;

    const float analysisRate = static_cast<float> (currentSampleRate / static_cast<double> (decimationFactor));
    const int minLag = juce::jmax (2, static_cast<int> (analysisRate / 1000.0f));
    const int maxLag = juce::jlimit (minLag + 2, analysisSize / 2 - 2,
                                     static_cast<int> (analysisRate / 65.0f));
    const int windowLength = analysisSize - maxLag - 1;

    std::array<float, analysisSize / 2> difference {};
    std::array<float, analysisSize / 2> cumulativeMeanDifference {};
    float runningSum = 0.0f;

    for (int lag = 1; lag <= maxLag; ++lag)
    {
        float sum = 0.0f;
        for (int i = 0; i < windowLength; ++i)
        {
            const float delta = frame[static_cast<size_t> (i)]
                              - frame[static_cast<size_t> (i + lag)];
            sum += delta * delta;
        }
        difference[static_cast<size_t> (lag)] = sum;
        runningSum += sum;
        cumulativeMeanDifference[static_cast<size_t> (lag)] =
            runningSum > 1.0e-12f ? sum * static_cast<float> (lag) / runningSum : 1.0f;
    }

    int selectedLag = -1;
    constexpr float yinThreshold = 0.15f;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        if (cumulativeMeanDifference[static_cast<size_t> (lag)] < yinThreshold)
        {
            while (lag + 1 <= maxLag
                   && cumulativeMeanDifference[static_cast<size_t> (lag + 1)]
                      < cumulativeMeanDifference[static_cast<size_t> (lag)])
                ++lag;
            selectedLag = lag;
            break;
        }
    }

    if (selectedLag < 0)
    {
        float bestValue = 1.0f;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            const float value = cumulativeMeanDifference[static_cast<size_t> (lag)];
            if (value < bestValue)
            {
                bestValue = value;
                selectedLag = lag;
            }
        }
        if (selectedLag < 0 || bestValue > 0.35f)
            return false;
    }

    float refinedLag = static_cast<float> (selectedLag);
    if (selectedLag > 1 && selectedLag < maxLag)
    {
        const float left = cumulativeMeanDifference[static_cast<size_t> (selectedLag - 1)];
        const float centre = cumulativeMeanDifference[static_cast<size_t> (selectedLag)];
        const float right = cumulativeMeanDifference[static_cast<size_t> (selectedLag + 1)];
        const float denominator = left - 2.0f * centre + right;
        if (std::abs (denominator) > 1.0e-9f)
            refinedLag += juce::jlimit (-1.0f, 1.0f, 0.5f * (left - right) / denominator);
    }

    frequencyHz = analysisRate / refinedLag;
    return std::isfinite (frequencyHz) && frequencyHz >= 65.0f && frequencyHz <= 1000.0f;
}

int MokiTuneAudioProcessor::findNearestScaleNote (float midiNote, int key, int scale)
{
    const int rounded = juce::roundToInt (midiNote);
    if (scale == 0)
        return rounded;

    int bestNote = rounded;
    float bestDistance = std::numeric_limits<float>::max();
    for (int note = rounded - 7; note <= rounded + 7; ++note)
    {
        const int pitchClass = (note % 12 + 12) % 12;
        if (! pitchClassBelongsToScale (pitchClass, key, scale))
            continue;

        const float distance = std::abs (static_cast<float> (note) - midiNote);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestNote = note;
        }
    }
    return bestNote;
}

void MokiTuneAudioProcessor::updatePitchTarget (float amount, int key, int scale)
{
    float frequency = 0.0f;
    if (! estimateFundamental (frequency))
    {
        targetPitchRatio = 1.0f;
        detectedFrequencyHz.store (0.0f, std::memory_order_relaxed);
        displayMidiNote.store (-1, std::memory_order_relaxed);
        return;
    }

    const float midiNote = 69.0f + 12.0f * std::log2 (frequency / 440.0f);
    const int nearestNote = findNearestScaleNote (midiNote, key, scale);
    const float correctedMidi = midiNote + (static_cast<float> (nearestNote) - midiNote)
                                           * juce::jlimit (0.0f, 1.0f, amount);
    targetPitchRatio = std::exp2 ((correctedMidi - midiNote) / 12.0f);

    detectedFrequencyHz.store (frequency, std::memory_order_relaxed);
    displayMidiNote.store (nearestNote, std::memory_order_relaxed);
}

void MokiTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (2, getTotalNumInputChannels());
    if (numChannels <= 0)
        return;

    const float amount = amountParameter->load (std::memory_order_relaxed) * 0.01f;
    const float speedMs = speedParameter->load (std::memory_order_relaxed);
    const float wetMix = mixParameter->load (std::memory_order_relaxed) * 0.01f;
    const int key = juce::roundToInt (keyParameter->load (std::memory_order_relaxed));
    const int scale = juce::roundToInt (scaleParameter->load (std::memory_order_relaxed));
    const bool bypass = bypassParameter->load (std::memory_order_relaxed) >= 0.5f;
    const float smoothing = 1.0f - std::exp (-1.0f / (static_cast<float> (currentSampleRate)
                                                       * juce::jmax (0.001f, speedMs * 0.001f)));
    const float rangeSamples = juce::jmax (16.0f, modulationRangeSamples);

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        float mono = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float inputSample = buffer.getSample (channel, sampleIndex);
            mono += inputSample;
            delayLines[static_cast<size_t> (channel)][static_cast<size_t> (delayWritePosition)] = inputSample;
        }
        mono /= static_cast<float> (numChannels);

        decimationSum += mono;
        ++decimationCounter;
        if (decimationCounter >= decimationFactor)
        {
            analysisRing[static_cast<size_t> (analysisWritePosition)] = decimationSum / static_cast<float> (decimationFactor);
            analysisWritePosition = (analysisWritePosition + 1) % analysisSize;
            analysisFilledSamples = juce::jmin (analysisSize, analysisFilledSamples + 1);
            decimationSum = 0.0f;
            decimationCounter = 0;
            ++analysisSamplesSincePitchUpdate;

            if (analysisFilledSamples >= analysisSize && analysisSamplesSincePitchUpdate >= 256)
            {
                updatePitchTarget (amount, key, scale);
                analysisSamplesSincePitchUpdate = 0;
            }
        }

        smoothedPitchRatio += smoothing * (targetPitchRatio - smoothedPitchRatio);
        const float ratioDelta = smoothedPitchRatio - 1.0f;
        const float absoluteDelta = std::abs (ratioDelta);
        const float neutralBlend = juce::jlimit (0.0f, 1.0f, absoluteDelta / 0.01f);

        float shiftedPhase = shifterPhase + 0.5f;
        if (shiftedPhase >= 1.0f)
            shiftedPhase -= 1.0f;

        const float direction = ratioDelta >= 0.0f ? -1.0f : 1.0f;
        const float delayA = centreDelaySamples + direction * (shifterPhase - 0.5f) * rangeSamples;
        const float delayB = centreDelaySamples + direction * (shiftedPhase - 0.5f) * rangeSamples;
        const float weightA = 0.5f - 0.5f * std::cos (twoPi * shifterPhase);
        const float weightB = 1.0f - weightA;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = readDelayLine (channel, centreDelaySamples);
            float wet = dry;
            if (absoluteDelta > 0.0001f)
            {
                const float shifted = readDelayLine (channel, delayA) * weightA
                                    + readDelayLine (channel, delayB) * weightB;
                wet = dry + neutralBlend * (shifted - dry);
            }

            const float output = bypass ? dry : dry + wetMix * (wet - dry);
            buffer.setSample (channel, sampleIndex, output);
        }

        if (absoluteDelta > 0.0001f)
        {
            shifterPhase += absoluteDelta / rangeSamples;
            if (shifterPhase >= 1.0f)
                shifterPhase -= std::floor (shifterPhase);
        }

        ++delayWritePosition;
        if (delayWritePosition >= delayLineSize)
            delayWritePosition = 0;
    }

    for (int channel = numChannels; channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);
}

juce::String MokiTuneAudioProcessor::getPitchDisplayText() const
{
    const int midiNote = displayMidiNote.load (std::memory_order_relaxed);
    const float frequency = detectedFrequencyHz.load (std::memory_order_relaxed);
    if (midiNote < 0 || frequency <= 0.0f)
        return "WAITING FOR VOICE";

    const auto names = juce::StringArray { "C", "C#", "D", "D#", "E", "F",
                                           "F#", "G", "G#", "A", "A#", "B" };
    const int pitchClass = (midiNote % 12 + 12) % 12;
    const int octave = midiNote / 12 - 1;
    return names[pitchClass] + juce::String (octave) + "   /   "
         + juce::String (frequency, 1) + " Hz";
}

juce::AudioProcessorEditor* MokiTuneAudioProcessor::createEditor()
{
    return new MokiTuneAudioProcessorEditor (*this);
}

void MokiTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MokiTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (parameters.state.getType().toString()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MokiTuneAudioProcessor();
}
