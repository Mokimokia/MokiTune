#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class MokiTuneLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MokiTuneLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

class MokiTuneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit MokiTuneAudioProcessorEditor (MokiTuneAudioProcessor&);
    ~MokiTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void styleSlider (juce::Slider&, const juce::String& suffix = {});
    void styleLabel (juce::Label&, const juce::String&, int fontSize = 11);

    MokiTuneAudioProcessor& processor;
    MokiTuneLookAndFeel customLookAndFeel;

    juce::ComboBox keyBox;
    juce::ComboBox scaleBox;
    juce::Label keyLabel;
    juce::Label scaleLabel;
    juce::Label pitchReadout;
    juce::Label amountLabel;
    juce::Label speedLabel;
    juce::Label mixLabel;
    juce::Label footerLabel;

    juce::Slider amountSlider;
    juce::Slider speedSlider;
    juce::Slider mixSlider;
    juce::ToggleButton bypassButton { "BYPASS" };

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ComboAttachment> keyAttachment;
    std::unique_ptr<ComboAttachment> scaleAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> speedAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MokiTuneAudioProcessorEditor)
};
