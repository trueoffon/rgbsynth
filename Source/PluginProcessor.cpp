#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kTwoPi = 2.0 * kPi;

    inline double polyBlep(double t, double dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    // Безусловно стабильный TPT Zero-Delay Feedback SVF фильтр
    struct TptSvf
    {
        double ic1eq = 0.0;
        double ic2eq = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double a3 = 0.0;
        double k = 0.707;

        void reset() noexcept
        {
            ic1eq = 0.0;
            ic2eq = 0.0;
        }

        void setParams(double cutoffHz, double q, double piOverSr, double maxCutoffHz) noexcept
        {
            const double clampedFc = std::clamp(cutoffHz, 20.0, maxCutoffHz);
            const double g = std::tan(piOverSr * clampedFc);
            k = 1.0 / std::max(0.5, q);
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        inline void process(double v0, double& lp, double& bp, double& hp) noexcept
        {
            const double v3 = v0 - ic2eq;
            const double v1 = a1 * ic1eq + a2 * v3;
            const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0 * v1 - ic1eq;
            ic2eq = 2.0 * v2 - ic2eq;
            lp = v2;
            bp = v1;
            hp = v0 - k * v1 - v2;
        }
    };

    struct RgbSound final : public juce::SynthesiserSound
    {
        bool appliesToNote(int) override { return true; }
        bool appliesToChannel(int) override { return true; }
    };

    class RgbVoice final : public juce::SynthesiserVoice
    {
    public:
        RgbVoice(std::atomic<float>* lowPtr,
                 std::atomic<float>* midPtr,
                 std::atomic<float>* highPtr,
                 std::atomic<float>* attackPtr,
                 std::atomic<float>* releasePtr)
            : lowParam(lowPtr),
              midParam(midPtr),
              highParam(highPtr),
              attackParam(attackPtr),
              releaseParam(releasePtr)
        {
        }

        bool canPlaySound(juce::SynthesiserSound* sound) override
        {
            return dynamic_cast<RgbSound*>(sound) != nullptr;
        }

        void startNote(int midiNoteNumber,
                       float velocity,
                       juce::SynthesiserSound*,
                       int currentPitchWheelPosition) override
        {
            baseFreq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
            level = static_cast<double>(velocity);
            updatePitchBend(currentPitchWheelPosition);

            subPhase = 0.0;
            fundPhase = 0.0;
            midPhases = { 0.0, 0.11, 0.23, 0.37, 0.51 };
            highPhaseL = 0.0;
            highPhaseR = 0.29;
            shimmerPhase = 0.0;

            punchEnv = 1.0;

            bassFilter1.reset();
            bassFilter2.reset();
            midFilterL.reset();
            midFilterR.reset();
            highFilterL.reset();
            highFilterR.reset();

            lowSmooth = juce::jlimit(0.0f, 1.0f, lowParam->load());
            midSmooth = juce::jlimit(0.0f, 1.0f, midParam->load());
            highSmooth = juce::jlimit(0.0f, 1.0f, highParam->load());

            adsr.reset();
            updateAdsrParameters(true);
            adsr.noteOn();
        }

        void stopNote(float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                adsr.noteOff();
            }
            else
            {
                clearCurrentNote();
                adsr.reset();
            }
        }

        void pitchWheelMoved(int newPitchWheelValue) override
        {
            updatePitchBend(newPitchWheelValue);
        }

        void controllerMoved(int, int) override {}

        void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                             int startSample,
                             int numSamples) override
        {
            if (!isVoiceActive() || numSamples <= 0)
                return;

            const double sr = getSampleRate();
            const int numCh = outputBuffer.getNumChannels();
            if (sr <= 0.0 || numCh <= 0)
                return;

            updateAdsrParameters(false);

            const float lowTarget = juce::jlimit(0.0f, 1.0f, lowParam->load());
            const float midTarget = juce::jlimit(0.0f, 1.0f, midParam->load());
            const float highTarget = juce::jlimit(0.0f, 1.0f, highParam->load());

            const double invSr = 1.0 / sr;
            const double piOverSr = kPi * invSr;
            const double maxFilterFc = 0.42 * sr;
            const double nyquistSafe = 0.45 * sr;
            const double paramCoeff = 1.0 - std::exp(-kTwoPi * 60.0 * invSr);
            const double punchDecay = std::exp(-55.0 * invSr);
            const bool isStereo = numCh > 1;

            constexpr std::array<double, 5> kMidDetune{ 0.9885, 0.9955, 1.0, 1.0045, 1.0115 };

            for (int i = 0; i < numSamples; ++i)
            {
                const float env = adsr.getNextSample();
                if (!adsr.isActive())
                {
                    clearCurrentNote();
                    break;
                }

                lowSmooth += static_cast<float>(paramCoeff) * (lowTarget - lowSmooth);
                midSmooth += static_cast<float>(paramCoeff) * (midTarget - midSmooth);
                highSmooth += static_cast<float>(paramCoeff) * (highTarget - highSmooth);

                punchEnv *= punchDecay;

                const double f = baseFreq * pitchBendRatio;
                const double lowAmt = static_cast<double>(lowSmooth);
                const double midAmt = static_cast<double>(midSmooth);
                const double highAmt = static_cast<double>(highSmooth);

                // Обновление коэффициентов TPT-фильтров на control-rate (раз в 16 сэмплов)
                if ((i & 15) == 0)
                {
                    const double bassFc = 145.0 + 360.0 * lowAmt + 260.0 * punchEnv * lowAmt;
                    bassFilter1.setParams(bassFc, 0.85, piOverSr, maxFilterFc);
                    bassFilter2.setParams(bassFc * 1.15, 0.78, piOverSr, maxFilterFc);

                    const double midFc = 280.0 + 2500.0 * midAmt;
                    const double midQ = 1.30 + 0.95 * midAmt;
                    midFilterL.setParams(midFc, midQ, piOverSr, maxFilterFc);
                    midFilterR.setParams(midFc, midQ, piOverSr, maxFilterFc);

                    const double highFc = std::max(720.0, f * 1.45);
                    highFilterL.setParams(highFc, 1.05, piOverSr, maxFilterFc);
                    highFilterR.setParams(highFc, 1.05, piOverSr, maxFilterFc);
                }

                // =========================================================
                // 1. НИЗ (Моно Саб-ядро + Питч-панч + Тёплое ламповое насыщение)
                // =========================================================
                double lowOut = 0.0;
                if (lowAmt > 0.0005)
                {
                    const double punchMul = 1.0 + 0.28 * lowAmt * punchEnv;
                    const double subFreq = ((f > 48.0) ? (0.5 * f) : f) * punchMul;
                    const double fundFreq = f * punchMul;

                    const double subDt = std::min(subFreq * invSr, 0.45);
                    const double fundDt = std::min(fundFreq * invSr, 0.45);

                    const double subSin = std::sin(kTwoPi * subPhase);
                    const double fundTri = 1.0 - 4.0 * std::abs(fundPhase - 0.5);
                    const double fundSaw = (fundFreq < nyquistSafe)
                                               ? (2.0 * fundPhase - 1.0 - polyBlep(fundPhase, fundDt))
                                               : 0.0;

                    const double bassRaw = 0.64 * subSin + 0.26 * fundTri + 0.22 * fundSaw;
                    const double drive = 1.05 + 1.65 * lowAmt;
                    const double bassSat = std::tanh(bassRaw * drive + 0.08 * bassRaw * bassRaw);

                    double lp1, bp1, hp1, lp2, bp2, hp2;
                    bassFilter1.process(bassSat, lp1, bp1, hp1);
                    bassFilter2.process(lp1, lp2, bp2, hp2);
                    lowOut = lp2 * lowAmt * 0.90;

                    subPhase += subDt;
                    if (subPhase >= 1.0) subPhase -= 1.0;
                    fundPhase += fundDt;
                    if (fundPhase >= 1.0) fundPhase -= 1.0;
                }

                // =========================================================
                // 2. СЕРЕДИНА (5-голосный стерео-гиперунисон + TPT резонанс)
                // =========================================================
                double midOutL = 0.0;
                double midOutR = 0.0;
                if (midAmt > 0.0005)
                {
                    std::array<double, 5> saws{};
                    for (size_t v = 0; v < 5; ++v)
                    {
                        const double vf = f * kMidDetune[v];
                        const double dt = std::min(vf * invSr, 0.45);
                        if (vf < nyquistSafe)
                            saws[v] = 2.0 * midPhases[v] - 1.0 - polyBlep(midPhases[v], dt);

                        midPhases[v] += dt;
                        if (midPhases[v] >= 1.0) midPhases[v] -= 1.0;
                    }

                    const double rawL = 0.36 * saws[0] + 0.34 * saws[1] + 0.30 * saws[2] + 0.14 * saws[3] + 0.10 * saws[4];
                    const double rawR = 0.10 * saws[0] + 0.14 * saws[1] + 0.30 * saws[2] + 0.34 * saws[3] + 0.36 * saws[4];

                    const double midDrive = 1.0 + 1.20 * midAmt;
                    const double satL = std::tanh(rawL * midDrive);
                    const double satR = std::tanh(rawR * midDrive);

                    double lpL, bpL, hpL, lpR, bpR, hpR;
                    midFilterL.process(satL, lpL, bpL, hpL);
                    midFilterR.process(satR, lpR, bpR, hpR);

                    midOutL = (0.62 * bpL + 0.45 * lpL) * midAmt * 0.56;
                    midOutR = (0.62 * bpR + 0.45 * lpR) * midAmt * 0.56;
                }

                // =========================================================
                // 3. ВЕРХ (2040 Crystal Hyper-Lead + FM-блеск + Air Sheen)
                // =========================================================
                double highOutL = 0.0;
                double highOutR = 0.0;
                if (highAmt > 0.0005)
                {
                    const double freqL = f * 2.0 * 0.9962;
                    const double freqR = f * 2.0 * 1.0038;
                    const double fmFreq = f * 3.0;

                    const double dtL = std::min(freqL * invSr, 0.45);
                    const double dtR = std::min(freqR * invSr, 0.45);
                    const double fmDt = std::min(fmFreq * invSr, 0.45);

                    const double fmMod = (fmFreq < nyquistSafe)
                                             ? std::sin(kTwoPi * shimmerPhase)
                                             : 0.0;

                    const double sawL = (freqL < nyquistSafe)
                                            ? (2.0 * highPhaseL - 1.0 - polyBlep(highPhaseL, dtL))
                                            : 0.0;
                    const double sawR = (freqR < nyquistSafe)
                                            ? (2.0 * highPhaseR - 1.0 - polyBlep(highPhaseR, dtR))
                                            : 0.0;

                    const double fmIndex = 0.45 + 0.95 * highAmt;
                    const double glassL = (freqL < nyquistSafe) ? std::sin(kTwoPi * highPhaseL + fmIndex * fmMod) : 0.0;
                    const double glassR = (freqR < nyquistSafe) ? std::sin(kTwoPi * highPhaseR + fmIndex * fmMod) : 0.0;

                    // Лёгкий воздушный призвук без блокировок
                    noiseState = noiseState * 1664525u + 1013904223u;
                    const double airNoise = static_cast<double>(static_cast<int32_t>(noiseState)) * (1.0 / 2147483648.0);

                    const double leadL = 0.55 * sawL + 0.45 * glassL + 0.03 * airNoise;
                    const double leadR = 0.55 * sawR + 0.45 * glassR - 0.03 * airNoise;

                    double lpL, bpL, hpL, lpR, bpR, hpR;
                    highFilterL.process(leadL, lpL, bpL, hpL);
                    highFilterR.process(leadR, lpR, bpR, hpR);

                    highOutL = (hpL + 0.18 * bpL) * highAmt * 0.46;
                    highOutR = (hpR + 0.18 * bpR) * highAmt * 0.46;

                    highPhaseL += dtL;
                    if (highPhaseL >= 1.0) highPhaseL -= 1.0;
                    highPhaseR += dtR;
                    if (highPhaseR >= 1.0) highPhaseR -= 1.0;
                    shimmerPhase += fmDt;
                    if (shimmerPhase >= 1.0) shimmerPhase -= 1.0;
                }

                const double gain = static_cast<double>(env) * level;
                const float sampleL = static_cast<float>((lowOut + midOutL + highOutL) * gain);
                const float sampleR = static_cast<float>((lowOut + midOutR + highOutR) * gain);

                const int sampleIdx = startSample + i;
                outputBuffer.addSample(0, sampleIdx, sampleL);
                if (isStereo)
                    outputBuffer.addSample(1, sampleIdx, sampleR);
            }
        }

    private:
        void updateAdsrParameters(bool force) noexcept
        {
            const float a = juce::jlimit(0.0f, 1.0f, attackParam->load());
            const float r = juce::jlimit(0.0f, 1.0f, releaseParam->load());

            if (!force && a == lastAttack && r == lastRelease)
                return;

            lastAttack = a;
            lastRelease = r;

            juce::ADSR::Parameters envParams;
            envParams.attack = 0.001f + std::pow(a, 2.5f) * 2.0f;
            envParams.decay = 0.16f;
            envParams.sustain = 0.84f;
            envParams.release = 0.008f + std::pow(r, 2.5f) * 3.5f;

            if (getSampleRate() > 0.0)
                adsr.setSampleRate(getSampleRate());

            adsr.setParameters(envParams);
        }

        void updatePitchBend(int pitchWheelPos) noexcept
        {
            const double norm = (static_cast<double>(pitchWheelPos) - 8192.0) / 8192.0;
            pitchBendRatio = std::pow(2.0, (norm * 2.0) / 12.0);
        }

        std::atomic<float>* lowParam = nullptr;
        std::atomic<float>* midParam = nullptr;
        std::atomic<float>* highParam = nullptr;
        std::atomic<float>* attackParam = nullptr;
        std::atomic<float>* releaseParam = nullptr;

        juce::ADSR adsr;
        double baseFreq = 440.0;
        double pitchBendRatio = 1.0;
        double level = 0.0;

        float lowSmooth = 0.0f;
        float midSmooth = 0.0f;
        float highSmooth = 0.0f;
        float lastAttack = -1.0f;
        float lastRelease = -1.0f;

        double subPhase = 0.0;
        double fundPhase = 0.0;
        std::array<double, 5> midPhases{ 0.0, 0.11, 0.23, 0.37, 0.51 };
        double highPhaseL = 0.0;
        double highPhaseR = 0.29;
        double shimmerPhase = 0.0;
        double punchEnv = 0.0;
        uint32_t noiseState = 0x12345678u;

        TptSvf bassFilter1, bassFilter2;
        TptSvf midFilterL, midFilterR;
        TptSvf highFilterL, highFilterR;
    };

    juce::String sanitizePresetName(const juce::String& rawName)
    {
        auto clean = juce::File::createLegalFileName(rawName.trim());
        while (clean.endsWithChar('.'))
            clean = clean.dropLastCharacters(1);
        return clean.trim();
    }
} // namespace

