// PlateVerb editor (included by PluginProcessor.cpp)

// ============================================================================ look & feel (blue / white)
namespace col
{
    static const Colour navy    { 0xff0a1d45 };
    static const Colour panel   { 0xff1d4a9e };
    static const Colour panelHi { 0xff2a5cb8 };
    static const Colour white   { 0xfff4f7fb };
    static const Colour lcdBg   { 0xff06122b };
    static const Colour lcdText { 0xffcfe3ff };
    static const Colour led     { 0xff3d9bff };
    static const Colour amber   { 0xffffc857 };
}

static Font uiFont (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }
static Font monoFont (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::bold)); }

class PVLook : public LookAndFeel_V4
{
public:
    PVLook()
    {
        setColour (ComboBox::backgroundColourId, col::navy);
        setColour (ComboBox::textColourId, col::white);
        setColour (ComboBox::outlineColourId, col::white.withAlpha (0.5f));
        setColour (ComboBox::arrowColourId, col::white);
        setColour (PopupMenu::backgroundColourId, col::navy);
        setColour (PopupMenu::textColourId, col::white);
        setColour (PopupMenu::highlightedBackgroundColourId, col::panelHi);
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (BubbleComponent::backgroundColourId, col::white);
        setColour (BubbleComponent::outlineColourId, col::navy);
        setColour (TooltipWindow::textColourId, col::navy);
        setColour (AlertWindow::backgroundColourId, col::navy);
        setColour (AlertWindow::textColourId, col::white);
        setColour (TextEditor::backgroundColourId, col::lcdBg);
        setColour (TextEditor::textColourId, col::lcdText);
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&) override
    {
        auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0.0f, a0, a1, true);
        g.setColour (col::navy);
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        Path val; val.addCentredArc (c.x, c.y, r - 2, r - 2, 0.0f, a0, ang, true);
        g.setColour (Colour (0xffbfe0ff));
        g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        const float kr = r - 7.0f;
        g.setColour (Colours::black.withAlpha (0.3f));
        g.fillEllipse (c.x - kr, c.y - kr + 2.0f, kr * 2, kr * 2);
        g.setGradientFill (ColourGradient (Colours::white, c.x, c.y - kr, Colour (0xffbccbe3), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (col::navy.withAlpha (0.6f));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
        g.setColour (col::panel);
        g.drawLine (Line<float> (c.getPointOnCircumference (kr * 0.2f, ang), c.getPointOnCircumference (kr * 0.85f, ang)), 3.0f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           const Slider::SliderStyle style, Slider& s) override
    {
        if (style != Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
            return;
        }
        const float cx = (float) x + (float) w * 0.5f;
        g.setColour (col::white.withAlpha (0.35f));
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = (float) y + (float) h * (float) i / 10.0f;
            const float len = (i % 5 == 0) ? 9.0f : 5.0f;
            g.drawLine (cx - 9.0f - len, ty, cx - 9.0f, ty, 1.0f);
            g.drawLine (cx + 9.0f, ty, cx + 9.0f + len, ty, 1.0f);
        }
        g.setColour (col::lcdBg);
        g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) h, 3.0f);
        Rectangle<float> cap (cx - 14.0f, sliderPos - 10.0f, 28.0f, 20.0f);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
        g.setGradientFill (ColourGradient (Colours::white, cap.getX(), cap.getY(), Colour (0xffb9c8e0), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (col::panel);
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f);
    }

    int getSliderThumbRadius (Slider& s) override { return s.isVertical() ? 10 : LookAndFeel_V4::getSliderThumbRadius (s); }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();
        const bool plain = b.getProperties()["plain"];
        g.setGradientFill (ColourGradient (on ? Colours::white : Colour (0xffdfe7f3), r.getX(), r.getY(),
                                           on ? Colour (0xffd2e4ff) : Colour (0xffaebdd4), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        if (over || down) { g.setColour (Colours::white.withAlpha (down ? 0.35f : 0.18f)); g.fillRoundedRectangle (r, 3.0f); }
        if (! plain)
        {
            const float d = 7.0f;
            Rectangle<float> led (r.getX() + 7.0f, r.getCentreY() - d * 0.5f, d, d);
            if (on) { g.setColour (col::led.withAlpha (0.35f)); g.fillEllipse (led.expanded (3.0f)); g.setColour (col::led); }
            else g.setColour (Colour (0xff56657f));
            g.fillEllipse (led);
        }
        g.setColour (col::navy);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setColour (col::navy);
        g.setFont (uiFont (b.getButtonText().length() > 6 ? 10.5f : 11.5f, true));
        const bool plain = b.getProperties()["plain"];
        g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (plain ? 0 : 14), Justification::centred, false);
    }
};

// ============================================================================ display (double-click = service page)
class LcdDisplay : public Component
{
public:
    std::function<void()> onDoubleClick;
    StringArray lines; String badges[3]; bool badgeOn[3] = { false, false, false };

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 6.0f);
        auto in = r.reduced (4.0f);
        g.setGradientFill (ColourGradient (Colour (0xff0b1d42), in.getX(), in.getY(), col::lcdBg, in.getX(), in.getBottom(), false));
        g.fillRoundedRectangle (in, 4.0f);
        g.setColour (col::lcdText.withAlpha (0.06f));
        for (float yy = in.getY() + 2.0f; yy < in.getBottom(); yy += 3.0f) g.drawHorizontalLine ((int) yy, in.getX(), in.getRight());
        float bx = in.getRight() - 10.0f;
        for (int i = 2; i >= 0; --i)
        {
            Rectangle<float> br (bx - 50.0f, in.getY() + 8.0f, 50.0f, 16.0f); bx -= 56.0f;
            g.setColour (badgeOn[i] ? col::lcdText : col::lcdText.withAlpha (0.18f));
            g.drawRoundedRectangle (br, 2.0f, 1.0f);
            g.setFont (monoFont (10.0f));
            g.drawText (badges[i], br, Justification::centred, false);
        }
        auto t = in.reduced (12.0f, 8.0f);
        for (int i = 0; i < lines.size(); ++i)
        {
            g.setColour (i == 0 ? Colours::white : col::lcdText);
            g.setFont (monoFont (i == 0 ? 15.0f : 12.5f));
            auto row = t.removeFromTop (i == 0 ? 24.0f : 20.0f);
            if (i == 0) row = row.withTrimmedRight (170.0f);
            g.drawText (lines[i], row, Justification::centredLeft, true);
        }
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
};

// ============================================================================ impulse / decay viewer
class WaveView : public Component
{
public:
    WaveView()
    {
        for (auto* b : { &logTime, &logLevel })
        {
            b->setClickingTogglesState (true);
            b->getProperties().set ("plain", true);
            b->onClick = [this] { repaint(); };
            addAndMakeVisible (*b);
        }
        logTime.setButtonText ("LOG TIME"); logLevel.setButtonText ("LOG LEVEL"); logLevel.setToggleState (true, dontSendNotification);
    }
    pv::WaveData data; String title, emptyText;
    TextButton logTime, logLevel;

