#pragma once

#include "PluginProcessor.h"

class RgbBarLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void setThemeColours(juce::Colour bg, juce::Colour ink) noexcept
    {
        bgColour = bg;
        inkColour = ink;
    }

    void drawLinearSlider(juce::Graphics&,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          juce::Slider::SliderStyle,
                          juce::Slider&) override;

    void drawComboBox(juce::Graphics&,
                      int width, int height,
                      bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;

    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    void drawButtonBackground(juce::Graphics&,
                              juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics&,
                        juce::TextButton&,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override;

private:
    juce::Colour bgColour = juce::Colours::black;
    juce::Colour inkColour = juce::Colours::white;
};

class RgbSynthAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit RgbSynthAudioProcessorEditor(RgbSynthAudioProcessor&);
    ~RgbSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupBarSlider(juce::Slider& slider, bool clearsPresetName = true);
    void disableFocus(juce::Component& c);
    void applyThemeToWidgets();
    void refreshPresetList();
    void setSaveMode(bool enabled);
    void commitSavePreset();

    RgbSynthAudioProcessor& audioProcessor;
    RgbBarLookAndFeel themeLookAndFeel;

    juce::ComboBox presetBox;
    juce::TextButton saveButton;
    juce::TextButton deleteButton;

    juce::TextEditor nameEditor;
    juce::TextButton confirmSaveButton;
    juce::TextButton cancelSaveButton;

    juce::Slider lowSlider;
    juce::Slider midSlider;
    juce::Slider highSlider;
    juce::Slider releaseSlider;
    juce::Slider attackSlider;
    juce::Slider volumeSlider;

    juce::AudioProcessorValueTreeState::SliderAttachment lowAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment midAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment highAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment releaseAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment attackAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment volumeAttachment;

    juce::Colour currentBgColour = juce::Colours::black;
    juce::Colour currentInkColour = juce::Colours::white;
    juce::String trackedPresetName;
    bool isApplyingPreset = false;
    bool inSaveMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RgbSynthAudioProcessorEditor)
};