RgbSynthAudioProcessor::RgbSynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    synth.addSound(new RgbSound());
    synth.setNoteStealingEnabled(true);

    auto* lowPtr = apvts.getRawParameterValue("low");
    auto* midPtr = apvts.getRawParameterValue("mid");
    auto* highPtr = apvts.getRawParameterValue("high");
    auto* attackPtr = apvts.getRawParameterValue("attack");
    auto* releasePtr = apvts.getRawParameterValue("release");

    for (int i = 0; i < 16; ++i)
        synth.addVoice(new RgbVoice(lowPtr, midPtr, highPtr, attackPtr, releasePtr));
}

juce::AudioProcessorValueTreeState::ParameterLayout RgbSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "low", 1 }, "Low (Red)", 0.0f, 1.0f, 0.75f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "mid", 1 }, "Mid (Green)", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "high", 1 }, "High (Blue)", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "attack", 1 }, "Attack", 0.0f, 1.0f, 0.05f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "release", 1 }, "Release", 0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "volume", 1 }, "Volume", 0.0f, 1.0f, 0.80f));

    return { params.begin(), params.end() };
}

void RgbSynthAudioProcessor::prepareToPlay(double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    sortedMidi.ensureSize(8192);

    volumeSmoothCoeff = (sampleRate > 0.0)
        ? (1.0f - std::exp(-static_cast<float>(kTwoPi) * 40.0f / static_cast<float>(sampleRate)))
        : 1.0f;

    const float v = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("volume")->load());
    smoothedVolume = v * v;
    dcPrevIn[0] = dcPrevIn[1] = 0.0f;
    dcPrevOut[0] = dcPrevOut[1] = 0.0f;
}