    void resized() override
    {
        logLevel.setBounds (getWidth() - 96, 8, 86, 22);
        logTime.setBounds (getWidth() - 188, 8, 86, 22);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 6.0f);
        auto in = r.reduced (4.0f);
        g.setGradientFill (ColourGradient (Colour (0xff0b1d42), in.getX(), in.getY(), col::lcdBg, in.getX(), in.getBottom(), false));
        g.fillRoundedRectangle (in, 4.0f);
        g.setColour (Colours::white); g.setFont (monoFont (13.0f));
        g.drawText (title, (int) in.getX() + 12, (int) in.getY() + 8, 420, 20, Justification::centredLeft);
        auto plot = in.reduced (46.0f, 0.0f).withTrimmedTop (36.0f).withTrimmedBottom (22.0f);
        const bool lt = logTime.getToggleState(), ll = logLevel.getToggleState();
        if (! data.valid || data.l.empty())
        {
            g.setColour (col::lcdText.withAlpha (0.7f)); g.setFont (monoFont (12.0f));
            g.drawText (emptyText, plot, Justification::centred);
            return;
        }
        const float total = (float) data.l.size() * data.binMs;           // ms
        const float t0 = 1.0f;
        auto xOf = [&] (float ms) {
            if (lt) return plot.getX() + plot.getWidth() * std::log (jmax (t0, ms) / t0) / std::log (jmax (t0 * 2, total) / t0);
            return plot.getX() + plot.getWidth() * ms / total;
        };
        const float floorDb = -90.0f;
        // grid
        g.setFont (monoFont (9.5f));
        if (ll)
            for (int d = 0; d >= -90; d -= 15)
            {
                const float y = plot.getY() + plot.getHeight() * (float) d / floorDb;
                g.setColour (col::lcdText.withAlpha (0.12f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
                g.setColour (col::lcdText.withAlpha (0.6f)); g.drawText (String (d), (int) plot.getX() - 40, (int) y - 6, 34, 12, Justification::centredRight);
            }
        else
        {
            const float cy = plot.getCentreY();
            g.setColour (col::lcdText.withAlpha (0.2f)); g.drawHorizontalLine ((int) cy, plot.getX(), plot.getRight());
            g.setColour (col::lcdText.withAlpha (0.6f));
            g.drawText ("L", (int) plot.getX() - 40, (int) plot.getY(), 34, 12, Justification::centredRight);
            g.drawText ("R", (int) plot.getX() - 40, (int) plot.getBottom() - 12, 34, 12, Justification::centredRight);
        }
        std::vector<float> ticks;
        if (lt) ticks = { 1, 10, 100, 1000, 10000 };
        else { const float step = total > 6000 ? 1000 : total > 2500 ? 500 : total > 1000 ? 250 : 100; for (float t = 0; t <= total; t += step) ticks.push_back (t); }
        for (float t : ticks)
        {
            if (t > total) continue;
            const float x = xOf (t);
            g.setColour (col::lcdText.withAlpha (0.12f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col::lcdText.withAlpha (0.6f));
            const String lab = t >= 1000 ? String (t / 1000.0f, t >= 10000 || std::fmod (t, 1000.0f) == 0 ? 0 : 2) + " s" : String ((int) t) + " ms";
            g.drawText (lab, (int) x - 30, (int) plot.getBottom() + 4, 60, 12, Justification::centred);
        }
        // traces
        const int W = (int) plot.getWidth();
        auto peakAt = [&] (const std::vector<float>& v, float msA, float msB) {
            int a = jlimit (0, (int) v.size() - 1, (int) (msA / data.binMs)), b = jlimit (a, (int) v.size() - 1, (int) (msB / data.binMs));
            float p = 0; for (int i = a; i <= b; ++i) p = jmax (p, v[(size_t) i]); return p;
        };
        auto msOfX = [&] (float x) {
            const float u = (x - plot.getX()) / plot.getWidth();
            return lt ? t0 * std::pow (jmax (t0 * 2, total) / t0, u) : u * total;
        };
        if (ll)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                Path p; bool started = false;
                for (int x = 0; x < W; ++x)
                {
                    const float ms0 = msOfX (plot.getX() + (float) x), ms1 = msOfX (plot.getX() + (float) x + 1.0f);
                    const float v = peakAt (ch == 0 ? data.l : data.r, ms0, ms1);
                    const float db = jmax (floorDb, 20.0f * std::log10 (v + 1e-9f));
                    const float y = plot.getY() + plot.getHeight() * db / floorDb;
                    if (! started) { p.startNewSubPath (plot.getX() + (float) x, y); started = true; } else p.lineTo (plot.getX() + (float) x, y);
                }
                g.setColour (ch == 0 ? Colour (0xffbfe0ff) : col::led.withAlpha (0.85f));
                g.strokePath (p, PathStrokeType (1.3f));
            }
        }
        else
        {
            const float cy = plot.getCentreY(), hh = plot.getHeight() * 0.5f - 2.0f;
            Path pl, pr; pl.startNewSubPath (plot.getX(), cy); pr.startNewSubPath (plot.getX(), cy);
            for (int x = 0; x < W; ++x)
            {
                const float ms0 = msOfX (plot.getX() + (float) x), ms1 = msOfX (plot.getX() + (float) x + 1.0f);
                pl.lineTo (plot.getX() + (float) x, cy - hh * peakAt (data.l, ms0, ms1));
                pr.lineTo (plot.getX() + (float) x, cy + hh * peakAt (data.r, ms0, ms1));
            }
            pl.lineTo (plot.getRight(), cy); pr.lineTo (plot.getRight(), cy); pl.closeSubPath(); pr.closeSubPath();
            g.setColour (Colour (0xffbfe0ff).withAlpha (0.85f)); g.fillPath (pl);
            g.setColour (col::led.withAlpha (0.85f)); g.fillPath (pr);
        }
        // ER / LR crossover marker (IR)
        if (data.xoverMs >= 0.0f && data.xoverMs < total)
        {
            const float x = xOf (jmax (t0, data.xoverMs));
            g.setColour (col::amber.withAlpha (0.8f));
            for (float y = plot.getY(); y < plot.getBottom(); y += 6.0f) g.drawVerticalLine ((int) x, y, y + 3.0f);
            g.setFont (monoFont (9.5f)); g.drawText ("ER|LR", (int) x + 3, (int) plot.getY(), 50, 12, Justification::centredLeft);
        }
        g.setColour (col::lcdText); g.setFont (monoFont (11.5f));
        g.drawText ("RT60 " + (data.rt > 0 ? String (data.rt, 2) + " s" : String ("--")) + "   LENGTH " + String (total / 1000.0f, 2) + " s",
                    (int) in.getX() + 440, (int) in.getY() + 8, 300, 20, Justification::centredLeft);
    }
};

