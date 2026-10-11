#include "PluginEditor.h"
#include <cmath>

namespace
{
    const juce::Colour backgroundColour { 0xff0b0b10 };
    const juce::Colour panelColour { 0xff14141d };
    const juce::Colour accentColour { 0xffff4fa3 };
    const juce::Colour mainTextColour { 0xfff4f1f5 };
    const juce::Colour mutedTextColour { 0xff858391 };
}

MokiTuneLookAndFeel::MokiTuneLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff20202c));
    setColour (juce::ComboBox::textColourId, mainTextColour);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff353544));
    setColour (juce::ComboBox::arrowColourId, accentColour);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1b27));
    setColour (juce::PopupMenu::textColourId, mainTextColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha (0.25f));
    setColour (juce::ToggleButton::textColourId, mainTextColour);
    setColour (juce::ToggleButton::tickColourId, accentColour);
    setColour (juce::ToggleButton::tickDisabledColourId, mutedTextColour);
}

void MokiTuneLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.46f;
    const float radius = juce::jmax (12.0f, juce::jmin (static_cast<float> (width),
                                                        static_cast<float> (height) - 14.0f) * 0.34f);
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colour (0xff252532));
    g.fillEllipse (centreX - radius - 3.0f, centreY - radius - 3.0f,
                   (radius + 3.0f) * 2.0f, (radius + 3.0f) * 2.0f);

    juce::Path track;
    track.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff343442));
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                          rotaryStartAngle, juce::jmax (rotaryStartAngle + 0.001f, angle), true);
    g.setColour (accentColour);
    g.strokePath (active, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    const float pointerLength = radius * 0.72f;
    const float pointerX = centreX + std::cos (angle - juce::MathConstants<float>::halfPi) * pointerLength;
    const float pointerY = centreY + std::sin (angle - juce::MathConstants<float>::halfPi) * pointerLength;
    g.setColour (mainTextColour);
    g.drawLine (centreX, centreY, pointerX, pointerY, 2.5f);
    g.fillEllipse (centreX - 3.5f, centreY - 3.5f, 7.0f, 7.0f);
}

MokiTuneAudioProcessorEditor::MokiTuneAudioProcessorEditor (MokiTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&customLookAndFeel);
    setSize (620, 420);
    setResizable (false, false);

    keyBox.addItemList (juce::StringArray { "C", "C#", "D", "D#", "E", "F",
                                            "F#", "G", "G#", "A", "A#", "B" }, 1);
    scaleBox.addItemList (juce::StringArray { "Chromatic", "Major", "Minor" }, 1);

    styleLabel (keyLabel, "KEY");
    styleLabel (scaleLabel, "SCALE");
    styleLabel (amountLabel, "CORRECTION", 12);
    styleLabel (speedLabel, "RETUNE SPEED", 12);
    styleLabel (mixLabel, "MIX", 12);
    styleLabel (pitchReadout, "WAITING FOR VOICE", 12);
    pitchReadout.setColour (juce::Label::textColourId, accentColour);
    pitchReadout.setJustificationType (juce::Justification::centredRight);
    styleLabel (footerLabel, "MONO LEAD VOCAL  •  LIVE PITCH TRACKING", 10);
    footerLabel.setColour (juce::Label::textColourId, mutedTextColour);

    styleSlider (amountSlider, "%");
    styleSlider (speedSlider, " ms");
    styleSlider (mixSlider, "%");

    bypassButton.setColour (juce::ToggleButton::textColourId, mutedTextColour);
    bypassButton.setColour (juce::ToggleButton::tickColourId, accentColour);
    bypassButton.setColour (juce::ToggleButton::tickDisabledColourId, mutedTextColour);
    bypassButton.setButtonText ("BYPASS");

    for (auto* component : std::array<juce::Component*, 10> {
             &keyBox, &scaleBox, &keyLabel, &scaleLabel, &pitchReadout,
             &amountLabel, &speedLabel, &mixLabel, &footerLabel, &bypassButton })
        addAndMakeVisible (component);

    addAndMakeVisible (amountSlider);
    addAndMakeVisible (speedSlider);
    addAndMakeVisible (mixSlider);

    keyAttachment = std::make_unique<ComboAttachment> (processor.parameters, "KEY", keyBox);
    scaleAttachment = std::make_unique<ComboAttachment> (processor.parameters, "SCALE", scaleBox);
    amountAttachment = std::make_unique<SliderAttachment> (processor.parameters, "AMOUNT", amountSlider);
    speedAttachment = std::make_unique<SliderAttachment> (processor.parameters, "SPEED", speedSlider);
    mixAttachment = std::make_unique<SliderAttachment> (processor.parameters, "MIX", mixSlider);
    bypassAttachment = std::make_unique<ButtonAttachment> (processor.parameters, "BYPASS", bypassButton);

    startTimerHz (12);
}

