#include "PresetBrowser.h"
#include "../PluginProcessor.h"

static const juce::Colour kBg       = juce::Colour (0xff1a1a2e);
static const juce::Colour kSurface  = juce::Colour (0xff16213e);
static const juce::Colour kAccent   = juce::Colour (0xffe94560);
static const juce::Colour kText     = juce::Colour (0xffeeeeee);
static const juce::Colour kDim      = juce::Colour (0xff888888);
static const juce::Colour kListSel  = juce::Colour (0xffe94560).withAlpha (0.25f);
static const juce::Colour kListBg   = juce::Colour (0xff111122);

static constexpr const char* kPresetExtension = ".choppr";

//==============================================================================
static void styleButton (juce::TextButton& btn,
                          juce::Colour normalBg = juce::Colour (0xff16213e))
{
    btn.setColour (juce::TextButton::buttonColourId,  normalBg);
    btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe94560));
    btn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffeeeeee));
    btn.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffeeeeee));
}

//==============================================================================
PresetBrowser::PresetBrowser (ChopprProcessor& processor)
    : processor_ (processor)
{
    // Title
    titleLabel_.setText ("PRESETS", juce::dontSendNotification);
    titleLabel_.setFont (juce::Font (13.0f, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, kText);
    titleLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel_);

    // List box
    presetListBox_.setModel (this);
    presetListBox_.setColour (juce::ListBox::backgroundColourId,   kListBg);
    presetListBox_.setColour (juce::ListBox::outlineColourId,       juce::Colour (0xff333355));
    presetListBox_.setColour (juce::ListBox::textColourId,          kText);
    presetListBox_.setRowHeight (24);
    presetListBox_.setOutlineThickness (1);
    addAndMakeVisible (presetListBox_);

    // Buttons
    styleButton (saveButton_,   juce::Colour (0xff1a2a1a));
    saveButton_.addListener (this);
    addAndMakeVisible (saveButton_);

    styleButton (saveAsButton_);
    saveAsButton_.addListener (this);
    addAndMakeVisible (saveAsButton_);

    styleButton (loadButton_,   juce::Colour (0xff1a1a2e));
    loadButton_.setColour (juce::TextButton::buttonColourId, kAccent.darker (0.3f));
    loadButton_.addListener (this);
    addAndMakeVisible (loadButton_);

    styleButton (deleteButton_, juce::Colour (0xff2a1a1a));
    deleteButton_.addListener (this);
    addAndMakeVisible (deleteButton_);

    refreshPresets();
}

//==============================================================================
void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (juce::Colour (0xff0a0a1a));
    g.drawLine (static_cast<float> (getWidth() - 1), 0.0f,
                static_cast<float> (getWidth() - 1), static_cast<float> (getHeight()),
                1.0f);
}

void PresetBrowser::resized()
{
    const int pad   = 8;
    const int w     = getWidth() - pad * 2;
    const int btnH  = 24;
    const int gap   = 4;
    const int titleH = 28;

    // Title at top
    titleLabel_.setBounds (pad, pad, w, 20);

    // Buttons at the bottom
    const int bottomY   = getHeight() - pad - btnH * 2 - gap;
    const int halfW     = (w - gap) / 2;

    saveButton_.setBounds   (pad,           bottomY,         halfW, btnH);
    saveAsButton_.setBounds (pad + halfW + gap, bottomY,     halfW, btnH);
    loadButton_.setBounds   (pad,           bottomY + btnH + gap, halfW, btnH);
    deleteButton_.setBounds (pad + halfW + gap, bottomY + btnH + gap, halfW, btnH);

    // List box fills the remaining middle area
    const int listTop    = titleH + pad;
    const int listBottom = bottomY - gap;
    presetListBox_.setBounds (pad, listTop, w, juce::jmax (0, listBottom - listTop));
}

