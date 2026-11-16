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
    static const Colour bandLo  { 0xffff9f43 };
    static const Colour bandMid { 0xff6ee7b7 };
    static const Colour bandHi  { 0xff7cc4ff };
    // controls: warm ivory caps, light blue when switched on, ink-blue lettering, cream panel labels
    static const Colour creamHi { 0xfffbf5e3 };
    static const Colour creamLo { 0xffe2d4ae };
    static const Colour creamEdge { 0xff8a7a55 };
    static const Colour onHi    { 0xffdff0ff };
    static const Colour onLo    { 0xff94c9f5 };
    static const Colour ink     { 0xff16305e };
    static const Colour label   { 0xfff3e6c4 };
}
// a 2D cap with a soft top highlight and a short drop shadow
static void drawCap (Graphics& g, Rectangle<float> r, float corner, Colour hi, Colour lo)
{
    g.setColour (Colours::black.withAlpha (0.28f)); g.fillRoundedRectangle (r.translated (0.0f, 1.8f), corner);
    g.setGradientFill (ColourGradient (hi, r.getX(), r.getY(), lo, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (Colours::white.withAlpha (0.55f)); g.drawHorizontalLine ((int) (r.getY() + 1.5f), r.getX() + corner, r.getRight() - corner);
    g.setColour (col::creamEdge.withAlpha (0.8f)); g.drawRoundedRectangle (r, corner, 1.0f);
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
    Font getComboBoxFont (ComboBox&) override { return uiFont (14.0f, true); }
    Font getPopupMenuFont() override { return uiFont (15.0f); }
    Font getSliderPopupFont (Slider&) override { return monoFont (14.0f); }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&) override
    {
        auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0.0f, a0, a1, true);
        g.setColour (col::navy);
        g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
        Path val; val.addCentredArc (c.x, c.y, r - 2, r - 2, 0.0f, a0, ang, true);
        g.setColour (Colour (0xffbfe0ff));
        g.strokePath (val, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
        const float kr = r - 8.0f;
        g.setColour (Colours::black.withAlpha (0.3f));
        g.fillEllipse (c.x - kr, c.y - kr + 2.0f, kr * 2, kr * 2);
        g.setGradientFill (ColourGradient (col::creamHi, c.x, c.y - kr, col::creamLo, c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawEllipse (c.x - kr + 1.5f, c.y - kr + 1.5f, kr * 2 - 3.0f, kr * 2 - 3.0f, 1.0f);
        g.setColour (col::creamEdge);
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
        g.setColour (col::ink);
        g.drawLine (Line<float> (c.getPointOnCircumference (kr * 0.2f, ang), c.getPointOnCircumference (kr * 0.88f, ang)), 3.5f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           const Slider::SliderStyle style, Slider& s) override
    {
        if (style == Slider::LinearHorizontal)
        {
            const float cy = (float) y + (float) h * 0.5f;
            g.setColour (col::lcdBg); g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) w, 6.0f, 3.0f);
            g.setColour (Colour (0xffbfe0ff)); g.fillRoundedRectangle ((float) x, cy - 3.0f, sliderPos - (float) x, 6.0f, 3.0f);
            Rectangle<float> cap (sliderPos - 10.0f, cy - 12.0f, 20.0f, 24.0f);
            drawCap (g, cap, 3.0f, col::creamHi, col::creamLo);
            g.setColour (col::ink); g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 4.0f, 2.0f, cap.getHeight() - 8.0f);
            return;
        }
        if (style != Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s); return; }
        const float cx = (float) x + (float) w * 0.5f;
        g.setColour (col::white.withAlpha (0.35f));
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = (float) y + (float) h * (float) i / 10.0f;
            const float len = (i % 5 == 0) ? 9.0f : 5.0f;
            g.drawLine (cx - 10.0f - len, ty, cx - 10.0f, ty, 1.0f);
            g.drawLine (cx + 10.0f, ty, cx + 10.0f + len, ty, 1.0f);
        }
        g.setColour (col::lcdBg);
        g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) h, 3.0f);
        Rectangle<float> cap (cx - 15.0f, sliderPos - 11.0f, 30.0f, 22.0f);
        drawCap (g, cap, 3.0f, col::creamHi, col::creamLo);
        g.setColour (col::ink);
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f);
    }

    int getSliderThumbRadius (Slider& s) override { return s.isVertical() ? 11 : (s.isHorizontal() ? 10 : LookAndFeel_V4::getSliderThumbRadius (s)); }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f).withTrimmedBottom (1.0f);
        const bool on = b.getToggleState();
        if (down) r = r.translated (0.0f, 0.8f);
        drawCap (g, r, 3.0f, on ? col::onHi : col::creamHi, on ? col::onLo : col::creamLo);
        if (over && ! down) { g.setColour (Colours::white.withAlpha (0.16f)); g.fillRoundedRectangle (r, 3.0f); }
    }

    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setColour (col::ink);
        g.setFont (uiFont (b.getButtonText().length() > 7 ? 12.0f : 13.5f, true));
        g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedBottom (1), Justification::centred, false);
    }
};

// ============================================================================ slider with Option/Alt fine adjustment
class FineSlider : public Slider
{
public:
    void mouseDown (const MouseEvent& e) override { Slider::mouseDown (e); last = e.position; prop = valueToProportionOfLength (getValue()); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isAltDown())                       // Option (Mac) / Alt (Windows): ten times slower
        {
            const auto d = e.position - last; last = e.position;
            const double full = isVertical() ? (double) getHeight() : isHorizontal() ? (double) getWidth() : 320.0;
            const double delta = isVertical() ? -d.y : isHorizontal() ? d.x : (d.x - d.y);
            prop = jlimit (0.0, 1.0, prop + delta / (full * 10.0));
            setValue (proportionOfLengthToValue (prop), sendNotificationSync);
            return;
        }
        Slider::mouseDrag (e);
        last = e.position; prop = valueToProportionOfLength (getValue());
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isAltDown()) { auto w2 = w; w2.deltaY *= 0.1f; w2.deltaX *= 0.1f; Slider::mouseWheelMove (e, w2); }
        else Slider::mouseWheelMove (e, w);
    }
private:
    Point<float> last; double prop = 0.0;
};

// ============================================================================ decay viewer with draggable handles
class WaveView : public Component
{
public:
    explicit WaveView (APVTS& s) : st (s)
    {
        for (auto* b : { &logTime, &logLevel, &bands })
        {
            b->setClickingTogglesState (true);
            b->getProperties().set ("plain", true);
            b->onClick = [this] { repaint(); };
            addAndMakeVisible (*b);
        }
        logTime.setButtonText ("LOG TIME"); logLevel.setButtonText ("LOG LEVEL"); bands.setButtonText ("LOW/MID/HIGH");
        logLevel.setToggleState (true, dontSendNotification); bands.setToggleState (true, dontSendNotification);
    }
    pv::WaveData data; String title, emptyText; int slot = 0;
    TextButton logTime, logLevel, bands;