MokiTuneAudioProcessorEditor::~MokiTuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MokiTuneAudioProcessorEditor::styleSlider (juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20);
    slider.setTextValueSuffix (suffix);
    slider.setNumDecimalPlacesToDisplay (1);
    slider.setLookAndFeel (&customLookAndFeel);
    slider.setColour (juce::Slider::textBoxTextColourId, mainTextColour);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, panelColour);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff343442));
}

void MokiTuneAudioProcessorEditor::styleLabel (juce::Label& label, const juce::String& text, int fontSize)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::Font (static_cast<float> (fontSize), juce::Font::bold));
    label.setColour (juce::Label::textColourId, mainTextColour);
    label.setJustificationType (juce::Justification::centred);
}

void MokiTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    juce::ColourGradient gradient (juce::Colour (0xff171722), 0.0f, 0.0f,
                                   backgroundColour, 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (gradient);
    g.fillAll();

    g.setColour (accentColour);
    g.fillRoundedRectangle (24.0f, 25.0f, 4.0f, 33.0f, 2.0f);

    g.setColour (mainTextColour);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("MOKITUNE", 38, 21, 230, 30, juce::Justification::centredLeft);

    g.setColour (mutedTextColour);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("REAL-TIME VOCAL PITCH CORRECTION", 39, 52, 280, 17,
                juce::Justification::centredLeft);

    g.setColour (panelColour.withAlpha (0.97f));
    g.fillRoundedRectangle (20.0f, 91.0f, 580.0f, 284.0f, 16.0f);
    g.setColour (juce::Colour (0xff292936));
    g.drawRoundedRectangle (20.0f, 91.0f, 580.0f, 284.0f, 16.0f, 1.0f);

    g.setColour (juce::Colour (0xff242431));
    g.drawLine (39.0f, 151.0f, 581.0f, 151.0f, 1.0f);

    g.setColour (mutedTextColour);
    g.setFont (juce::Font (10.0f, juce::Font::plain));
    g.drawText ("AUDIO THREAD ACTIVE", 34, 389, 190, 16, juce::Justification::centredLeft);
    g.drawText ("~20 ms processing latency", 398, 389, 184, 16, juce::Justification::centredRight);
}

void MokiTuneAudioProcessorEditor::resized()
{
    keyLabel.setBounds (35, 105, 55, 16);
    keyBox.setBounds (35, 123, 88, 27);
    scaleLabel.setBounds (137, 105, 60, 16);
    scaleBox.setBounds (137, 123, 125, 27);
    pitchReadout.setBounds (330, 112, 248, 30);

    amountLabel.setBounds (50, 164, 145, 22);
    speedLabel.setBounds (237, 164, 145, 22);
    mixLabel.setBounds (425, 164, 145, 22);
    amountSlider.setBounds (47, 187, 151, 154);
    speedSlider.setBounds (234, 187, 151, 154);
    mixSlider.setBounds (422, 187, 151, 154);

    bypassButton.setBounds (35, 347, 105, 22);
    footerLabel.setBounds (150, 349, 420, 18);
}

void MokiTuneAudioProcessorEditor::timerCallback()
{
    pitchReadout.setText (processor.getPitchDisplayText(), juce::dontSendNotification);
}
