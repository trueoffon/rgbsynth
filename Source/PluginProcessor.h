#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

class RgbSynthAudioProcessor final : public juce::AudioProcessor
{
public:
    RgbSynthAudioProcessor();
    ~RgbSynthAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.6; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Управление пользовательскими звуками
    juce::File getPresetsFolder() const;
    juce::StringArray getAllPresetNames() const;
    bool savePreset(const juce::String& name);
    bool loadPreset(const juce::String& name);
    bool deletePreset(const juce::String& name);

    const juce::String& getCurrentPresetName() const noexcept { return currentPresetName; }
    void clearCurrentPresetName() noexcept { currentPresetName.clear(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::Synthesiser synth;
    juce::MidiBuffer sortedMidi;
    juce::String currentPresetName;

    float smoothedVolume = 0.64f;
    float volumeSmoothCoeff = 0.01f;
    float dcPrevIn[2]{ 0.0f, 0.0f };
    float dcPrevOut[2]{ 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RgbSynthAudioProcessor)
};
