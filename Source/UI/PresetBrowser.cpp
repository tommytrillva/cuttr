#include "PresetBrowser.h"
#include "../PluginProcessor.h"

//==============================================================================
/**
 * NamePromptOverlay
 *
 * A non-blocking, self-deleting overlay that asks the user to type a name.
 * Add it to a parent Component; it removes and deletes itself on dismiss.
 */
class NamePromptOverlay final : public juce::Component
{
public:
    using Callback = std::function<void(juce::String)>;

    NamePromptOverlay (const juce::String& title, Callback callback)
        : title_ (title), callback_ (std::move (callback))
    {
        nameField_.setTextToShowWhenEmpty ("Enter name...", juce::Colours::grey);
        nameField_.onReturnKey  = [this] { confirm(); };
        nameField_.onEscapeKey  = [this] { dismiss(); };
        nameField_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0d0d1f));
        nameField_.setColour (juce::TextEditor::textColourId,        juce::Colour (0xffeeeeee));
        nameField_.setColour (juce::TextEditor::outlineColourId,     juce::Colour (0xff333355));
        nameField_.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffe94560));
        addAndMakeVisible (nameField_);

        okButton_.setColour     (juce::TextButton::buttonColourId,  juce::Colour (0xffe94560).darker (0.3f));
        okButton_.setColour     (juce::TextButton::textColourOffId, juce::Colour (0xffeeeeee));
        cancelButton_.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff16213e));
        cancelButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeeeeee));
        okButton_.onClick     = [this] { confirm(); };
        cancelButton_.onClick = [this] { dismiss(); };
        addAndMakeVisible (okButton_);
        addAndMakeVisible (cancelButton_);

        setInterceptsMouseClicks (true, true);
    }

    static void show (juce::Component* parent,
                      const juce::String& title,
                      Callback cb)
    {
        auto* overlay = new NamePromptOverlay (title, std::move (cb));
        parent->addAndMakeVisible (overlay);
        overlay->setBounds (parent->getLocalBounds());
        overlay->nameField_.grabKeyboardFocus();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.65f));

        const auto card = getLocalBounds().withSizeKeepingCentre (300, 140);
        g.setColour (juce::Colour (0xff1e2040));
        g.fillRoundedRectangle (card.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xffe94560));
        g.drawRoundedRectangle (card.toFloat(), 8.0f, 1.5f);

        g.setColour (juce::Colour (0xffeeeeee));
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (title_,
                    card.withHeight (42).reduced (14, 0).toFloat(),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto card = getLocalBounds().withSizeKeepingCentre (300, 140);
        card.removeFromTop (44);
        nameField_.setBounds (card.removeFromTop (32).reduced (12, 0));
        card.removeFromTop (8);
        auto btnRow = card.removeFromTop (32).reduced (12, 0);
        cancelButton_.setBounds (btnRow.removeFromRight (80));
        btnRow.removeFromRight (6);
        okButton_.setBounds     (btnRow.removeFromRight (80));
    }

private:
    void confirm()
    {
        if (callback_) callback_ (nameField_.getText().trim());
        close();
    }
    void dismiss()
    {
        if (callback_) callback_ ({});
        close();
    }
    void close()
    {
        if (auto* p = getParentComponent())
            p->removeChildComponent (this);
        delete this;
    }

    juce::String     title_;
    Callback         callback_;
    juce::TextEditor nameField_;
    juce::TextButton okButton_     { "OK"     };
    juce::TextButton cancelButton_ { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamePromptOverlay)
};

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
            saveAsNew();
        }
    }
    else if (button == &saveAsButton_)
    {
        saveAsNew();
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

//==============================================================================
void PresetBrowser::saveCurrentOrAs()
{
    const int selected = presetListBox_.getSelectedRow();
    if (selected >= 0 && selected < presetNames_.size())
    {
        savePresetAs (presetNames_[selected]);
    }
    else
    {
        saveAsNew();
    }
}

void PresetBrowser::saveAsNew()
{
    promptForName ("Save Preset As", [this] (const juce::String& name)
    {
        if (name.isNotEmpty())
        {
            savePresetAs (name);
            refreshPresets();
        }
    });
}

void PresetBrowser::promptForName (const juce::String& title,
                                    std::function<void(juce::String)> callback)
{
    // Show the overlay on the top-level editor so it covers the entire window.
    auto* root = getTopLevelComponent();
    if (root == nullptr)
        root = this;

    NamePromptOverlay::show (root, title, std::move (callback));
}