    void resized() override
    {
        bands.setBounds (getWidth() - 134, 8, 124, 26);
        logLevel.setBounds (getWidth() - 236, 8, 96, 26);
        logTime.setBounds (getWidth() - 338, 8, 96, 26);
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 6.0f);
        auto in = r.reduced (4.0f);
        g.setGradientFill (ColourGradient (Colour (0xff0b1d42), in.getX(), in.getY(), col::lcdBg, in.getX(), in.getBottom(), false));
        g.fillRoundedRectangle (in, 4.0f);
        g.setColour (Colours::white); g.setFont (monoFont (15.0f));
        g.drawText (title, (int) in.getX() + 14, (int) in.getY() + 8, (int) in.getWidth() - 620, 24, Justification::centredLeft, true);
        const auto plot = plotArea();
        if (! data.valid || data.l.empty())
        {
            g.setColour (col::lcdText.withAlpha (0.7f)); g.setFont (monoFont (14.0f));
            g.drawText (emptyText, plot, Justification::centred);
            return;
        }
        const bool lt = logTime.getToggleState(), ll = logLevel.getToggleState(), bd = bands.getToggleState() && ll;
        const float total = totalMs();
        const float floorDb = -90.0f;
        // grid
        g.setFont (monoFont (11.5f));
        if (ll)
            for (int d = 0; d >= -90; d -= 15)
            {
                const float y = plot.getY() + plot.getHeight() * (float) d / floorDb;
                g.setColour (col::lcdText.withAlpha (0.12f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
                g.setColour (col::lcdText.withAlpha (0.7f)); g.drawText (String (d), (int) plot.getX() - 50, (int) y - 7, 42, 14, Justification::centredRight);
            }
        else
        {
            g.setColour (col::lcdText.withAlpha (0.2f)); g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
            g.setColour (col::lcdText.withAlpha (0.7f));
            g.drawText ("L", (int) plot.getX() - 50, (int) plot.getY(), 42, 14, Justification::centredRight);
            g.drawText ("R", (int) plot.getX() - 50, (int) plot.getBottom() - 14, 42, 14, Justification::centredRight);
        }
        std::vector<float> ticks;
        if (lt) ticks = { 1, 3, 10, 30, 100, 300, 1000, 3000, 10000 };
        else { const float step = total > 6000 ? 1000 : total > 2500 ? 500 : total > 1000 ? 250 : 100; for (float t = 0; t <= total; t += step) ticks.push_back (t); }
        for (float t : ticks)
        {
            if (t > total) continue;
            const float x = xOf (t);
            g.setColour (col::lcdText.withAlpha (0.12f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col::lcdText.withAlpha (0.7f));
            const String lab = t >= 1000 ? String (t / 1000.0f, std::fmod (t, 1000.0f) == 0 ? 0 : 2) + " s" : String ((int) t) + " ms";
            g.drawText (lab, (int) x - 34, (int) plot.getBottom() + 5, 68, 14, Justification::centred);
        }
        // traces: one value per physical pixel column
        const float scale = jmax (1.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
        const int cols = (int) (plot.getWidth() * scale);
        const float off = offsetMs();
        auto peakAt = [&] (const std::vector<float>& v, float msA, float msB) {
            msA -= off; msB -= off;
            if (msB < 0.0f) return 0.0f;
            int a = jlimit (0, (int) v.size() - 1, (int) (msA / data.binMs)), b = jlimit (a, (int) v.size() - 1, (int) (msB / data.binMs));
            float p = 0; for (int i = a; i <= b; ++i) p = jmax (p, v[(size_t) i]); return p;
        };
        auto colX = [&] (int c) { return plot.getX() + (float) c / scale; };
        if (ll)
        {
            auto curve = [&] (const std::vector<float>& v, Colour c, float width, bool fill)
            {
                Path p; bool started = false;
                for (int k = 0; k <= cols; ++k)
                {
                    const float x = colX (k), v0 = peakAt (v, msOf (x), msOf (colX (k + 1)));
                    const float y = plot.getY() + plot.getHeight() * jmax (floorDb, 20.0f * std::log10 (v0 + 1e-9f)) / floorDb;
                    if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
                }
                if (fill)
                {
                    Path f (p); f.lineTo (plot.getRight(), plot.getBottom()); f.lineTo (plot.getX(), plot.getBottom()); f.closeSubPath();
                    g.setGradientFill (ColourGradient (c.withAlpha (0.34f), 0.0f, plot.getY(), c.withAlpha (0.10f), 0.0f, plot.getBottom(), false));
                    g.fillPath (f);
                }
                g.setColour (c); g.strokePath (p, PathStrokeType (width));
            };
            if (bd)
            {
                // layered fill: at every pixel column the area under each band's curve takes that band's colour
                // (no mixing of the three into grey); the loudest band sits on top
                const std::vector<float>* bv[3] = { &data.lo, &data.mid, &data.hi };
                const Colour bc[3] = { col::bandLo, col::bandMid, col::bandHi };
                for (int k = 0; k < cols; ++k)
                {
                    const float x0 = colX (k), x1 = colX (k + 1), m0 = msOf (x0), m1 = msOf (x1);
                    float y[3]; int ord[3] = { 0, 1, 2 };
                    for (int i = 0; i < 3; ++i)
                        y[i] = plot.getY() + plot.getHeight() * jmax (floorDb, 20.0f * std::log10 (peakAt (*bv[i], m0, m1) + 1e-9f)) / floorDb;
                    std::sort (ord, ord + 3, [&] (int p, int q) { return y[p] < y[q]; });
                    for (int j = 0; j < 3; ++j)
                    {
                        const float top = y[ord[j]], bot = j < 2 ? y[ord[j + 1]] : plot.getBottom();
                        if (bot <= top) continue;
                        const float depth = (bot - plot.getY()) / plot.getHeight();
                        g.setColour (bc[ord[j]].withAlpha (0.30f + 0.12f * (1.0f - depth)));
                        g.fillRect (x0, top, x1 - x0 + 0.02f, bot - top);
                    }
                }
                curve (data.lo, col::bandLo, 1.6f, false);
                curve (data.mid, col::bandMid, 1.6f, false);
                curve (data.hi, col::bandHi, 1.6f, false);
            }
            else { curve (data.l, Colour (0xffbfe0ff), 1.4f, false); curve (data.r, col::led.withAlpha (0.9f), 1.4f, false); }
        }
        else
        {
            const float cy = plot.getCentreY(), hh = plot.getHeight() * 0.5f - 2.0f;
            Path pl, pr; pl.startNewSubPath (plot.getX(), cy); pr.startNewSubPath (plot.getX(), cy);
            for (int k = 0; k <= cols; ++k)
            {
                const float x = colX (k), m0 = msOf (x), m1 = msOf (colX (k + 1));
                pl.lineTo (x, cy - hh * peakAt (data.l, m0, m1));
                pr.lineTo (x, cy + hh * peakAt (data.r, m0, m1));
            }
            pl.lineTo (plot.getRight(), cy); pr.lineTo (plot.getRight(), cy); pl.closeSubPath(); pr.closeSubPath();
            g.setColour (Colour (0xffbfe0ff).withAlpha (0.85f)); g.fillPath (pl);
            g.setColour (col::led.withAlpha (0.85f)); g.fillPath (pr);
        }
        if (bd)
        {
            g.setFont (monoFont (11.5f));
            const char* nm[] = { "LOW", "MID", "HIGH" }; const Colour cc[] = { col::bandLo, col::bandMid, col::bandHi };
            for (int i = 0; i < 3; ++i) { g.setColour (cc[i]); g.drawText (nm[i], (int) plot.getRight() - 156 + i * 52, (int) plot.getBottom() - 20, 48, 14, Justification::centredRight); }
        }
        // handles
        for (auto& h : handles())
        {
            const bool hot = h.id == hover || h.id == dragging;
            const Colour c = h.irOnly ? col::amber : col::creamLo.brighter (0.15f);
            g.setColour (c.withAlpha (hot ? 1.0f : 0.85f));
            if (h.vertical)
            {
                for (float y = plot.getY(); y < plot.getBottom(); y += 6.0f) g.drawVerticalLine ((int) h.pos, y, y + 3.0f);
                const float tw = (float) h.label.length() * 8.5f + 12.0f;
                const float tx = h.pos + tw + 4.0f > plot.getRight() ? h.pos - tw + 2.0f : h.pos - 2.0f;
                Rectangle<float> tab (tx, plot.getY() + 2.0f + (float) h.row * 21.0f, tw, 18.0f);
                g.fillRoundedRectangle (tab, 3.0f);
                g.setColour (col::ink); g.setFont (monoFont (11.0f)); g.drawText (h.label, tab, Justification::centred);
            }
            else
            {
                g.drawHorizontalLine ((int) h.pos, h.x0, h.x1);
                Rectangle<float> tab (h.x1 + 4.0f, h.pos - 9.0f, (float) h.label.length() * 8.5f + 12.0f, 18.0f);
                g.fillRoundedRectangle (tab, 3.0f);
                g.setColour (col::ink); g.setFont (monoFont (11.0f)); g.drawText (h.label, tab, Justification::centred);
            }
        }
        g.setColour (col::lcdText); g.setFont (monoFont (13.0f));
        g.drawText ("RT60 " + (data.rt > 0 ? String (data.rt, 2) + " s" : String ("--")) + "   LENGTH " + String (total / 1000.0f, 2) + " s",
                    (int) in.getRight() - 620, (int) in.getY() + 9, 270, 22, Justification::centredRight);
    }

    void mouseMove (const MouseEvent& e) override { const String h = hit (e.position); if (h != hover) { hover = h; repaint(); } setMouseCursor (h.isEmpty() ? MouseCursor::NormalCursor : MouseCursor::DraggingHandCursor); }
    void mouseExit (const MouseEvent&) override { if (hover.isNotEmpty()) { hover = {}; repaint(); } }
    void mouseDown (const MouseEvent& e) override
    {
        dragging = hit (e.position);
        if (dragging.isEmpty()) return;
        if (auto* p = st.getParameter (paramId (dragging))) { p->beginChangeGesture(); startValue = p->convertFrom0to1 (p->getValue()); }
        startPos = e.position;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragging.isEmpty()) return;
        auto* p = st.getParameter (paramId (dragging)); if (p == nullptr) return;
        const bool fine = e.mods.isAltDown();
        const auto plot = plotArea();
        float v = startValue;
        const float pre = raw (sid (slot, "predelay"));
        // fine mode: a tenth of the mouse movement
        const float x = fine ? startPos.x + (e.position.x - startPos.x) * 0.1f : e.position.x;
        const float y = fine ? startPos.y + (e.position.y - startPos.y) * 0.1f : e.position.y;
        if (dragging == "pre") v = msOf (x) - (data.predelayIncluded ? 0.0f : 0.0f);
        else if (dragging == "decay") v = (msOf (x) - pre) / 1000.0f;
        else if (dragging == "build") v = msOf (x) - pre;
        else if (dragging == "xover") v = msOf (x) - pre;
        else if (dragging == "fade") v = totalMs() - msOf (x);
        else if (dragging == "early")
        {
            const float range = logLevel.getToggleState() ? 90.0f : 40.0f;
            v = startValue * std::pow (10.0f, ((startPos.y - y) / plot.getHeight() * range) / 20.0f);
        }
        const auto range = p->getNormalisableRange();
        p->setValueNotifyingHost (p->convertTo0to1 (jlimit (range.start, range.end, v)));
        repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (dragging.isNotEmpty()) if (auto* p = st.getParameter (paramId (dragging))) p->endChangeGesture();
        dragging = {};
    }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        const String h = hit (e.position);
        if (auto* p = st.getParameter (paramId (h))) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    }

private:
    APVTS& st;
    String hover, dragging; float startValue = 0; Point<float> startPos;
    struct Handle { String id, label; bool vertical = true, irOnly = false; float pos = 0, x0 = 0, x1 = 0; int row = 0; };

    Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (4.0f).reduced (58.0f, 0.0f).withTrimmedTop (44.0f).withTrimmedBottom (28.0f).withTrimmedRight (40.0f); }
    float raw (const String& id) const { auto* v = st.getRawParameterValue (id); return v != nullptr ? v->load() : 0.0f; }
    float offsetMs() const { return data.predelayIncluded ? 0.0f : raw (sid (slot, "predelay")); }
    float totalMs() const { return (float) data.l.size() * data.binMs + offsetMs(); }
    float xOf (float ms) const
    {
        const auto plot = plotArea(); const float total = jmax (2.0f, totalMs()), t0 = 1.0f;
        if (logTime.getToggleState()) return plot.getX() + plot.getWidth() * std::log (jmax (t0, ms) / t0) / std::log (total / t0);
        return plot.getX() + plot.getWidth() * ms / total;
    }
    float msOf (float x) const
    {
        const auto plot = plotArea(); const float total = jmax (2.0f, totalMs()), t0 = 1.0f;
        const float u = jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
        return logTime.getToggleState() ? t0 * std::pow (total / t0, u) : u * total;
    }
    String paramId (const String& h) const
    {
        if (h == "pre") return sid (slot, "predelay");
        if (h == "decay") return sid (slot, "decay");
        if (h == "build") return sid (slot, "build");
        if (h == "early") return sid (slot, "early");
        if (h == "xover") return "irXover";
        if (h == "fade") return "irFadeOut";
        return {};
    }
    std::vector<Handle> handles() const
    {
        std::vector<Handle> hs;
        if (! data.valid) return hs;
        const float pre = raw (sid (slot, "predelay")), total = totalMs();
        auto add = [&] (const char* id, const String& label, float ms, bool irOnly, int row) { Handle h; h.id = id; h.label = label; h.irOnly = irOnly; h.row = row; h.pos = xOf (jlimit (0.0f, total, ms)); hs.push_back (h); };
        add ("pre", "PRE", pre, false, 0);
        add ("decay", "RT " + String (raw (sid (slot, "decay")), 2) + "s", pre + raw (sid (slot, "decay")) * 1000.0f, false, 0);
        if (slot == pv::IR)
        {
            const float xo = raw ("irXover");
            add ("build", "FADE IN", pre + raw (sid (slot, "build")), true, 1);
            add ("xover", "ER|LR", pre + xo, true, 2);
            add ("fade", "FADE OUT", total - raw ("irFadeOut"), true, 1);
            // early level: a horizontal handle at the loudest point of the early part
            float pk = 1e-6f;
            for (float ms = pre; ms < pre + jmax (5.0f, xo); ms += data.binMs)
            {
                const int i = (int) ((ms - offsetMs()) / data.binMs);
                if (i >= 0 && i < (int) data.l.size()) pk = jmax (pk, jmax (data.l[(size_t) i], data.r[(size_t) i]));
            }
            const auto plot = plotArea();
            Handle h; h.id = "early"; h.vertical = false; h.irOnly = true; h.label = "EARLY";
            h.pos = logLevel.getToggleState() ? plot.getY() + plot.getHeight() * jmax (-90.0f, 20.0f * std::log10 (pk)) / -90.0f
                                              : plot.getCentreY() - (plot.getHeight() * 0.5f - 2.0f) * pk;
            h.x0 = xOf (pre); h.x1 = xOf (pre + jmax (5.0f, xo));
            hs.push_back (h);
        }
        return hs;
    }
    String hit (Point<float> p) const
    {
        const auto plot = plotArea();
        String best; float bestD = 8.0f;
        for (auto& h : handles())
        {
            float d;
            if (h.vertical) d = (p.y >= plot.getY() && p.y <= plot.getBottom()) ? std::abs (p.x - h.pos) : 1e9f;
            else d = (p.x >= h.x0 - 6.0f && p.x <= h.x1 + 70.0f) ? std::abs (p.y - h.pos) : 1e9f;
            if (d < bestD) { bestD = d; best = h.id; }
        }
        return best;
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
        g.setColour (col::white); g.setFont (uiFont (16.0f, true));
        g.drawText ("SERVICE PAGE  -  OUTPUT TRANSFORMER", 18, 12, getWidth() - 36, 22, Justification::centredLeft);
        g.setColour (col::lcdText.withAlpha (0.8f)); g.setFont (uiFont (12.5f));
        g.drawText ("Models are approximations inspired by classic hardware.", 18, 36, getWidth() - 36, 18, Justification::centredLeft);
        g.setColour (col::white); g.setFont (uiFont (12.5f, true));
        g.drawText ("FREQUENCY", freq.getBounds().translated (0, freq.getHeight() - 2).withHeight (18), Justification::centred);
        g.drawText ("AMOUNT", amount.getBounds().translated (0, amount.getHeight() - 2).withHeight (18), Justification::centred);
        g.drawText ("MODEL", model.getBounds().translated (0, -20).withHeight (18), Justification::centredLeft);
    }
    void resized() override
    {
        engage.setBounds (18, 64, 120, 30);
        model.setBounds (18, 124, 280, 30);
        freq.setBounds (320, 60, 76, 76);
        amount.setBounds (410, 60, 76, 76);
        close.setBounds (getWidth() - 116, getHeight() - 44, 98, 30);
    }
private:
    APVTS& st;
    TextButton engage, close; ComboBox model; Slider freq, amount;
    std::unique_ptr<APVTS::ButtonAttachment> engageAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> modelAtt;
    std::unique_ptr<APVTS::SliderAttachment> freqAtt, amtAtt;
};