// ============================================================================ hidden service page (transformer)
class ServicePanel : public Component
{
public:
    ServicePanel (APVTS& s) : st (s)
    {
        engage.setButtonText ("ENGAGE"); engage.setClickingTogglesState (true);
        addAndMakeVisible (engage);
        engageAtt = std::make_unique<APVTS::ButtonAttachment> (st, "trOn", engage);
        model.addItemList (trNames(), 1);
        addAndMakeVisible (model);
        modelAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "trModel", model);
        for (auto* k : { &freq, &amount })
        {
            k->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            k->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
            k->setPopupDisplayEnabled (true, true, this);
            addAndMakeVisible (*k);
        }
        freqAtt = std::make_unique<APVTS::SliderAttachment> (st, "trFreq", freq);
        amtAtt = std::make_unique<APVTS::SliderAttachment> (st, "trAmt", amount);
        close.setButtonText ("CLOSE");
        close.onClick = [this] { setVisible (false); };
        addAndMakeVisible (close);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.translated (0, 4), 8.0f);
        g.setColour (col::navy); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::white.withAlpha (0.6f)); g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 1.5f);
        g.setColour (col::white); g.setFont (uiFont (14.0f, true));
        g.drawText ("SERVICE PAGE  -  OUTPUT TRANSFORMER", 18, 12, getWidth() - 36, 20, Justification::centredLeft);
        g.setColour (col::lcdText.withAlpha (0.8f)); g.setFont (uiFont (11.0f));
        g.drawText ("Models are approximations inspired by classic hardware.", 18, 32, getWidth() - 36, 16, Justification::centredLeft);
        g.setColour (col::white); g.setFont (uiFont (11.0f, true));
        g.drawText ("FREQUENCY", freq.getBounds().translated (0, freq.getHeight() - 4).withHeight (16), Justification::centred);
        g.drawText ("AMOUNT", amount.getBounds().translated (0, amount.getHeight() - 4).withHeight (16), Justification::centred);
        g.drawText ("MODEL", model.getBounds().translated (0, -18).withHeight (16), Justification::centredLeft);
    }
    void resized() override
    {
        engage.setBounds (18, 58, 110, 28);
        model.setBounds (18, 112, 250, 26);
        freq.setBounds (290, 56, 70, 70);
        amount.setBounds (370, 56, 70, 70);
        close.setBounds (getWidth() - 110, getHeight() - 40, 92, 26);
    }

private:
    APVTS& st;
    TextButton engage, close; ComboBox model; Slider freq, amount;
    std::unique_ptr<APVTS::ButtonAttachment> engageAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> modelAtt;
    std::unique_ptr<APVTS::SliderAttachment> freqAtt, amtAtt;
};

// ============================================================================ main panel (fixed 1400 x 900 design, scaled by the editor)
class MainPanel : public Component, private Timer
{
public:
    static constexpr int W = 1400, H = 900;
    enum { kDefaultId = 1, kBuiltinBase = 2, kUserBase = 2000, kSaveId = 9000, kDeleteId = 9001, kFolderId = 9002 };

    explicit MainPanel (PlateVerbProcessor& p) : proc (p), service (p.apvts)
    {
        editSlot = jlimit (0, pv::kNumSlots - 1, (int) proc.apvts.state.getProperty ("editSlot", 0));
        // slots: on / level / edit
        for (int s = 0; s < pv::kNumSlots; ++s)
        {
            addButton (sid (s, "on"), kSlotNames[s], false);
            addSlider (sid (s, "lv"), "LEVEL", true, false);
            auto* e = editButtons.add (new TextButton ("EDIT"));
            e->getProperties().set ("plain", true);
            e->setClickingTogglesState (false);
            e->onClick = [this, s] { selectSlot (s); };
            addAndMakeVisible (e);
        }
        // reverb faders (per slot)
        const char* rv[][2] = { { "decay", "DECAY" }, { "lowMult", "LOW DEC" }, { "highMult", "HIGH DEC" }, { "size", "SIZE" },
                                { "predelay", "PRE-DLY" }, { "build", "BUILD" }, { "early", "EARLY" }, { "diff", "DIFFUSE" },
                                { "damping", "DAMPING" }, { "tone", "TONE" }, { "mod", "MOD" }, { "inWidth", "WIDTH" } };
        for (auto& e : rv) addSlider (e[0], e[1], true, true);
        addSlider ("mix", "MIX", true, false);
        // reverb EQ (per slot)
        const char* bn[] = { "LOW", "LO-MID", "HI-MID", "HIGH" };
        for (int b = 0; b < 4; ++b) { addSlider (nid ("eqF", b), String (bn[b]) + " HZ", false, true); addSlider (nid ("eqG", b), String (bn[b]) + " dB", false, true); }
        // compressor (per slot)
        addButton ("cpOn", "COMP", true);
        addButton ("cpDuck", "DUCK", true);
        addCombo ("cpType", compTypeNames(), "TYPE", true);
        addCombo ("cpTone", StringArray { "Clean", "Vintage" }, "TONE", true);
        const char* cpn[][2] = { { "cpThr", "THRESH" }, { "cpRatio", "RATIO" }, { "cpAtk", "ATTACK" },
                                 { "cpRel", "RELEASE" }, { "cpMakeup", "MAKEUP" }, { "cpMix", "MIX" } };
        for (auto& e : cpn) addSlider (e[0], e[1], false, true);
        // IR
        const char* irk[][2] = { { "irStart", "START" }, { "irXover", "ER/LR X" }, { "irFadeOut", "FADE OUT" },
                                 { "irXfeed", "X-FEED" }, { "irXdelay", "X-DELAY" }, { "irAsym", "ASYM" } };
        for (auto& e : irk) addSlider (e[0], e[1], false, false);
        addButton ("irReverse", "REVERSE", false);
        addButton ("irPitch", "STRETCH PITCH", false);
        for (auto* b : { &irLoad, &irLoadL, &irLoadR, &irPrev, &irNext, &irClear })
        {
            b->getProperties().set ("plain", true);
            addAndMakeVisible (*b);
        }
        irLoad.setButtonText ("LOAD"); irLoadL.setButtonText ("L"); irLoadR.setButtonText ("R");
        irPrev.setButtonText ("<"); irNext.setButtonText (">"); irClear.setButtonText ("CLEAR");
        irLoad.onClick = [this] { chooseIr (0); };
        irLoadL.onClick = [this] { chooseIr (1); };
        irLoadR.onClick = [this] { chooseIr (2); };
        irPrev.onClick = [this] { stepIr (-1); };
        irNext.onClick = [this] { stepIr (1); };
        irClear.onClick = [this] { proc.clearIr(); };
        // delay
        addButton ("dlOn", "DELAY", false); addButton ("dlSync", "SYNC", false); addButton ("dlPing", "PING", false);
        const char* dln[][2] = { { "dlTimeL", "TIME L" }, { "dlTimeR", "TIME R" }, { "dlFb", "FEEDBACK" }, { "dlMix", "MIX" },
                                 { "dlAtk", "ATTACK" }, { "dlPanL", "PAN L" }, { "dlPanR", "PAN R" }, { "dlWidth", "WIDTH" },
                                 { "dlLow", "LOW" }, { "dlMid", "MID" }, { "dlMidF", "MID HZ" }, { "dlHigh", "HIGH" } };
        for (auto& e : dln) addSlider (e[0], e[1], false, false);
        addCombo ("dlDivL", divNames(), "DIV L", false); addCombo ("dlDivR", divNames(), "DIV R", false);
        // header
        addCombo ("os", osNames(), "OS", false);
        addSlider ("out", "OUTPUT", false, false);
        for (auto* b : { &presetPrev, &presetNext, &presetSave }) { b->getProperties().set ("plain", true); addAndMakeVisible (*b); }
        presetPrev.setButtonText ("<"); presetNext.setButtonText (">"); presetSave.setButtonText ("SAVE");
        presetPrev.onClick = [this] { stepPreset (-1); };
        presetNext.onClick = [this] { stepPreset (1); };
        presetSave.onClick = [this] { savePreset(); };
        presetBox.onChange = [this] { presetChosen(); };
        presetBox.setTextWhenNothingSelected ("Default");
        addAndMakeVisible (presetBox);
        buildPresetMenu();

        display.badges[0] = "COMP"; display.badges[1] = "DELAY"; display.badges[2] = "XFMR";
        display.onDoubleClick = [this] { service.setVisible (! service.isVisible()); if (service.isVisible()) service.toFront (true); };
        addAndMakeVisible (display);
        addAndMakeVisible (wave);
        addChildComponent (service);

        for (auto* prm : proc.getParameters()) lastValues.add (prm->getValue());
        bindSlot (editSlot);
        setSize (W, H);
        startTimerHz (25);
    }
    ~MainPanel() override { stopTimer(); }