//==============================================================================
void PresetBrowser::buttonClicked (juce::Button* button)
{
    if (button == &saveButton_)
    {
        // Overwrite currently selected preset, or fall through to Save As
        const int selected = presetListBox_.getSelectedRow();
        if (selected >= 0 && selected < presetNames_.size())
        {
            savePresetAs (presetNames_[selected]);
        }
        else
        {
            // No selection – behave like Save As
            const juce::String name = juce::AlertWindow::showInputBox (
                "Save Preset", "Enter preset name:", "");
            if (name.isNotEmpty())
            {
                savePresetAs (name);
                refreshPresets();
            }
        }
    }
    else if (button == &saveAsButton_)
    {
        const juce::String name = juce::AlertWindow::showInputBox (
            "Save Preset As", "Enter preset name:", "");
        if (name.isNotEmpty())
        {
            savePresetAs (name);
            refreshPresets();
        }
    }
    else if (button == &loadButton_)
    {
        const int selected = presetListBox_.getSelectedRow();
        if (selected >= 0 && selected < presetNames_.size())
            loadPreset (presetNames_[selected]);
    }
    else if (button == &deleteButton_)
    {
        const int selected = presetListBox_.getSelectedRow();
        if (selected < 0 || selected >= presetNames_.size())
            return;

        const juce::String name = presetNames_[selected];
        juce::AlertWindow::showOkCancelBox (
            juce::MessageBoxIconType::QuestionIcon,
            "Delete Preset",
            "Delete \"" + name + "\"?",
            "Delete", "Cancel",
            nullptr,
            juce::ModalCallbackFunction::create (
                [this, name] (int result)
                {
                    if (result == 1) // OK
                    {
                        deletePreset (name);
                        refreshPresets();
                    }
                }));
    }
}

//==============================================================================
// ListBoxModel
//==============================================================================
int PresetBrowser::getNumRows()
{
    return presetNames_.size();
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g,
                                       int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.setColour (kListSel);
        g.fillRect (0, 0, width, height);
    }

    // Accent stripe on selected row
    if (rowIsSelected)
    {
        g.setColour (kAccent.withAlpha (0.8f));
        g.fillRect (0, 0, 3, height);
    }

    g.setFont (juce::Font (12.0f));
    g.setColour (rowIsSelected ? kText : kDim);

    if (row >= 0 && row < presetNames_.size())
        g.drawText (presetNames_[row],
                    8, 0, width - 8, height,
                    juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < presetNames_.size())
        loadPreset (presetNames_[row]);
}

//==============================================================================
void PresetBrowser::refreshPresets()
{
    presetNames_.clear();

    const juce::File dir = presetDirectory();
    if (!dir.isDirectory())
    {
        presetListBox_.updateContent();
        return;
    }

    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false,
                        "*" + juce::String (kPresetExtension));
    files.sort();

    for (const auto& f : files)
        presetNames_.add (f.getFileNameWithoutExtension());

    presetListBox_.updateContent();
    presetListBox_.repaint();
}

//==============================================================================
juce::File PresetBrowser::presetDirectory() const
{
    juce::File dir = juce::File::getSpecialLocation (
                         juce::File::userApplicationDataDirectory)
                     .getChildFile ("CHOPPR")
                     .getChildFile ("Presets");

    if (!dir.exists())
        dir.createDirectory();

    return dir;
}

juce::File PresetBrowser::presetFile (const juce::String& name) const
{
    return presetDirectory().getChildFile (name + kPresetExtension);
}

void PresetBrowser::savePresetAs (const juce::String& name)
{
    // Ask the processor to serialise its state, then write to disk as XML.
    juce::MemoryBlock memBlock;
    processor_.getStateInformation (memBlock);

    const juce::File dest = presetFile (name);
    dest.replaceWithData (memBlock.getData(), memBlock.getSize());
}

void PresetBrowser::loadPreset (const juce::String& name)
{
    const juce::File src = presetFile (name);
    if (!src.existsAsFile()) return;

    juce::MemoryBlock memBlock;
    src.loadFileAsData (memBlock);

    if (memBlock.getSize() > 0)
        processor_.setStateInformation (memBlock.getData(),
                                         static_cast<int> (memBlock.getSize()));
}

void PresetBrowser::deletePreset (const juce::String& name)
{
    presetFile (name).deleteFile();
}