void RgbSynthAudioProcessor::releaseResources() {}

bool RgbSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& outSet = layouts.getMainOutputChannelSet();
    return outSet == juce::AudioChannelSet::mono()
        || outSet == juce::AudioChannelSet::stereo();
}

void RgbSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    // Фикс для нот вплотную в Piano Roll: на каждом сэмпле NoteOff всегда идёт до NoteOn
    if (!midiMessages.isEmpty())
    {
        sortedMidi.clear();
        auto it = midiMessages.cbegin();
        const auto end = midiMessages.cend();

        while (it != end)
        {
            const int currentSample = (*it).samplePosition;
            auto groupEnd = it;
            while (groupEnd != end && (*groupEnd).samplePosition == currentSample)
                ++groupEnd;

            for (auto cur = it; cur != groupEnd; ++cur)
            {
                const auto msg = (*cur).getMessage();
                if (!msg.isNoteOn(true))
                    sortedMidi.addEvent(msg, currentSample);
            }

            for (auto cur = it; cur != groupEnd; ++cur)
            {
                const auto msg = (*cur).getMessage();
                if (msg.isNoteOn(true))
                    sortedMidi.addEvent(msg, currentSample);
            }

            it = groupEnd;
        }

        synth.renderNextBlock(buffer, sortedMidi, 0, numSamples);
    }
    else
    {
        synth.renderNextBlock(buffer, midiMessages, 0, numSamples);
    }

    const float volParam = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("volume")->load());
    const float targetGain = volParam * volParam;
    const int activeChannels = std::min(numChannels, 2);

    for (int s = 0; s < numSamples; ++s)
    {
        smoothedVolume += volumeSmoothCoeff * (targetGain - smoothedVolume);

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float in = data[s];

            // DC Blocker убирает постоянное смещение от асимметричной сатурации
            const float dcFree = in - dcPrevIn[ch] + 0.995f * dcPrevOut[ch];
            dcPrevIn[ch] = in;
            dcPrevOut[ch] = dcFree;

            data[s] = std::tanh(dcFree * 0.80f) * smoothedVolume;
        }
    }

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear(ch, 0, numSamples);
}