    // ---------------------------------------------------------------- painting
    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (col::panelHi, 0, 0, col::panel, 0, (float) getHeight(), false));
        g.fillAll();
        auto head = getLocalBounds().removeFromTop (58).toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff12306c), 0, 0, col::navy, 0, head.getBottom(), false));
        g.fillRect (head);
        g.setColour (col::white.withAlpha (0.6f)); g.fillRect (0.0f, head.getBottom() - 2.0f, (float) getWidth(), 2.0f);
        g.setColour (col::white); g.setFont (uiFont (20.0f, true));
        g.drawText ("P L A T E V E R B", 24, 9, 260, 26, Justification::centredLeft);
        g.setColour (col::lcdText.withAlpha (0.85f)); g.setFont (uiFont (10.0f, true));
        g.drawText ("DIGITAL REVERBERATION PROCESSOR  /  PV-5", 26, 34, 300, 14, Justification::centredLeft);
        g.setColour (col::white); g.setFont (uiFont (10.5f, true));
        g.drawText ("PRESET", presetBox.getX() - 56, presetBox.getY(), 50, presetBox.getHeight(), Justification::centredRight);
        g.drawText ("OVERSAMPLING", combo ("os").getX() - 96, combo ("os").getY(), 90, combo ("os").getHeight(), Justification::centredRight);
        g.drawText ("OUTPUT", slider ("out").getX() - 58, slider ("out").getY(), 52, slider ("out").getHeight(), Justification::centredRight);
        for (auto pt : { Point<float> (10, 10), Point<float> (getWidth() - 10.0f, 10), Point<float> (10, getHeight() - 10.0f),
                         Point<float> (getWidth() - 10.0f, getHeight() - 10.0f) })
        {
            g.setColour (Colour (0xffc9d4e6)); g.fillEllipse (pt.x - 5, pt.y - 5, 10, 10);
            g.setColour (col::navy); g.drawLine (pt.x - 3, pt.y, pt.x + 3, pt.y, 1.5f);
        }
        // highlight the slot being edited
        {
            auto& e = *editButtons[editSlot];
            auto col0 = Rectangle<int> (e.getX() - 3, slotArea.getY() + 8, e.getWidth() + 6, slotArea.getHeight() - 14).toFloat();
            g.setColour (col::white.withAlpha (0.10f)); g.fillRoundedRectangle (col0, 5.0f);
            g.setColour (col::white.withAlpha (0.55f)); g.drawRoundedRectangle (col0, 5.0f, 1.0f);
        }
        for (auto& s : sections)
        {
            auto r = s.area.toFloat();
            g.setColour (col::navy.withAlpha (0.28f)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::white.withAlpha (0.45f)); g.drawRoundedRectangle (r, 6.0f, 1.0f);
            const String t = s.perSlot ? s.title + "  -  " + String (kSlotNames[editSlot]) : s.title;
            auto tab = Rectangle<float> (r.getX() + 12.0f, r.getY() - 9.0f, (float) t.length() * 8.0f + 18.0f, 18.0f);
            g.setColour (col::navy); g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (s.perSlot ? Colour (0xffbfe0ff) : col::white); g.setFont (uiFont (11.5f, true));
            g.drawText (t, tab, Justification::centred);
        }
        g.setColour (col::lcdText.withAlpha (0.85f)); g.setFont (uiFont (10.5f, true));
        for (auto& c : captions) g.drawText (c.title, c.area, Justification::centredLeft);
        // IR file info
        {
            g.setColour (col::lcdBg); g.fillRoundedRectangle (irInfoBox.toFloat(), 3.0f);
            g.setColour (col::lcdText.withAlpha (0.25f)); g.drawRoundedRectangle (irInfoBox.toFloat(), 3.0f, 1.0f);
            const int lay = proc.irLayout();
            const String layTxt = lay == pv::IrTrueStereo ? "TRUE STEREO (4 CH)" : lay == pv::IrStereo ? "STEREO" : lay == pv::IrMono ? "MONO" : "NO IR LOADED";
            g.setColour (col::lcdText); g.setFont (monoFont (10.5f));
            auto ib = irInfoBox.reduced (8, 3);
            g.drawText (proc.irInfo().isEmpty() ? String ("-") : proc.irInfo(), ib.removeFromTop (16), Justification::centredLeft, true);
            g.setColour (col::lcdText.withAlpha (0.75f));
            g.drawText (layTxt + (proc.irRecordedRt() > 0 ? "   RT " + String (proc.irRecordedRt(), 2) + " s" : String()), ib.removeFromTop (16), Justification::centredLeft, true);
        }
        // control labels and value boxes
        g.setFont (uiFont (10.5f, true));
        for (auto& c : sliders)
        {
            if (! c->slider.isVisible() || c->baseId == "out") continue;
            auto b = c->slider.getBounds();
            g.setColour (col::white); g.setFont (uiFont (10.5f, true));
            g.drawText (c->label, b.getX() - 14, b.getBottom() + 1, b.getWidth() + 28, 14, Justification::centred);
            auto vb = valueBox (*c).toFloat();
            bool limited = false;
            const String t = isComp (c->baseId) ? compValueText (c->baseId, limited) : text (c->slider.getName());
            g.setColour (col::lcdBg); g.fillRoundedRectangle (vb, 3.0f);
            g.setColour (col::lcdText.withAlpha (0.25f)); g.drawRoundedRectangle (vb, 3.0f, 1.0f);
            g.setColour (limited ? col::amber : col::lcdText); g.setFont (monoFont (c->fader ? 9.5f : 9.5f));
            g.drawText (t, vb, Justification::centred, false);
        }
        g.setColour (col::white); g.setFont (uiFont (10.5f, true));
        for (auto& c : combos)
            if (c->box.isVisible() && c->baseId != "os")
                g.drawText (c->label, c->box.getX(), c->box.getY() - 15, c->box.getWidth(), 14, Justification::centredLeft);
        paintCompMeters (g);
    }

    void paintCompMeters (Graphics& g)
    {
        const float lo = -80.0f;
        const float inDb = jlimit (lo, 0.0f, proc.compIn[editSlot].load());
        const float grDb = jlimit (0.0f, 30.0f, proc.compGr[editSlot].load());
        const float thr  = raw (sid (editSlot, "cpThr"));
        auto in = inMeter.toFloat(), gr = grMeter.toFloat();
        g.setColour (col::lcdBg); g.fillRoundedRectangle (in, 3.0f); g.fillRoundedRectangle (gr, 3.0f);
        const float inH = in.getHeight() * (inDb - lo) / -lo;
        g.setColour (col::lcdText); g.fillRect (in.getX() + 3.0f, in.getBottom() - inH, in.getWidth() - 6.0f, inH);
        const float ty = in.getBottom() - in.getHeight() * (jlimit (lo, 0.0f, thr) - lo) / -lo;
        g.setColour (col::amber); g.fillRect (in.getX() - 3.0f, ty - 1.0f, in.getWidth() + 6.0f, 2.0f);
        const float grH = gr.getHeight() * grDb / 30.0f;
        g.setColour (col::led); g.fillRect (gr.getX() + 3.0f, gr.getY(), gr.getWidth() - 6.0f, grH);
        g.setFont (monoFont (9.0f));
        for (int d = 0; d <= 80; d += 20)
        {
            const float y = in.getBottom() - in.getHeight() * (80.0f - (float) d) / 80.0f;
            g.setColour (col::white.withAlpha (0.6f));
            g.drawText (String (-d), (int) in.getX() - 26, (int) y - 6, 22, 12, Justification::centredRight);
        }
        for (int d = 0; d <= 30; d += 10)
        {
            const float y = gr.getY() + gr.getHeight() * (float) d / 30.0f;
            g.setColour (col::white.withAlpha (0.6f));
            g.drawText (String (-d), (int) gr.getRight() + 3, (int) y - 6, 24, 12, Justification::centredLeft);
        }
        g.setColour (col::lcdText); g.setFont (monoFont (10.5f));
        g.drawText (inDb <= lo + 0.1f ? String ("-inf") : String (inDb, 1), inMeter.getX() - 14, inMeter.getY() - 18, inMeter.getWidth() + 28, 14, Justification::centred);
        g.drawText (grDb < 0.05f ? String ("0.0") : String (-grDb, 1), grMeter.getX() - 14, grMeter.getY() - 18, grMeter.getWidth() + 28, 14, Justification::centred);
        g.setColour (col::white); g.setFont (uiFont (10.0f, true));
        g.drawText (raw (sid (editSlot, "cpDuck")) > 0.5f ? "DRY" : "IN", inMeter.getX() - 14, inMeter.getBottom() + 2, inMeter.getWidth() + 28, 14, Justification::centred);
        g.drawText ("GR", grMeter.getX() - 14, grMeter.getBottom() + 2, grMeter.getWidth() + 28, 14, Justification::centred);
    }

    // ---------------------------------------------------------------- layout
    void resized() override
    {
        sections.clear(); captions.clear();
        // header
        presetBox.setBounds (640, 16, 300, 26);
        presetPrev.setBounds (946, 16, 28, 26); presetNext.setBounds (978, 16, 28, 26); presetSave.setBounds (1012, 16, 60, 26);
        combo ("os").setBounds (1184, 17, 70, 24);
        slider ("out").setBounds (1330, 6, 46, 46);
        // display + viewer
        display.setBounds (16, 70, 400, 196);
        wave.setBounds (428, 70, 956, 196);
        service.setBounds (465, 160, 470, 230);
        // slots
        slotArea = { 16, 292, 412, 270 }; addSection (slotArea, "ALGORITHMS", false);
        for (int s = 0; s < pv::kNumSlots; ++s)
        {
            const int x = slotArea.getX() + 8 + s * 80;
            button (sid (s, "on")).setBounds (x, slotArea.getY() + 16, 76, 26);
            slider (sid (s, "lv")).setBounds (x + 20, slotArea.getY() + 72, 36, 146);
            editButtons[s]->setBounds (x + 6, slotArea.getY() + 236, 64, 24);
        }
        // reverb faders
        Rectangle<int> rv (440, 292, 944, 270); addSection (rv, "REVERB", true);
        const char* rvIds[] = { "decay", "lowMult", "highMult", "size", "predelay", "build", "early", "diff", "damping", "tone", "mod", "inWidth", "mix" };
        for (int i = 0; i < 13; ++i) sliderBase (rvIds[i]).setBounds (rv.getX() + 18 + i * 72, rv.getY() + 46, 36, 196);
        // IR
        Rectangle<int> ir (16, 588, 300, 296); addSection (ir, "IMPULSE RESPONSE", false);
        irLoad.setBounds (ir.getX() + 10, ir.getY() + 18, 60, 24);
        irLoadL.setBounds (ir.getX() + 74, ir.getY() + 18, 30, 24);
        irLoadR.setBounds (ir.getX() + 108, ir.getY() + 18, 30, 24);
        irPrev.setBounds (ir.getX() + 146, ir.getY() + 18, 30, 24);
        irNext.setBounds (ir.getX() + 180, ir.getY() + 18, 30, 24);
        irClear.setBounds (ir.getX() + 218, ir.getY() + 18, 72, 24);
        irInfoBox = { ir.getX() + 10, ir.getY() + 48, 280, 38 };
        const char* irIds[] = { "irStart", "irXover", "irFadeOut", "irXfeed", "irXdelay", "irAsym" };
        for (int i = 0; i < 6; ++i) slider (irIds[i]).setBounds (ir.getX() + 28 + (i % 3) * 94, ir.getY() + 96 + (i / 3) * 80, 46, 46);
        button ("irReverse").setBounds (ir.getX() + 10, ir.getY() + 262, 120, 26);
        button ("irPitch").setBounds (ir.getX() + 140, ir.getY() + 262, 150, 26);
        // reverb EQ
        Rectangle<int> eq (328, 588, 272, 296); addSection (eq, "REVERB EQ", true);
        for (int b = 0; b < 4; ++b)
        {
            const int x = eq.getX() + 16 + b * 64;
            sliderBase (nid ("eqF", b)).setBounds (x, eq.getY() + 40, 46, 46);
            sliderBase (nid ("eqG", b)).setBounds (x, eq.getY() + 150, 46, 46);
        }
        // compressor
        Rectangle<int> cpA (612, 588, 330, 296); addSection (cpA, "TAIL COMPRESSOR", true);
        buttonBase ("cpOn").setBounds (cpA.getX() + 10, cpA.getY() + 20, 72, 26);
        comboBase ("cpType").setBounds (cpA.getX() + 88, cpA.getY() + 34, 134, 24);
        comboBase ("cpTone").setBounds (cpA.getX() + 228, cpA.getY() + 34, 92, 24);
        const char* cpIds[] = { "cpThr", "cpRatio", "cpAtk", "cpRel", "cpMakeup", "cpMix" };
        for (int i = 0; i < 6; ++i) sliderBase (cpIds[i]).setBounds (cpA.getX() + 16 + (i % 3) * 70, cpA.getY() + 86 + (i / 3) * 96, 46, 46);
        inMeter = { cpA.getX() + 248, cpA.getY() + 88, 14, 150 };
        grMeter = { cpA.getX() + 284, cpA.getY() + 88, 14, 150 };
        buttonBase ("cpDuck").setBounds (cpA.getX() + 226, cpA.getY() + 262, 94, 26);
        // delay
        Rectangle<int> dl (954, 588, 430, 296); addSection (dl, "STEREO DELAY", false);
        button ("dlOn").setBounds (dl.getX() + 12, dl.getY() + 20, 84, 26);
        button ("dlSync").setBounds (dl.getX() + 102, dl.getY() + 20, 70, 26);
        button ("dlPing").setBounds (dl.getX() + 178, dl.getY() + 20, 70, 26);
        const char* row2[] = { "dlTimeL", "dlTimeR", "dlFb", "dlMix", "dlAtk" };
        for (int i = 0; i < 5; ++i) slider (row2[i]).setBounds (dl.getX() + 22 + i * 82, dl.getY() + 66, 46, 46);
        combo ("dlDivL").setBounds (dl.getX() + 10, dl.getY() + 78, 70, 24);
        combo ("dlDivR").setBounds (dl.getX() + 92, dl.getY() + 78, 70, 24);
        const char* row3[] = { "dlPanL", "dlPanR", "dlWidth", "dlLow", "dlMid", "dlMidF", "dlHigh" };
        for (int i = 0; i < 7; ++i) slider (row3[i]).setBounds (dl.getX() + 14 + i * 58 + (i >= 3 ? 10 : 0), dl.getY() + 180, 44, 44);
        captions.push_back ({ { dl.getX() + 14, dl.getY() + 160, 160, 14 }, "PAN / WIDTH", false });
        captions.push_back ({ { dl.getX() + 200, dl.getY() + 160, 200, 14 }, "ANALOG EQ", false });
    }