// ============================================================================ main panel (fixed design size, scaled by the editor)
class MainPanel : public Component, private Timer
{
public:
    static constexpr int W = 1400, HFull = 1020, HShort = 706;
    enum { kDefaultId = 1, kBuiltinBase = 2, kUserBase = 2000, kSaveId = 9000, kDeleteId = 9001, kFolderId = 9002 };
    std::function<void()> onHeightChanged;

    explicit MainPanel (PlateVerbProcessor& p) : proc (p), wave (p.apvts), service (p.apvts)
    {
        editSlot = jlimit (0, pv::kNumSlots - 1, (int) proc.apvts.state.getProperty ("editSlot", 0));
        collapsed = (bool) proc.apvts.state.getProperty ("collapsed", false);
        for (int s = 0; s < pv::kNumSlots; ++s)
        {
            addButton (sid (s, "on"), kSlotNames[s], false);
            addSlider (sid (s, "lv"), "LEVEL", Fader, false);
            // the algorithm's button switches it on / off and opens it for editing;
            // clicking its fader column only opens it for editing
            auto& b = button (sid (s, "on"));
            b.onClick = [this, s] { selectSlot (s); };
            slider (sid (s, "lv")).addMouseListener (&columnClicks, false);
        }
        const char* rv[][2] = { { "decay", "DECAY" }, { "lowMult", "LOW DEC" }, { "highMult", "HIGH DEC" }, { "size", "SIZE" },
                                { "predelay", "PRE-DLY" }, { "build", "BUILD" }, { "early", "EARLY" }, { "diff", "DIFFUSE" },
                                { "damping", "DAMPING" }, { "tone", "TONE" }, { "hiCut", "HI CUT" }, { "mod", "MOD" }, { "inWidth", "WIDTH" }, { "dirt", "DIRT" } };
        for (auto& e : rv) addSlider (e[0], e[1], Fader, true);
        addSlider ("mix", "MIX", Fader, false);
        addCombo ("matA", {}, "MATERIAL A", true);
        addCombo ("matB", {}, "MATERIAL B", true);
        addSlider ("matMix", "BLEND A - B", Horizontal, true);
        const char* bn[] = { "LOW", "LO-MID", "HI-MID", "HIGH" };
        for (int b = 0; b < 4; ++b) { addSlider (nid ("eqF", b), String (bn[b]) + " HZ", Knob, true); addSlider (nid ("eqG", b), String (bn[b]) + " dB", Knob, true); }
        addButton ("cpOn", "COMP", true);
        addButton ("cpDuck", "DUCK", true);
        addCombo ("cpType", compTypeNames(), "TYPE", true);
        addCombo ("cpTone", StringArray { "Clean", "Vintage" }, "TONE", true);
        const char* cpn[][2] = { { "cpThr", "THRESH" }, { "cpRatio", "RATIO" }, { "cpAtk", "ATTACK" },
                                 { "cpRel", "RELEASE" }, { "cpMakeup", "MAKEUP" }, { "cpMix", "MIX" } };
        for (auto& e : cpn) addSlider (e[0], e[1], Knob, true);
        const char* irk[][2] = { { "irStart", "START" }, { "irXover", "ER/LR X" }, { "irFadeOut", "FADE OUT" },
                                 { "irXfeed", "X-FEED" }, { "irXdelay", "X-DELAY" }, { "irAsym", "ASYM" } };
        for (auto& e : irk) addSlider (e[0], e[1], Knob, false);
        addButton ("irReverse", "REVERSE", false);
        addButton ("irPitch", "STRETCH PITCH", false);
        for (auto* b : { &irLoad, &irPrev, &irNext, &irClear, &presetPrev, &presetNext, &presetSave, &moreBtn })
        {
            b->getProperties().set ("plain", true);
            addAndMakeVisible (*b);
        }
        irLoad.setButtonText ("LOAD");
        irPrev.setButtonText ("<"); irNext.setButtonText (">"); irClear.setButtonText ("CLEAR");
        irLoad.onClick = [this] { chooseIr(); };
        irPrev.onClick = [this] { stepIr (-1); };
        irNext.onClick = [this] { stepIr (1); };
        irClear.onClick = [this] { proc.clearIr(); };
        addButton ("dlOn", "DELAY", false); addButton ("dlSync", "SYNC", false); addButton ("dlPing", "PING", false);
        const char* dln[][2] = { { "dlTimeL", "TIME L" }, { "dlTimeR", "TIME R" }, { "dlFb", "FEEDBACK" }, { "dlMix", "MIX" },
                                 { "dlAtk", "ATTACK" }, { "dlPanL", "PAN L" }, { "dlPanR", "PAN R" }, { "dlWidth", "WIDTH" },
                                 { "dlLow", "LOW" }, { "dlMid", "MID" }, { "dlMidF", "MID HZ" }, { "dlHigh", "HIGH" } };
        for (auto& e : dln) addSlider (e[0], e[1], Knob, false);
        addCombo ("dlDivL", divNames(), "DIV L", false); addCombo ("dlDivR", divNames(), "DIV R", false);
        addCombo ("os", osNames(), "OS", false);
        addSlider ("out", "OUTPUT", Knob, false);
        presetPrev.setButtonText ("<"); presetNext.setButtonText (">"); presetSave.setButtonText ("SAVE");
        presetPrev.onClick = [this] { stepPreset (-1); };
        presetNext.onClick = [this] { stepPreset (1); };
        presetSave.onClick = [this] { savePreset(); };
        moreBtn.onClick = [this] { setCollapsed (! collapsed); };
        presetBox.onChange = [this] { presetChosen(); };
        presetBox.setTextWhenNothingSelected ("Default");
        addAndMakeVisible (presetBox);
        buildPresetMenu();
        addAndMakeVisible (wave);
        addChildComponent (service);

        for (auto* prm : proc.getParameters()) lastValues.add (prm->getValue());
        bindSlot (editSlot);
        setSize (W, collapsed ? HShort : HFull);
        updateCollapse();
        startTimerHz (25);
    }
    ~MainPanel() override { stopTimer(); }
    int designHeight() const { return collapsed ? HShort : HFull; }
    PVLook& getLook() { return look; }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (titleArea.contains (e.getPosition())) { service.setVisible (! service.isVisible()); if (service.isVisible()) service.toFront (true); }
    }

    // ---------------------------------------------------------------- painting
    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (col::panelHi, 0, 0, col::panel, 0, (float) getHeight(), false));
        g.fillAll();
        auto head = getLocalBounds().removeFromTop (56).toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff12306c), 0, 0, col::navy, 0, head.getBottom(), false));
        g.fillRect (head);
        g.setColour (col::white.withAlpha (0.6f)); g.fillRect (0.0f, head.getBottom() - 2.0f, (float) getWidth(), 2.0f);
        g.setColour (col::white); g.setFont (uiFont (21.0f, true));
        g.drawText ("P L A T E V E R B", titleArea.withHeight (28), Justification::centredLeft);
        g.setColour (col::lcdText.withAlpha (0.85f)); g.setFont (uiFont (11.0f, true));
        g.drawText ("DIGITAL REVERBERATION PROCESSOR  /  PV-5", titleArea.getX() + 2, 34, 320, 14, Justification::centredLeft);
        g.setColour (shownDsp > 70.0f ? Colour (0xffff8a65) : col::lcdText); g.setFont (uiFont (13.0f, true));
        g.drawText ("DSP " + String (shownDsp, 1) + " %", dspArea, Justification::centredLeft);
        g.setColour (col::label); g.setFont (uiFont (12.5f, true));
        g.drawText ("PRESET", presetBox.getX() - 64, presetBox.getY(), 58, presetBox.getHeight(), Justification::centredRight);
        g.drawText ("OVERSAMPLING", combo ("os").getX() - 112, combo ("os").getY(), 106, combo ("os").getHeight(), Justification::centredRight);
        g.drawText ("OUTPUT", slider ("out").getX() - 66, slider ("out").getY(), 60, slider ("out").getHeight(), Justification::centredRight);
        {
            auto& e = button (sid (editSlot, "on"));
            auto c0 = Rectangle<int> (e.getX() - 1, slotArea.getY() + 12, e.getWidth() + 2, slotArea.getHeight() - 20).toFloat();
            g.setColour (col::white.withAlpha (0.10f)); g.fillRoundedRectangle (c0, 5.0f);
            g.setColour (col::white.withAlpha (0.55f)); g.drawRoundedRectangle (c0, 5.0f, 1.0f);
        }
        for (auto& s : sections)
        {
            if (! s.visible) continue;
            auto r = s.area.toFloat();
            g.setColour (col::navy.withAlpha (0.28f)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::white.withAlpha (0.45f)); g.drawRoundedRectangle (r, 6.0f, 1.0f);
            const String t = s.perSlot ? s.title + "  -  " + String (kSlotNames[editSlot]) : s.title;
            auto tab = Rectangle<float> (r.getX() + 12.0f, r.getY() - 10.0f, (float) t.length() * 9.0f + 22.0f, 20.0f);
            g.setColour (col::navy); g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (s.perSlot ? Colour (0xffbfe0ff) : col::label); g.setFont (uiFont (13.0f, true));
            g.drawText (t, tab, Justification::centred);
        }
        g.setColour (col::lcdText.withAlpha (0.9f)); g.setFont (uiFont (12.0f, true));
        for (auto& c : captions) if (c.visible) g.drawText (c.title, c.area, Justification::centredLeft);
        for (auto& c : sliders)
        {
            if (! c->slider.isVisible() || c->baseId == "out") continue;
            auto b = c->slider.getBounds();
            const float dim = c->slider.getAlpha();
            g.setColour (col::label.withMultipliedAlpha (dim)); g.setFont (uiFont (12.5f, true));
            if (c->kind == Horizontal) g.drawText (c->label, b.getX(), b.getY() - 20, b.getWidth(), 16, Justification::centredLeft);
            else g.drawText (c->label, b.getX() - 18, b.getBottom() + 2, b.getWidth() + 36, 16, Justification::centred);
            auto vb = valueBox (*c).toFloat();
            bool limited = false;
            const String t = isComp (c->baseId) ? compValueText (c->baseId, limited) : text (c->slider.getName());
            g.setColour (col::lcdBg.withMultipliedAlpha (dim)); g.fillRoundedRectangle (vb, 3.0f);
            g.setColour (col::lcdText.withAlpha (0.25f * dim)); g.drawRoundedRectangle (vb, 3.0f, 1.0f);
            g.setColour ((limited ? col::amber : col::lcdText).withMultipliedAlpha (dim)); g.setFont (monoFont (12.0f));
            g.drawText (t, vb, Justification::centred, false);
        }
        g.setColour (col::label); g.setFont (uiFont (12.5f, true));
        for (auto& c : combos)
            if (c->box.isVisible() && c->baseId != "os")
                g.drawText (c->label, c->box.getX(), c->box.getY() - 18, c->box.getWidth(), 16, Justification::centredLeft);
        if (! collapsed) paintCompMeters (g);
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
        g.setColour (col::amber); g.fillRect (in.getX() - 4.0f, ty - 1.0f, in.getWidth() + 8.0f, 2.5f);
        const float grH = gr.getHeight() * grDb / 30.0f;
        g.setColour (col::led); g.fillRect (gr.getX() + 3.0f, gr.getY(), gr.getWidth() - 6.0f, grH);
        g.setFont (monoFont (11.0f));
        for (int d = 0; d <= 80; d += 20)
        {
            const float y = in.getBottom() - in.getHeight() * (80.0f - (float) d) / 80.0f;
            g.setColour (col::white.withAlpha (0.6f));
            g.drawText (String (-d), (int) in.getX() - 30, (int) y - 7, 26, 14, Justification::centredRight);
        }
        for (int d = 0; d <= 30; d += 10)
        {
            const float y = gr.getY() + gr.getHeight() * (float) d / 30.0f;
            g.setColour (col::white.withAlpha (0.6f));
            g.drawText (String (-d), (int) gr.getRight() + 4, (int) y - 7, 28, 14, Justification::centredLeft);
        }
        g.setColour (col::lcdText); g.setFont (monoFont (12.0f));
        g.drawText (inDb <= lo + 0.1f ? String ("-inf") : String (inDb, 1), inMeter.getX() - 18, inMeter.getY() - 20, inMeter.getWidth() + 36, 16, Justification::centred);
        g.drawText (grDb < 0.05f ? String ("0.0") : String (-grDb, 1), grMeter.getX() - 18, grMeter.getY() - 20, grMeter.getWidth() + 36, 16, Justification::centred);
        g.setColour (col::white); g.setFont (uiFont (12.0f, true));
        g.drawText (raw (sid (editSlot, "cpDuck")) > 0.5f ? "DRY" : "IN", inMeter.getX() - 18, inMeter.getBottom() + 3, inMeter.getWidth() + 36, 16, Justification::centred);
        g.drawText ("GR", grMeter.getX() - 18, grMeter.getBottom() + 3, grMeter.getWidth() + 36, 16, Justification::centred);
    }

    // ---------------------------------------------------------------- layout
    void resized() override
    {
        sections.clear(); captions.clear();
        titleArea = { 24, 6, 300, 44 };
        presetBox.setBounds (604, 13, 320, 30);
        presetPrev.setBounds (930, 13, 32, 30); presetNext.setBounds (966, 13, 32, 30); presetSave.setBounds (1004, 13, 66, 30);
        combo ("os").setBounds (1174, 14, 70, 28);
        slider ("out").setBounds (1328, 3, 50, 50);
        moreBtn.setBounds (340, 13, 170, 30);
        // row A: IR panel + viewer
        Rectangle<int> ir (16, 76, 400, 300); addSection (ir, "IMPULSE RESPONSE", false);
        irLoad.setBounds (ir.getX() + 12, ir.getY() + 18, 70, 28);
        irPrev.setBounds (ir.getX() + 92, ir.getY() + 18, 36, 28);
        irNext.setBounds (ir.getX() + 132, ir.getY() + 18, 36, 28);
        irClear.setBounds (ir.getX() + 300, ir.getY() + 18, 88, 28);
        const char* irIds[] = { "irStart", "irXover", "irFadeOut", "irXfeed", "irXdelay", "irAsym" };
        for (int i = 0; i < 6; ++i) slider (irIds[i]).setBounds (ir.getX() + 44 + (i % 3) * 124, ir.getY() + 64 + (i / 3) * 96, 54, 54);
        button ("irReverse").setBounds (ir.getX() + 12, ir.getY() + 266, 150, 26);
        button ("irPitch").setBounds (ir.getX() + 200, ir.getY() + 266, 188, 26);
        wave.setBounds (428, 76, 956, 300);
        service.setBounds (465, 160, 520, 260);
        // row B: algorithms + reverb
        slotArea = { 16, 398, 400, 296 }; addSection (slotArea, "ALGORITHMS", false);
        for (int s = 0; s < pv::kNumSlots; ++s)
        {
            const int x = slotArea.getX() + 8 + s * 78;
            button (sid (s, "on")).setBounds (x, slotArea.getY() + 18, 76, 30);
            slider (sid (s, "lv")).setBounds (x + 19, slotArea.getY() + 82, 38, 186);
        }
        Rectangle<int> rv (428, 398, 956, 296); addSection (rv, "REVERB", true);
        comboBase ("matA").setBounds (rv.getX() + 14, rv.getY() + 34, 270, 28);
        sliderBase ("matMix").setBounds (rv.getX() + 300, rv.getY() + 34, 230, 28);
        comboBase ("matB").setBounds (rv.getX() + 650, rv.getY() + 34, 270, 28);
        const char* rvIds[] = { "decay", "lowMult", "highMult", "size", "predelay", "build", "early", "diff", "damping", "tone", "hiCut", "mod", "inWidth", "dirt", "mix" };
        for (int i = 0; i < 15; ++i) sliderBase (rvIds[i]).setBounds (rv.getX() + 16 + i * 62, rv.getY() + 106, 38, 160);
        // row C (collapsible): EQ, compressor, delay
        const bool vis = ! collapsed;
        Rectangle<int> eq (16, 716, 300, 290); addSection (eq, "REVERB EQ", true, vis);
        for (int b = 0; b < 4; ++b)
        {
            const int x = eq.getX() + 16 + b * 70;
            sliderBase (nid ("eqF", b)).setBounds (x, eq.getY() + 34, 54, 54);
            sliderBase (nid ("eqG", b)).setBounds (x, eq.getY() + 158, 54, 54);
        }
        Rectangle<int> cpA (328, 716, 460, 290); addSection (cpA, "TAIL COMPRESSOR", true, vis);
        buttonBase ("cpOn").setBounds (cpA.getX() + 12, cpA.getY() + 30, 80, 30);
        comboBase ("cpType").setBounds (cpA.getX() + 100, cpA.getY() + 32, 162, 28);
        comboBase ("cpTone").setBounds (cpA.getX() + 270, cpA.getY() + 32, 96, 28);
        const char* cpIds[] = { "cpThr", "cpRatio", "cpAtk", "cpRel", "cpMakeup", "cpMix" };
        for (int i = 0; i < 6; ++i) sliderBase (cpIds[i]).setBounds (cpA.getX() + 26 + (i % 3) * 96, cpA.getY() + 84 + (i / 3) * 100, 54, 54);
        inMeter = { cpA.getX() + 350, cpA.getY() + 96, 16, 146 };
        grMeter = { cpA.getX() + 404, cpA.getY() + 96, 16, 146 };
        buttonBase ("cpDuck").setBounds (cpA.getX() + 374, cpA.getY() + 30, 78, 30);
        Rectangle<int> dl (800, 716, 584, 290); addSection (dl, "STEREO DELAY", false, vis);
        button ("dlOn").setBounds (dl.getX() + 12, dl.getY() + 26, 96, 30);
        button ("dlSync").setBounds (dl.getX() + 116, dl.getY() + 26, 80, 30);
        button ("dlPing").setBounds (dl.getX() + 204, dl.getY() + 26, 80, 30);
        const char* row2[] = { "dlTimeL", "dlTimeR", "dlFb", "dlMix", "dlAtk" };
        for (int i = 0; i < 5; ++i) slider (row2[i]).setBounds (dl.getX() + 34 + i * 112, dl.getY() + 70, 54, 54);
        combo ("dlDivL").setBounds (dl.getX() + 16, dl.getY() + 84, 92, 28);
        combo ("dlDivR").setBounds (dl.getX() + 128, dl.getY() + 84, 92, 28);
        const char* row3[] = { "dlPanL", "dlPanR", "dlWidth", "dlLow", "dlMid", "dlMidF", "dlHigh" };
        for (int i = 0; i < 7; ++i) slider (row3[i]).setBounds (dl.getX() + 22 + i * 78 + (i >= 3 ? 16 : 0), dl.getY() + 186, 50, 50);
        captions.push_back ({ { dl.getX() + 16, dl.getY() + 164, 200, 16 }, "PAN / WIDTH", false, vis });
        captions.push_back ({ { dl.getX() + 270, dl.getY() + 164, 200, 16 }, "ANALOG EQ", false, vis });
    }

