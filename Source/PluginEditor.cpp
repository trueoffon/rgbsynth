#include "PluginEditor.h"
#include <cmath>

void RgbBarLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float, float, float,
                                         juce::Slider::SliderStyle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    constexpr float border = 3.0f;

    g.setColour(inkColour.withAlpha(0.10f));
    g.fillRect(bounds);

    const float proportion = static_cast<float>(slider.valueToProportionOfLength(slider.getValue()));
    const auto inner = bounds.reduced(border);
    if (proportion > 0.0f)
    {
        auto filled = inner;
        filled.setWidth(inner.getWidth() * juce::jlimit(0.0f, 1.0f, proportion));
        g.setColour(inkColour.withAlpha(0.92f));
        g.fillRect(filled);
    }

    g.setColour(inkColour);
    g.drawRect(bounds, border);
}

void RgbBarLookAndFeel::drawComboBox(juce::Graphics& g,
                                     int width, int height,
                                     bool isButtonDown,
                                     int, int, int, int,
                                     juce::ComboBox&)
{
    const auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    constexpr float border = 3.0f;

    g.setColour(inkColour.withAlpha(isButtonDown ? 0.22f : 0.10f));
    g.fillRect(bounds);

    g.setColour(inkColour);
    g.drawRect(bounds, border);

    const float arrowX = static_cast<float>(width) - 18.0f;
    const float arrowY = static_cast<float>(height) * 0.5f - 2.5f;
    juce::Path arrow;
    arrow.addTriangle(arrowX - 5.0f, arrowY,
                      arrowX + 5.0f, arrowY,
                      arrowX, arrowY + 6.0f);
    g.fillPath(arrow);
}

void RgbBarLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 2, box.getWidth() - 32, box.getHeight() - 4);
    label.setFont(getComboBoxFont(box));
}

juce::Font RgbBarLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(15.0f));
}

void RgbBarLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                             juce::Button& button,
                                             const juce::Colour&,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    constexpr float border = 3.0f;

    const float alpha = !button.isEnabled() ? 0.04f
                      : shouldDrawButtonAsDown ? 0.28f
                      : shouldDrawButtonAsHighlighted ? 0.18f
                      : 0.10f;

    g.setColour(inkColour.withAlpha(alpha));
    g.fillRect(bounds);

    g.setColour(inkColour.withAlpha(button.isEnabled() ? 1.0f : 0.35f));
    g.drawRect(bounds, border);
}

void RgbBarLookAndFeel::drawButtonText(juce::Graphics& g,
                                       juce::TextButton& button,
                                       bool, bool)
{
    g.setColour(inkColour.withAlpha(button.isEnabled() ? 1.0f : 0.35f));
    g.setFont(juce::FontOptions(15.0f));
    g.drawText(button.getButtonText(),
               button.getLocalBounds().reduced(4, 2),
               juce::Justification::centred,
               false);
}

void RgbBarLookAndFeel::fillTextEditorBackground(juce::Graphics& g,
                                                 int width, int height,
                                                 juce::TextEditor&)
{
    g.setColour(inkColour.withAlpha(0.12f));
    g.fillRect(0, 0, width, height);
}

void RgbBarLookAndFeel::drawTextEditorOutline(juce::Graphics& g,
                                              int width, int height,
                                              juce::TextEditor&)
{
    g.setColour(inkColour);
    g.drawRect(0, 0, width, height, 3);
}

void RgbBarLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(bgColour);
    g.setColour(inkColour);
    g.drawRect(0, 0, width, height, 3);
}

void RgbBarLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                          const juce::Rectangle<int>& area,
                                          bool isSeparator,
                                          bool isActive,
                                          bool isHighlighted,
                                          bool isTicked,
                                          bool,
                                          const juce::String& text,
                                          const juce::String&,
                                          const juce::Drawable*,
                                          const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour(inkColour.withAlpha(0.35f));
        g.fillRect(area.reduced(8, area.getHeight() / 2));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour(inkColour);
        g.fillRect(area.reduced(3, 1));
        g.setColour(bgColour);
    }
    else
    {
        g.setColour(inkColour.withAlpha(isActive ? 1.0f : 0.4f));
    }

    g.setFont(juce::FontOptions(15.0f));
    auto textArea = area.reduced(12, 0);
    if (isTicked)
        g.drawText("*", textArea.removeFromLeft(14), juce::Justification::centred, false);

    g.drawText(text, textArea, juce::Justification::centredLeft, true);
}

RgbSynthAudioProcessorEditor::RgbSynthAudioProcessorEditor(RgbSynthAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      lowAttachment(p.apvts, "low", lowSlider),
      midAttachment(p.apvts, "mid", midSlider),
      highAttachment(p.apvts, "high", highSlider),
      releaseAttachment(p.apvts, "release", releaseSlider),
      attackAttachment(p.apvts, "attack", attackSlider),
      volumeAttachment(p.apvts, "volume", volumeSlider)
{
    // Окно не забирает клавиатуру у FL Studio — можно крутить звук и играть ноты сразу
    disableFocus(*this);

    presetBox.setLookAndFeel(&themeLookAndFeel);
    presetBox.setTextWhenNothingSelected(juce::String::fromUTF8("— выбрать звук —"));
    presetBox.setTextWhenNoChoicesAvailable(juce::String::fromUTF8("нет сохранённых звуков"));
    disableFocus(presetBox);
    presetBox.onChange = [this]
    {
        if (isApplyingPreset)
            return;

        const int selectedId = presetBox.getSelectedId();
        if (selectedId > 0)
        {
            const auto name = presetBox.getText();
            juce::ScopedValueSetter<bool> guard(isApplyingPreset, true);
            if (audioProcessor.loadPreset(name))
            {
                trackedPresetName = name;
                deleteButton.setEnabled(true);
                timerCallback();
            }
        }
        else
        {
            deleteButton.setEnabled(false);
        }
    };
    addAndMakeVisible(presetBox);

    saveButton.setLookAndFeel(&themeLookAndFeel);
    saveButton.setButtonText(juce::String::fromUTF8("сохранить"));
    disableFocus(saveButton);
    saveButton.onClick = [this] { setSaveMode(!inSaveMode); };
    addAndMakeVisible(saveButton);

    deleteButton.setLookAndFeel(&themeLookAndFeel);
    deleteButton.setButtonText(juce::String::fromUTF8("×"));
    deleteButton.setTooltip(juce::String::fromUTF8("Удалить выбранный звук"));
    disableFocus(deleteButton);
    deleteButton.onClick = [this]
    {
        if (presetBox.getSelectedId() > 0)
        {
            audioProcessor.deletePreset(presetBox.getText());
            trackedPresetName.clear();
            refreshPresetList();
        }
    };
    addAndMakeVisible(deleteButton);

    nameEditor.setLookAndFeel(&themeLookAndFeel);
    nameEditor.setMultiLine(false);
    nameEditor.setReturnKeyStartsNewLine(false);
    nameEditor.setFont(juce::FontOptions(15.0f));
    nameEditor.setJustification(juce::Justification::centredLeft);
    disableFocus(nameEditor);
    nameEditor.onReturnKey = [this] { commitSavePreset(); };
    nameEditor.onEscapeKey = [this] { setSaveMode(false); };
    addChildComponent(nameEditor);

    confirmSaveButton.setLookAndFeel(&themeLookAndFeel);
    confirmSaveButton.setButtonText(juce::String::fromUTF8("ок"));
    disableFocus(confirmSaveButton);
    confirmSaveButton.onClick = [this] { commitSavePreset(); };
    addChildComponent(confirmSaveButton);

    cancelSaveButton.setLookAndFeel(&themeLookAndFeel);
    cancelSaveButton.setButtonText(juce::String::fromUTF8("отм."));
    disableFocus(cancelSaveButton);
    cancelSaveButton.onClick = [this] { setSaveMode(false); };
    addChildComponent(cancelSaveButton);

    setupBarSlider(lowSlider, true);
    setupBarSlider(midSlider, true);
    setupBarSlider(highSlider, true);
    setupBarSlider(releaseSlider, true);
    setupBarSlider(attackSlider, true);
    setupBarSlider(volumeSlider, false);

    setSize(460, 496);

    currentBgColour = juce::Colours::transparentBlack;
    timerCallback();
    refreshPresetList();
    startTimerHz(30);
}

RgbSynthAudioProcessorEditor::~RgbSynthAudioProcessorEditor()
{
    stopTimer();

    presetBox.setLookAndFeel(nullptr);
    saveButton.setLookAndFeel(nullptr);
    deleteButton.setLookAndFeel(nullptr);
    nameEditor.setLookAndFeel(nullptr);
    confirmSaveButton.setLookAndFeel(nullptr);
    cancelSaveButton.setLookAndFeel(nullptr);

    lowSlider.setLookAndFeel(nullptr);
    midSlider.setLookAndFeel(nullptr);
    highSlider.setLookAndFeel(nullptr);
    releaseSlider.setLookAndFeel(nullptr);
    attackSlider.setLookAndFeel(nullptr);
    volumeSlider.setLookAndFeel(nullptr);
}

void RgbSynthAudioProcessorEditor::disableFocus(juce::Component& c)
{
    c.setWantsKeyboardFocus(false);
    c.setMouseClickGrabsKeyboardFocus(false);
}

void RgbSynthAudioProcessorEditor::setupBarSlider(juce::Slider& slider, bool clearsPresetName)
{
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setSliderSnapsToMousePosition(true);
    slider.setLookAndFeel(&themeLookAndFeel);
    disableFocus(slider);
    slider.onValueChange = [this, clearsPresetName]
    {
        if (clearsPresetName && !isApplyingPreset && !audioProcessor.getCurrentPresetName().isEmpty())
        {
            audioProcessor.clearCurrentPresetName();
            trackedPresetName.clear();
            presetBox.setSelectedId(0, juce::dontSendNotification);
            deleteButton.setEnabled(false);
        }
        timerCallback();
    };
    addAndMakeVisible(slider);
}

void RgbSynthAudioProcessorEditor::applyThemeToWidgets()
{
    themeLookAndFeel.setThemeColours(currentBgColour, currentInkColour);

    presetBox.setColour(juce::ComboBox::textColourId, currentInkColour);
    presetBox.setColour(juce::ComboBox::arrowColourId, currentInkColour);
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    presetBox.setColour(juce::ComboBox::outlineColourId, currentInkColour);

    nameEditor.setColour(juce::TextEditor::textColourId, currentInkColour);
    nameEditor.setColour(juce::TextEditor::highlightColourId, currentInkColour.withAlpha(0.25f));
    nameEditor.setColour(juce::TextEditor::highlightedTextColourId, currentInkColour);
    nameEditor.setColour(juce::CaretComponent::caretColourId, currentInkColour);
    nameEditor.applyColourToAllText(currentInkColour, true);
    nameEditor.setTextToShowWhenEmpty(juce::String::fromUTF8("название звука..."),
                                      currentInkColour.withAlpha(0.50f));
}

void RgbSynthAudioProcessorEditor::refreshPresetList()
{
    juce::ScopedValueSetter<bool> guard(isApplyingPreset, true);

    const auto names = audioProcessor.getAllPresetNames();
    const auto activeName = audioProcessor.getCurrentPresetName();
    trackedPresetName = activeName;

    presetBox.clear(juce::dontSendNotification);
    int idToSelect = 0;

    for (int i = 0; i < names.size(); ++i)
    {
        const int itemId = i + 1;
        presetBox.addItem(names[i], itemId);
        if (names[i] == activeName)
            idToSelect = itemId;
    }

    presetBox.setSelectedId(idToSelect, juce::dontSendNotification);
    deleteButton.setEnabled(idToSelect > 0);
}

void RgbSynthAudioProcessorEditor::setSaveMode(bool enabled)
{
    inSaveMode = enabled;
    nameEditor.setVisible(enabled);
    confirmSaveButton.setVisible(enabled);
    cancelSaveButton.setVisible(enabled);

    if (enabled)
    {
        nameEditor.setWantsKeyboardFocus(true);
        nameEditor.setText(audioProcessor.getCurrentPresetName(), false);
        nameEditor.selectAll();
        nameEditor.grabKeyboardFocus();
    }
    else
    {
        nameEditor.giveAwayKeyboardFocus();
        nameEditor.setWantsKeyboardFocus(false);
    }
}

void RgbSynthAudioProcessorEditor::commitSavePreset()
{
    const auto newName = nameEditor.getText().trim();
    if (newName.isEmpty())
    {
        nameEditor.grabKeyboardFocus();
        return;
    }

    if (audioProcessor.savePreset(newName))
    {
        setSaveMode(false);
        refreshPresetList();
    }
}

void RgbSynthAudioProcessorEditor::timerCallback()
{
    const float low = audioProcessor.apvts.getRawParameterValue("low")->load();
    const float mid = audioProcessor.apvts.getRawParameterValue("mid")->load();
    const float high = audioProcessor.apvts.getRawParameterValue("high")->load();

    const auto r = static_cast<juce::uint8>(std::lround(juce::jlimit(0.0f, 1.0f, low) * 255.0f));
    const auto g = static_cast<juce::uint8>(std::lround(juce::jlimit(0.0f, 1.0f, mid) * 255.0f));
    const auto b = static_cast<juce::uint8>(std::lround(juce::jlimit(0.0f, 1.0f, high) * 255.0f));

    const auto newBg = juce::Colour::fromRGB(r, g, b);
    if (newBg != currentBgColour)
    {
        currentBgColour = newBg;
        const float lum = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
        currentInkColour = (lum > 0.48f) ? juce::Colours::black : juce::Colours::white;
        applyThemeToWidgets();
        repaint();
    }

    if (trackedPresetName != audioProcessor.getCurrentPresetName())
        refreshPresetList();
}

void RgbSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(currentBgColour);

    g.setColour(currentInkColour);
    g.drawRect(getLocalBounds().reduced(16), 4);

    g.setFont(juce::FontOptions(19.0f));

    const auto drawLabel = [&](int x, int yCenter, int w, const char* utf8Text)
    {
        g.drawText(juce::String::fromUTF8(utf8Text),
                   x, yCenter - 14, w, 28,
                   juce::Justification::centred, false);
    };

    drawLabel(34, 200, 106, "низ");
    drawLabel(34, 252, 106, "середина");
    drawLabel(34, 304, 106, "верх");

    drawLabel(34, 366, 72, "релиз");
    drawLabel(236, 366, 72, "атака");

    drawLabel(34, 432, 106, "громкость");
}

void RgbSynthAudioProcessorEditor::resized()
{
    presetBox.setBounds(34, 34, 240, 28);
    saveButton.setBounds(282, 34, 104, 28);
    deleteButton.setBounds(394, 34, 32, 28);

    nameEditor.setBounds(34, 70, 272, 28);
    confirmSaveButton.setBounds(314, 70, 52, 28);
    cancelSaveButton.setBounds(374, 70, 52, 28);

    constexpr int barH = 24;

    lowSlider.setBounds(148, 200 - barH / 2, 278, barH);
    midSlider.setBounds(148, 252 - barH / 2, 278, barH);
    highSlider.setBounds(148, 304 - barH / 2, 278, barH);

    releaseSlider.setBounds(112, 366 - barH / 2, 110, barH);
    attackSlider.setBounds(316, 366 - barH / 2, 110, barH);

    volumeSlider.setBounds(148, 432 - barH / 2, 278, barH);
}