private:
    struct SliderCtl { Slider slider; String label, baseId; bool fader = false, perSlot = false; std::unique_ptr<APVTS::SliderAttachment> att; };
    struct ButtonCtl { TextButton button; String baseId; bool perSlot = false; std::unique_ptr<APVTS::ButtonAttachment> att; };
    struct ComboCtl  { ComboBox box; String label, baseId; bool perSlot = false; std::unique_ptr<APVTS::ComboBoxAttachment> att; };
    struct Section   { Rectangle<int> area; String title; bool perSlot; };

    Rectangle<int> valueBox (const SliderCtl& c) const
    {
        const auto& s = c.slider;
        if (c.fader) return { s.getX() - 14, s.getY() - 24, s.getWidth() + 28, 17 };
        return { s.getX() - 16, s.getBottom() + 15, s.getWidth() + 32, 15 };
    }
    static bool isComp (const String& id) { return id.startsWith ("cp") && id != "cpOn" && id != "cpDuck"; }

    // the setting each compressor type really uses: 1176 limits (amber when the knob is past them), AUTO for the LA-2A
    String compValueText (const String& base, bool& limited)
    {
        limited = false;
        const int type = (int) raw (sid (editSlot, "cpType"));
        const bool fet = type == pv::CompFET || type == pv::CompFetOpto, optoOnly = type == pv::CompOpto;
        const String id = sid (editSlot, base);
        if (base == "cpAtk" || base == "cpRel")
        {
            if (optoOnly) return base == "cpAtk" ? "AUTO 0.010 s" : "AUTO 0.06-15 s";
            const float v = raw (id);
            float lo = 0.0f, hi = 1.0e9f;
            if (fet) { lo = base == "cpAtk" ? pv::Compressor::kFetAtkMin * 0.001f : pv::Compressor::kFetRelMin * 0.001f;
                       hi = base == "cpAtk" ? pv::Compressor::kFetAtkMax * 0.001f : pv::Compressor::kFetRelMax * 0.001f; }
            const float e = jlimit (lo, hi, v);
            limited = e != v;
            return fmtSeconds (e);
        }
        return text (id);
    }

    String idFor (const String& base, bool perSlot) const { return perSlot ? sid (editSlot, base) : base; }

    void addSlider (const String& id, const String& label, bool fader, bool perSlot)
    {
        auto c = std::make_unique<SliderCtl>();
        c->baseId = id; c->perSlot = perSlot; c->fader = fader; c->label = label;
        c->slider.setSliderStyle (fader ? Slider::LinearVertical : Slider::RotaryHorizontalVerticalDrag);
        c->slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        c->slider.setPopupDisplayEnabled (true, true, this);
        c->slider.setMouseDragSensitivity (320);
        addAndMakeVisible (c->slider);
        sliders.push_back (std::move (c));
        if (! perSlot) attach (*sliders.back(), id);
    }
    void attach (SliderCtl& c, const String& id)
    {
        c.att.reset();
        c.slider.setName (id);
        c.att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, c.slider);
        if (auto* prm = proc.apvts.getParameter (id))
            c.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    void addButton (const String& id, const String& text, bool perSlot)
    {
        auto c = std::make_unique<ButtonCtl>();
        c->baseId = id; c->perSlot = perSlot;
        c->button.setButtonText (text);
        c->button.setClickingTogglesState (true);
        addAndMakeVisible (c->button);
        if (! perSlot) { c->button.setName (id); c->att = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, c->button); }
        buttons.push_back (std::move (c));
    }
    void addCombo (const String& id, const StringArray& items, const String& label, bool perSlot)
    {
        auto c = std::make_unique<ComboCtl>();
        c->baseId = id; c->perSlot = perSlot; c->label = label;
        c->box.addItemList (items, 1);
        addAndMakeVisible (c->box);
        if (! perSlot) { c->box.setName (id); c->att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, c->box); }
        combos.push_back (std::move (c));
    }
    void bindSlot (int s)
    {
        editSlot = s;
        for (auto& c : sliders) if (c->perSlot) attach (*c, sid (s, c->baseId));
        for (auto& c : buttons) if (c->perSlot) { c->att.reset(); c->button.setName (sid (s, c->baseId)); c->att = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, sid (s, c->baseId), c->button); }
        for (auto& c : combos) if (c->perSlot) { c->att.reset(); c->box.setName (sid (s, c->baseId)); c->att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, sid (s, c->baseId), c->box); }
        for (int k = 0; k < editButtons.size(); ++k) editButtons[k]->setToggleState (k == s, dontSendNotification);
        proc.viewSlot.store (s);
        proc.apvts.state.setProperty ("editSlot", s, nullptr);
        lastWaveVersion = -1;
        repaint();
    }
    void selectSlot (int s) { if (s != editSlot) bindSlot (s); }

    Slider& findSlider (const String& name) { for (auto& c : sliders) if (c->slider.getName() == name) return c->slider; jassertfalse; return sliders.front()->slider; }
    Slider& slider (const String& id) { return findSlider (id); }
    Slider& sliderBase (const String& base) { for (auto& c : sliders) if (c->baseId == base) return c->slider; jassertfalse; return sliders.front()->slider; }
    TextButton& button (const String& id) { for (auto& c : buttons) if (c->button.getName() == id) return c->button; jassertfalse; return buttons.front()->button; }
    TextButton& buttonBase (const String& base) { for (auto& c : buttons) if (c->baseId == base) return c->button; jassertfalse; return buttons.front()->button; }
    ComboBox& combo (const String& id) { for (auto& c : combos) if (c->box.getName() == id) return c->box; jassertfalse; return combos.front()->box; }
    ComboBox& comboBase (const String& base) { for (auto& c : combos) if (c->baseId == base) return c->box; jassertfalse; return combos.front()->box; }
    void addSection (Rectangle<int> r, const String& t, bool perSlot) { sections.push_back ({ r, t, perSlot }); }

    float raw (const String& id) { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.0f; }
    static String withUnit (RangedAudioParameter& p)
    {
        const auto l = p.getLabel();
        const auto t = p.getCurrentValueAsText();
        return t + (l.isEmpty() || l == "%" || l == "x" || ! (t.containsAnyOf ("0123456789")) ? "" : " ") + (t.containsAnyOf ("0123456789") ? l : String());
    }
    String text (const String& id)
    {
        if (auto* prm = proc.apvts.getParameter (id)) return withUnit (*prm);
        return {};
    }

    // ---------------------------------------------------------------- presets
    Array<File> userFiles;
    void buildPresetMenu()
    {
        auto* root = presetBox.getRootMenu();
        root->clear();
        root->addItem (kDefaultId, "Default");
        String group; PopupMenu sub;
        for (int i = 0; i < kNumPresets; ++i)
        {
            if (group != kPresets[i].group && group.isNotEmpty()) { root->addSubMenu (group, sub); sub = PopupMenu(); }
            group = kPresets[i].group;
            sub.addItem (kBuiltinBase + i, kPresets[i].name);
        }
        if (group.isNotEmpty()) root->addSubMenu (group, sub);
        userFiles = userPresetFolder().findChildFiles (File::findFiles, false, "*.pvpreset");
        std::sort (userFiles.begin(), userFiles.end(), [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        PopupMenu u;
        for (int i = 0; i < userFiles.size(); ++i) u.addItem (kUserBase + i, userFiles[i].getFileNameWithoutExtension());
        if (userFiles.isEmpty()) u.addItem (8999, "(no user presets yet)", false);
        root->addSeparator();
        root->addSubMenu ("User", u);
        root->addItem (kSaveId, "Save User Preset...");
        root->addItem (kDeleteId, "Delete Current User Preset", proc.userPresetName.isNotEmpty());
        root->addItem (kFolderId, "Show User Preset Folder");
        syncPresetBox();
    }
    void syncPresetBox()
    {
        int id = 0;
        if (proc.userPresetName.isNotEmpty())
        {
            for (int i = 0; i < userFiles.size(); ++i) if (userFiles[i].getFileNameWithoutExtension() == proc.userPresetName) id = kUserBase + i;
        }
        else id = proc.getCurrentProgram() == 0 ? kDefaultId : kBuiltinBase + proc.getCurrentProgram() - 1;
        if (id != 0 && presetBox.getSelectedId() != id) presetBox.setSelectedId (id, dontSendNotification);
    }
    void presetChosen()
    {
        const int id = presetBox.getSelectedId();
        if (id == kSaveId) { syncPresetBox(); savePreset(); return; }
        if (id == kDeleteId)
        {
            syncPresetBox();
            const File f = userPresetFolder().getChildFile (proc.userPresetName + ".pvpreset");
            if (f.existsAsFile()) f.moveToTrash();
            proc.userPresetName = {};
            buildPresetMenu(); return;
        }
        if (id == kFolderId) { syncPresetBox(); auto d = userPresetFolder(); d.createDirectory(); d.revealToUser(); return; }
        if (id == kDefaultId) proc.setCurrentProgram (0);
        else if (id >= kBuiltinBase && id < kBuiltinBase + kNumPresets) proc.setCurrentProgram (id - kBuiltinBase + 1);
        else if (id >= kUserBase && id - kUserBase < userFiles.size()) proc.loadUserPreset (userFiles[id - kUserBase]);
        buildPresetMenu();
    }
    void stepPreset (int d)
    {
        if (proc.userPresetName.isNotEmpty() && ! userFiles.isEmpty())
        {
            int i = 0; for (int k = 0; k < userFiles.size(); ++k) if (userFiles[k].getFileNameWithoutExtension() == proc.userPresetName) i = k;
            proc.loadUserPreset (userFiles[(i + d + userFiles.size()) % userFiles.size()]);
        }
        else proc.setCurrentProgram ((proc.getCurrentProgram() + d + kNumPresets + 1) % (kNumPresets + 1));
        buildPresetMenu();
    }
    std::unique_ptr<AlertWindow> alert;
    void savePreset()
    {
        alert = std::make_unique<AlertWindow> ("Save User Preset", "Name:", MessageBoxIconType::NoIcon, this);
        alert->addTextEditor ("name", proc.userPresetName.isNotEmpty() ? proc.userPresetName : String ("My Preset"));
        alert->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
        alert->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        alert->enterModalState (true, ModalCallbackFunction::create ([this] (int r)
        {
            if (r == 1 && alert != nullptr)
            {
                const String name = alert->getTextEditorContents ("name").trim();
                if (name.isNotEmpty()) proc.saveUserPreset (name);
                buildPresetMenu();
            }
            alert.reset();
        }), false);
    }

    // ---------------------------------------------------------------- IR files
    std::unique_ptr<FileChooser> chooser;
    void chooseIr (int mode)                // 0 = file (auto pair), 1 = left-input file, 2 = right-input file
    {
        File start = proc.irFileA().existsAsFile() ? proc.irFileA().getParentDirectory() : File::getSpecialLocation (File::userDocumentsDirectory);
        chooser = std::make_unique<FileChooser> (mode == 0 ? "Load impulse response" : mode == 1 ? "Load IR for the LEFT input" : "Load IR for the RIGHT input",
                                                 start, "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, mode] (const FileChooser& fc)
        {
            const File f = fc.getResult();
            if (! f.existsAsFile()) return;
            if (mode == 0) loadAuto (f);
            else
            {
                File a (proc.apvts.state.getProperty ("irA").toString()), b (proc.apvts.state.getProperty ("irB").toString());
                if (mode == 1) a = f; else b = f;
                if (! a.existsAsFile()) { a = b; b = File(); }
                proc.requestIrLoad (a, b, true);
            }
            selectSlot (pv::IR);
            if (raw (sid (pv::IR, "on")) < 0.5f) proc.setParam (sid (pv::IR, "on"), 1.0f);
        });
    }
    void loadAuto (const File& f)
    {
        bool isLeft = true;
        const File partner = findPartner (f, isLeft);
        if (partner.existsAsFile()) proc.requestIrLoad (isLeft ? f : partner, isLeft ? partner : f, true);
        else proc.requestIrLoad (f, File(), true);
    }
    void stepIr (int d)
    {
        const File cur = proc.irFileA();
        if (! cur.existsAsFile()) return;
        auto files = cur.getParentDirectory().findChildFiles (File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
        std::sort (files.begin(), files.end(), [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        // a true-stereo pair counts once (its left file)
        Array<File> list;
        for (auto& f : files) { bool left = true; const File p = findPartner (f, left); if (p.existsAsFile() && ! left) continue; list.add (f); }
        int i = list.indexOf (cur); if (i < 0) i = 0;
        if (list.isEmpty()) return;
        loadAuto (list[(i + d + list.size()) % list.size()]);
    }

    // ---------------------------------------------------------------- timer
    void timerCallback() override
    {
        // a preset (or user preset) asks the editor to show its slot
        const int req = proc.presetSlotRequest.exchange (-1);
        if (req >= 0 && req < pv::kNumSlots) { bindSlot (req); buildPresetMenu(); }

        const bool sync = raw ("dlSync") > 0.5f;
        slider ("dlTimeL").setVisible (! sync); slider ("dlTimeR").setVisible (! sync);
        combo ("dlDivL").setVisible (sync); combo ("dlDivR").setVisible (sync);
        const bool optoOnly = (int) raw (sid (editSlot, "cpType")) == pv::CompOpto;
        for (auto* b : { "cpAtk", "cpRel" }) { sliderBase (b).setEnabled (! optoOnly); sliderBase (b).setAlpha (optoOnly ? 0.4f : 1.0f); }
        const bool isIr = editSlot == pv::IR;
        sliderBase ("diff").setAlpha (1.0f);

        auto& params = proc.getParameters();
        bool changed = false;
        for (int i = 0; i < params.size() && i < lastValues.size(); ++i)
        {
            const float v = params[i]->getValue();
            if (v != lastValues[i]) { lastValues.set (i, v); lastTouched = i; changed = true; }
        }
        StringArray on;
        for (int s = 0; s < pv::kNumSlots; ++s) if (raw (sid (s, "on")) > 0.5f) on.add (kSlotNames[s]);
        const String presetName = proc.userPresetName.isNotEmpty() ? proc.userPresetName
                                : proc.getCurrentProgram() > 0 ? proc.getProgramName (proc.getCurrentProgram()) : String ("Default");
        StringArray l;
        l.add (presetName.toUpperCase());
        l.add ("ACTIVE  " + (on.isEmpty() ? String ("-- NONE --") : on.joinIntoString (" + ")));
        l.add ("EDIT    " + String (kSlotNames[editSlot]) + (isIr ? "   " + (proc.irLayout() == pv::IrNone ? String ("(no IR)") : proc.irInfo().substring (0, 22)) : String()));
        l.add ("DECAY " + text (sid (editSlot, "decay")) + "  PRE " + text (sid (editSlot, "predelay")) + "  SIZE " + text (sid (editSlot, "size")));
        l.add ("MIX " + text ("mix") + "  OS " + osNames()[(int) raw ("os")] + "  DSP " + String (proc.dspLoad.load(), 1) + "%");
        l.add ("COMP " + String (raw (sid (editSlot, "cpOn")) > 0.5f ? compTypeNames()[(int) raw (sid (editSlot, "cpType"))] : String ("off"))
               + "   DELAY " + String (raw ("dlOn") > 0.5f ? "on" : "off"));
        if (lastTouched >= 0 && lastTouched < params.size())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (params[lastTouched]))
                l.add ("> " + rp->getName (40).toUpperCase() + ": " + withUnit (*rp));
        display.lines = l;
        display.badgeOn[0] = raw (sid (editSlot, "cpOn")) > 0.5f; display.badgeOn[1] = raw ("dlOn") > 0.5f; display.badgeOn[2] = raw ("trOn") > 0.5f;
        display.repaint();
        syncPresetBox();

        // wave viewer
        const int ver = proc.waveVersion.load();
        if (ver != lastWaveVersion)
        {
            lastWaveVersion = ver;
            wave.data = proc.getWave (editSlot);
            wave.title = String (kSlotNames[editSlot]) + (isIr ? "  IMPULSE RESPONSE" : "  IMPULSE");
            wave.emptyText = isIr ? "NO IMPULSE RESPONSE LOADED  -  PRESS LOAD IN THE IMPULSE RESPONSE SECTION" : "RENDERING...";
            wave.repaint();
        }
        repaint (inMeter.getUnion (grMeter).expanded (34, 22));
        if (changed) repaint();
        else repaint (irInfoBox);
    }

    PlateVerbProcessor& proc;
    PVLook look;
    LcdDisplay display;
    WaveView wave;
    ServicePanel service;
    ComboBox presetBox;
    TextButton presetPrev, presetNext, presetSave, irLoad, irLoadL, irLoadR, irPrev, irNext, irClear;
    OwnedArray<TextButton> editButtons;
    std::vector<std::unique_ptr<SliderCtl>> sliders;
    std::vector<std::unique_ptr<ButtonCtl>> buttons;
    std::vector<std::unique_ptr<ComboCtl>> combos;
    std::vector<Section> sections, captions;
    Rectangle<int> inMeter, grMeter, slotArea, irInfoBox;
    Array<float> lastValues; int lastTouched = -1, editSlot = 0, lastWaveVersion = -1;

public:
    PVLook& getLook() { return look; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

// ============================================================================ editor: scales the panel to the window
class PlateVerbEditor : public AudioProcessorEditor
{
public:
    explicit PlateVerbEditor (PlateVerbProcessor& p) : AudioProcessorEditor (&p), panel (p)
    {
        setLookAndFeel (&panel.getLook());
        addAndMakeVisible (panel);
        setResizable (true, true);
        setResizeLimits (MainPanel::W / 2, MainPanel::H / 2, MainPanel::W * 2, MainPanel::H * 2);
        getConstrainer()->setFixedAspectRatio ((double) MainPanel::W / MainPanel::H);
        const double sc = (double) p.apvts.state.getProperty ("uiScale", 0.9);
        setSize (roundToInt (MainPanel::W * sc), roundToInt (MainPanel::H * sc));
        procRef = &p;
    }
    ~PlateVerbEditor() override { setLookAndFeel (nullptr); }
    void resized() override
    {
        const float sc = (float) getWidth() / (float) MainPanel::W;
        panel.setTransform (AffineTransform::scale (sc));
        panel.setBounds (0, 0, MainPanel::W, MainPanel::H);
        if (procRef != nullptr) procRef->apvts.state.setProperty ("uiScale", sc, nullptr);
    }

private:
    MainPanel panel;
    PlateVerbProcessor* procRef = nullptr;
};

AudioProcessorEditor* PlateVerbProcessor::createEditor() { return new PlateVerbEditor (*this); }