private:
    enum Kind { Knob, Fader, Horizontal };
    struct SliderCtl { FineSlider slider; String label, baseId; Kind kind = Knob; bool perSlot = false; std::unique_ptr<APVTS::SliderAttachment> att; };
    struct ButtonCtl { TextButton button; String baseId; bool perSlot = false; std::unique_ptr<APVTS::ButtonAttachment> att; };
    struct ComboCtl  { ComboBox box; String label, baseId; bool perSlot = false; std::unique_ptr<APVTS::ComboBoxAttachment> att; };
    struct Section   { Rectangle<int> area; String title; bool perSlot; bool visible = true; };

    bool inBottomRow (const Component& c) const { return c.getY() >= 700; }
    void setCollapsed (bool c)
    {
        collapsed = c;
        proc.apvts.state.setProperty ("collapsed", c, nullptr);
        resized(); updateCollapse();
        setSize (W, designHeight());
        if (onHeightChanged) onHeightChanged();
        repaint();
    }
    void updateCollapse()
    {
        for (auto* c : getChildren())
            if (c != &service && c != &presetBox && inBottomRow (*c)) c->setVisible (! collapsed);
        moreBtn.setButtonText (collapsed ? "EQ / COMP / DLY  v" : "EQ / COMP / DLY  ^");
    }

    Rectangle<int> valueBox (const SliderCtl& c) const
    {
        const auto& s = c.slider;
        if (c.kind == Fader) return { s.getX() - 12, s.getY() - 28, s.getWidth() + 24, 20 };
        if (c.kind == Horizontal) return { s.getRight() + 8, s.getY() + 4, 92, 20 };
        return { s.getX() - 18, s.getBottom() + 19, s.getWidth() + 36, 18 };
    }
    static bool isComp (const String& id) { return id.startsWith ("cp") && id != "cpOn" && id != "cpDuck"; }

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

    void addSlider (const String& id, const String& label, Kind kind, bool perSlot)
    {
        auto c = std::make_unique<SliderCtl>();
        c->baseId = id; c->perSlot = perSlot; c->kind = kind; c->label = label;
        c->slider.setSliderStyle (kind == Fader ? Slider::LinearVertical : kind == Horizontal ? Slider::LinearHorizontal : Slider::RotaryHorizontalVerticalDrag);
        c->slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        c->slider.setPopupDisplayEnabled (true, true, this);
        c->slider.setMouseDragSensitivity (320);
        c->slider.setSliderSnapsToMousePosition (false);
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
        for (auto& c : combos)
            if (c->perSlot)
            {
                c->att.reset(); c->box.setName (sid (s, c->baseId));
                if (c->baseId == "matA" || c->baseId == "matB") { c->box.clear (dontSendNotification); c->box.addItemList (materialNames (s), 1); }
                c->att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, sid (s, c->baseId), c->box);
            }
        const float irAlpha = s == pv::IR ? 1.0f : 0.4f;
        for (auto* b : { &irPrev, &irNext, &irClear }) b->setAlpha (irAlpha);
        for (auto* id : { "irStart", "irXover", "irFadeOut", "irXfeed", "irXdelay", "irAsym" }) slider (id).setAlpha (irAlpha);
        for (auto* id : { "irReverse", "irPitch" }) button (id).setAlpha (irAlpha);
        proc.viewSlot.store (s);
        proc.apvts.state.setProperty ("editSlot", s, nullptr);
        wave.slot = s;
        lastWaveVersion = -1;
        repaint();
    }
    void selectSlot (int s) { if (s != editSlot) bindSlot (s); }
    int slotColumnAt (Point<int> p) const
    {
        for (int s = 0; s < pv::kNumSlots; ++s)
            if (Rectangle<int> (slotArea.getX() + 8 + s * 78, slotArea.getY() + 50, 76, slotArea.getHeight() - 56).contains (p)) return s;
        return -1;
    }
    void mouseDown (const MouseEvent& e) override
    {
        const int s = slotColumnAt (e.getEventRelativeTo (this).getPosition());
        if (s >= 0) selectSlot (s);
    }
    struct ColumnClicks : MouseListener
    {
        MainPanel& m; explicit ColumnClicks (MainPanel& p) : m (p) {}
        void mouseDown (const MouseEvent& e) override { const int s = m.slotColumnAt (e.getEventRelativeTo (&m).getPosition()); if (s >= 0) m.selectSlot (s); }
    } columnClicks { *this };

    Slider& slider (const String& id) { for (auto& c : sliders) if (c->slider.getName() == id) return c->slider; jassertfalse; return sliders.front()->slider; }
    Slider& sliderBase (const String& base) { for (auto& c : sliders) if (c->baseId == base) return c->slider; jassertfalse; return sliders.front()->slider; }
    TextButton& button (const String& id) { for (auto& c : buttons) if (c->button.getName() == id) return c->button; jassertfalse; return buttons.front()->button; }
    TextButton& buttonBase (const String& base) { for (auto& c : buttons) if (c->baseId == base) return c->button; jassertfalse; return buttons.front()->button; }
    ComboBox& combo (const String& id) { for (auto& c : combos) if (c->box.getName() == id) return c->box; jassertfalse; return combos.front()->box; }
    ComboBox& comboBase (const String& base) { for (auto& c : combos) if (c->baseId == base) return c->box; jassertfalse; return combos.front()->box; }
    void addSection (Rectangle<int> r, const String& t, bool perSlot, bool visible = true) { sections.push_back ({ r, t, perSlot, visible }); }

    float raw (const String& id) { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.0f; }
    static String withUnit (RangedAudioParameter& p)
    {
        const auto l = p.getLabel();
        const auto t = p.getCurrentValueAsText();
        const bool num = t.containsAnyOf ("0123456789");
        return t + (l.isEmpty() || l == "%" || l == "x" || ! num ? "" : " ") + (num ? l : String());
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
    // One LOAD for everything: pick one file (its L/R partner is found by name), or select the
    // L and R files together and both are loaded as a pair.
    void chooseIr()
    {
        File start = proc.irFileA().existsAsFile() ? proc.irFileA().getParentDirectory() : File::getSpecialLocation (File::userDocumentsDirectory);
        chooser = std::make_unique<FileChooser> ("Load impulse response (select one file, or the L and R files together)",
                                                 start, "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                              [this] (const FileChooser& fc)
        {
            Array<File> fs;
            for (auto& f : fc.getResults()) if (f.existsAsFile()) fs.add (f);
            if (fs.isEmpty()) return;
            if (fs.size() >= 2)
            {
                File a = fs[0], b = fs[1];
                if (irSide (a) == 1 || irSide (b) == 0) std::swap (a, b);     // put the left file first
                proc.requestIrLoad (a, b, true);
            }
            else loadAuto (fs[0]);
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
        Array<File> list;
        for (auto& f : files) { bool left = true; const File p = findPartner (f, left); if (p.existsAsFile() && ! left) continue; list.add (f); }
        int i = list.indexOf (cur); if (i < 0) i = 0;
        if (list.isEmpty()) return;
        loadAuto (list[(i + d + list.size()) % list.size()]);
    }

    // ---------------------------------------------------------------- timer
    void timerCallback() override
    {
        const int req = proc.presetSlotRequest.exchange (-1);
        if (req >= 0 && req < pv::kNumSlots) { bindSlot (req); buildPresetMenu(); }
        if (++dspTick >= 10)    // update the DSP readout 2.5 times a second so it can be read
        {
            dspTick = 0;
            const float d = std::round (proc.dspLoad.load() * 10.0f) / 10.0f;
            if (d != shownDsp) { shownDsp = d; repaint (dspArea); }
        }

        if (! collapsed)
        {
            const bool sync = raw ("dlSync") > 0.5f;
            slider ("dlTimeL").setVisible (! sync); slider ("dlTimeR").setVisible (! sync);
            combo ("dlDivL").setVisible (sync); combo ("dlDivR").setVisible (sync);
            const bool optoOnly = (int) raw (sid (editSlot, "cpType")) == pv::CompOpto;
            for (auto* b : { "cpAtk", "cpRel" }) { sliderBase (b).setEnabled (! optoOnly); sliderBase (b).setAlpha (optoOnly ? 0.4f : 1.0f); }
        }
        auto& params = proc.getParameters();
        bool changed = false;
        for (int i = 0; i < params.size() && i < lastValues.size(); ++i)
        {
            const float v = params[i]->getValue();
            if (v != lastValues[i]) { lastValues.set (i, v); changed = true; }
        }
        syncPresetBox();

        // viewer title: the IR's name, or the algorithm and its material
        const bool isIr = editSlot == pv::IR;
        String title;
        if (isIr) title = proc.irLayout() == pv::IrNone ? String() : proc.irInfo();
        else
        {
            const auto names = materialNames (editSlot);
            const int a = (int) raw (sid (editSlot, "matA")), b = (int) raw (sid (editSlot, "matB"));
            const float bl = raw (sid (editSlot, "matMix"));
            String mat = names[a];
            if (bl > 0.005f && b != a) mat = names[a] + " / " + names[b] + " " + String (roundToInt (bl * 100.0f)) + "%";
            title = String (kSlotNames[editSlot]) + "  -  " + mat;
        }
        const int ver = proc.waveVersion.load();
        if (ver != lastWaveVersion || title != wave.title)
        {
            lastWaveVersion = ver;
            wave.data = proc.getWave (editSlot);
            wave.title = title;
            wave.emptyText = isIr ? "NO IR LOADED  -  PRESS LOAD" : "...";
            wave.repaint();
        }
        if (changed) { repaint(); wave.repaint(); }
        else { if (! collapsed) repaint (inMeter.getUnion (grMeter).expanded (40, 24)); }
    }

    PlateVerbProcessor& proc;
    PVLook look;
    WaveView wave;
    ServicePanel service;
    ComboBox presetBox;
    TextButton presetPrev, presetNext, presetSave, irLoad, irPrev, irNext, irClear, moreBtn;
    std::vector<std::unique_ptr<SliderCtl>> sliders;
    std::vector<std::unique_ptr<ButtonCtl>> buttons;
    std::vector<std::unique_ptr<ComboCtl>> combos;
    std::vector<Section> sections, captions;
    Rectangle<int> inMeter, grMeter, slotArea, titleArea, dspArea { 214, 10, 104, 22 };
    float shownDsp = 0.0f; int dspTick = 0;
    Array<float> lastValues; int editSlot = 0, lastWaveVersion = -1; bool collapsed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

// ============================================================================ editor: scales the panel to the window
class PlateVerbEditor : public AudioProcessorEditor
{
public:
    explicit PlateVerbEditor (PlateVerbProcessor& p) : AudioProcessorEditor (&p), procRef (p), panel (p)
    {
        setLookAndFeel (&panel.getLook());
        addAndMakeVisible (panel);
        scale = (float) (double) p.apvts.state.getProperty ("uiScale2", 0.95);
        panel.onHeightChanged = [this] { applySize(); };
        setResizable (true, true);
        applySize();
    }
    ~PlateVerbEditor() override { setLookAndFeel (nullptr); }
    void resized() override
    {
        if (applying) return;
        scale = jlimit (0.5f, 2.0f, (float) getWidth() / (float) MainPanel::W);
        procRef.apvts.state.setProperty ("uiScale2", scale, nullptr);
        panel.setTransform (AffineTransform::scale (scale));
        panel.setBounds (0, 0, MainPanel::W, panel.designHeight());
    }

private:
    void applySize()
    {
        const int h = panel.designHeight();
        applying = true;
        setResizeLimits (MainPanel::W / 2, h / 2, MainPanel::W * 2, h * 2);
        getConstrainer()->setFixedAspectRatio ((double) MainPanel::W / h);
        setSize (roundToInt (MainPanel::W * scale), roundToInt (h * scale));
        applying = false;
        panel.setTransform (AffineTransform::scale (scale));
        panel.setBounds (0, 0, MainPanel::W, h);
    }
    PlateVerbProcessor& procRef;
    MainPanel panel;
    float scale = 0.95f; bool applying = false;
};

AudioProcessorEditor* PlateVerbProcessor::createEditor() { return new PlateVerbEditor (*this); }