juce::AudioProcessorEditor* RgbSynthAudioProcessor::createEditor()
{
    return new RgbSynthAudioProcessorEditor(*this);
}

void RgbSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("presetName", currentPresetName);
    copyXmlToBinary(*xml, destData);
}

void RgbSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        currentPresetName = xmlState->getStringAttribute("presetName", "");
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::File RgbSynthAudioProcessor::getPresetsFolder() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("RGB Audio")
                   .getChildFile("RGB Synth")
                   .getChildFile("Presets");

    if (!dir.exists())
        dir.createDirectory();

    return dir;
}

juce::StringArray RgbSynthAudioProcessor::getAllPresetNames() const
{
    juce::StringArray names;
    auto files = getPresetsFolder().findChildFiles(juce::File::findFiles, false, "*.rgbpreset");
    files.sort();

    for (const auto& f : files)
        names.add(f.getFileNameWithoutExtension());

    return names;
}

bool RgbSynthAudioProcessor::savePreset(const juce::String& name)
{
    const auto cleanName = sanitizePresetName(name);
    if (cleanName.isEmpty())
        return false;

    auto file = getPresetsFolder().getChildFile(cleanName + ".rgbpreset");
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr)
        return false;

    xml->setAttribute("presetName", cleanName);
    if (!xml->writeTo(file))
        return false;

    currentPresetName = cleanName;
    return true;
}

bool RgbSynthAudioProcessor::loadPreset(const juce::String& name)
{
    const auto cleanName = sanitizePresetName(name);
    if (cleanName.isEmpty())
        return false;

    auto file = getPresetsFolder().getChildFile(cleanName + ".rgbpreset");
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
        return false;

    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    currentPresetName = cleanName;
    return true;
}

bool RgbSynthAudioProcessor::deletePreset(const juce::String& name)
{
    const auto cleanName = sanitizePresetName(name);
    if (cleanName.isEmpty())
        return false;

    auto file = getPresetsFolder().getChildFile(cleanName + ".rgbpreset");
    if (!file.existsAsFile())
        return false;

    if (!file.deleteFile())
        return false;

    if (currentPresetName == cleanName)
        currentPresetName.clear();

    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RgbSynthAudioProcessor();
}
