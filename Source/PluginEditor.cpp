#include "PluginEditor.h"

namespace
{
    using vegaui::Knob;
    using vegaui::Toggle;
    juce::String hz(double v)  { return v >= 1000.0 ? juce::String(v / 1000.0, v >= 10000.0 ? 1 : 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; }
    juce::String db(double v)  { return (v > 0.05 ? "+" : "") + juce::String(v, 1) + " dB"; }
    juce::String ms(double v)  { return v < 10.0 ? juce::String(v, 1) + " ms" : juce::String(juce::roundToInt(v)) + " ms"; }
    juce::String pct(double v) { return juce::String(juce::roundToInt(v)) + "%"; }
    juce::String q(double v)   { return juce::String(v, 2); }
    juce::String ratio(double v) { return juce::String(v, 1) + ":1"; }
    juce::String bal(double v) { return std::abs(v) < 0.5 ? juce::String("C") : (v < 0 ? "L" : "R") + juce::String(juce::roundToInt(std::fabs(v))); }
    juce::String fader(double v) { return v <= -79.9 ? juce::String("-inf dB") : db(v); }
}

class ConsoleStripEditor::Content : public juce::Component, private juce::Timer
{
public:
    explicit Content(ConsoleStripProcessor& pr)
        : proc(pr), a(pr.apvts),
          pre(a, "pre", "Preamp", true, db), drive(a, "drive", "Drive", false, pct),
          polarity(a, "polarity", juce::String::fromUTF8("Polarity \xC3\x98")),
          satMode(a, "satmode", "Saturation", "Transformer|Class-A"),
          hpf(a, "hpf", "High-Pass", false, hz), lpf(a, "lpf", "Low-Pass", false, hz),
          hpfOn(a, "hpfon", "HPF In"), lpfOn(a, "lpfon", "LPF In"),
          gThr(a, "gthr", "Threshold", false, db), gRange(a, "grange", "Range", false, db),
          gHold(a, "ghold", "Hold", false, ms), gRel(a, "grel", "Release", false, ms),
          gateOn(a, "gateon", "Gate In"),
          cThr(a, "cthr", "Threshold", false, db), cRatio(a, "cratio", "Ratio", false, ratio),
          cAtk(a, "catk", "Attack", false, ms), cRel(a, "crel", "Release", false, ms),
          cMakeup(a, "cmakeup", "Make-up", false, db),
          compOn(a, "compon", "Comp In"), compPost(a, "comppost", "Position", "Pre-EQ|Post-EQ"),
          hfg(a, "hfg", "HF Gain", false, db),  hff(a, "hff", "HF Freq", false, hz),
          hmg(a, "hmg", "HMF Gain", false, db), hmf(a, "hmf", "HMF Freq", false, hz), hmq(a, "hmq", "HMF Q", false, q),
          lmg(a, "lmg", "LMF Gain", false, db), lmf(a, "lmf", "LMF Freq", false, hz), lmq(a, "lmq", "LMF Q", false, q),
          lfg(a, "lfg", "LF Gain", false, db),  lff(a, "lff", "LF Freq", false, hz),
          hfBell(a, "hfbell", "HF Bell"), lfBell(a, "lfbell", "LF Bell"),
          eqOn(a, "eqon", "EQ In"), propQ(a, "propq", "Prop Q"),
          balance(a, "pan", "Balance", false, bal)
    {
        for (auto* c : std::initializer_list<juce::Component*> {
                 &pre, &drive, &polarity, &satMode, &hpf, &lpf, &hpfOn, &lpfOn,
                 &gThr, &gRange, &gHold, &gRel, &gateOn,
                 &cThr, &cRatio, &cAtk, &cRel, &cMakeup, &compOn, &compPost,
                 &hfg, &hff, &hmg, &hmf, &hmq, &lmg, &lmf, &lmq, &lfg, &lff, &hfBell, &lfBell, &eqOn, &propQ,
                 &balance, &meter, &compGr, &gateGr })
            addAndMakeVisible(c);

        faderSlider.setSliderStyle(juce::Slider::LinearVertical);
        faderSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 16);
        faderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, "fader", faderSlider);
        faderSlider.textFromValueFunction = [](double v) { return fader(v); };
        faderSlider.updateText();
        faderSlider.setDoubleClickReturnValue(true, 0.0);
        addAndMakeVisible(faderSlider);
        faderLabel.setText("Fader", juce::dontSendNotification);
        faderLabel.setJustificationType(juce::Justification::centred);
        faderLabel.setFont(VegaFonts::ui(11.5f));
        addAndMakeVisible(faderLabel);

        meter.capA = "IN"; meter.capB = "OUT";
        compGr.title = "COMP"; gateGr.title = "GATE"; gateGr.maxDb = 40.0f;

        resetButton.onClick = [this] { vegaui::resetAllParameters(proc); };
        addAndMakeVisible(resetButton);
        startTimerHz(30);
    }
    ~Content() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(VegaColours::background);
        vegaui::drawTopBar(g, kBaseW, "CHANNEL STRIP  VP-4");
        vegaui::drawBody(g, { 0, 35, kBaseW, kBaseH - 65 });
        const char* titles[] { "INPUT", "FILTERS", "GATE / EXPANDER", "COMPRESSOR", "4-BAND EQUALIZER", "OUTPUT" };
        for (int i = 0; i < 6; ++i)
            vegaui::drawSection(g, { sx[(size_t) i], 40, sx[(size_t) i + 1] - sx[(size_t) i], kBaseH - 76 }, titles[i], i > 0);
        vegaui::drawBottomBar(g, { 0, kBaseH - 30, kBaseW, 30 }, "PREAMP  /  FILTERS  /  GATE  /  COMPRESSOR  /  4-BAND EQ  /  FADER");
    }

    void resized() override
    {
        sx = { 0, 220, 390, 600, 890, 1290, kBaseW };
        resetButton.setBounds(kBaseW - 70, 7, 60, 20);
        const int r1 = 58, r2 = 196, r3 = 334, kh = 130;
        auto col = [](int x0, int x1, int n, int i) { const int w = (x1 - x0) / n; return juce::Rectangle<int>(x0 + i * w, 0, w, 0); };
        auto put = [&](juce::Component& c, int sec, int n, int i, int y, int h)
        {
            auto r = col(sx[(size_t) sec] + 6, sx[(size_t) sec + 1] - 6, n, i);
            c.setBounds(r.getX(), y, r.getWidth(), h);
        };

        // INPUT
        pre.setBounds(20, r1, 180, 180);
        put(drive, 0, 2, 0, 250, kh); put(polarity, 0, 2, 1, 262, 80);
        satMode.setBounds(40, 382, 150, 56);

        // FILTERS
        put(hpf, 1, 2, 0, r1, kh); put(lpf, 1, 2, 1, r1, kh);
        put(hpfOn, 1, 2, 0, r2 + 10, 80); put(lpfOn, 1, 2, 1, r2 + 10, 80);

        // GATE
        put(gThr, 2, 2, 0, r1, kh); put(gRange, 2, 2, 1, r1, kh);
        put(gHold, 2, 2, 0, r2, kh); put(gRel, 2, 2, 1, r2, kh);
        put(gateOn, 2, 1, 0, r3 + 10, 80);

        // COMPRESSOR
        put(cThr, 3, 3, 0, r1, kh); put(cRatio, 3, 3, 1, r1, kh); put(cAtk, 3, 3, 2, r1, kh);
        put(cRel, 3, 3, 0, r2, kh); put(cMakeup, 3, 3, 1, r2, kh);
        put(compPost, 3, 3, 2, r2 + 30, 60);
        put(compOn, 3, 1, 0, r3 + 10, 80);

        // EQ — columns HF / HMF / LMF / LF, plus switch column
        const int ex0 = sx[4] + 6, ex1 = sx[5] - 76, ew = (ex1 - ex0) / 4;
        auto eq = [&](juce::Component& c, int i, int y, int h) { c.setBounds(ex0 + i * ew, y, ew, h); };
        eq(hfg, 0, r1, kh); eq(hmg, 1, r1, kh); eq(lmg, 2, r1, kh); eq(lfg, 3, r1, kh);
        eq(hff, 0, r2, kh); eq(hmf, 1, r2, kh); eq(lmf, 2, r2, kh); eq(lff, 3, r2, kh);
        eq(hfBell, 0, r3 + 10, 80); eq(hmq, 1, r3, 100); eq(lmq, 2, r3, 100); eq(lfBell, 3, r3 + 10, 80);
        eqOn.setBounds(ex1 + 6, r1 + 20, 64, 80);
        propQ.setBounds(ex1 + 6, r2 + 20, 64, 80);

        // OUTPUT
        balance.setBounds(sx[5] + 8, r1, 96, kh);
        faderLabel.setBounds(sx[5] + 8, r2 - 6, 96, 16);
        faderSlider.setBounds(sx[5] + 20, r2 + 12, 72, 228);
        meter.setBounds(sx[5] + 112, 56, 70, 384);
        compGr.setBounds(sx[5] + 196, 56, 44, 384);
        gateGr.setBounds(sx[5] + 244, 56, 44, 384);
    }

private:
    void timerCallback() override
    {
        meter.setLevels(proc.inPeak.load(), proc.outPeak.load());
        compGr.setGr(juce::jmax(proc.compGr.load(), 0.0f));
        gateGr.setGr(juce::jmax(proc.gateGr.load(), 0.0f));
    }

    ConsoleStripProcessor& proc;
    juce::AudioProcessorValueTreeState& a;
    Knob pre, drive; Toggle polarity, satMode;
    Knob hpf, lpf; Toggle hpfOn, lpfOn;
    Knob gThr, gRange, gHold, gRel; Toggle gateOn;
    Knob cThr, cRatio, cAtk, cRel, cMakeup; Toggle compOn, compPost;
    Knob hfg, hff, hmg, hmf, hmq, lmg, lmf, lmq, lfg, lff; Toggle hfBell, lfBell, eqOn, propQ;
    Knob balance;
    juce::Slider faderSlider; juce::Label faderLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> faderAtt;
    vegaui::StereoMeter meter;
    vegaui::GrColumn compGr, gateGr;
    juce::TextButton resetButton { "RESET" };
    std::array<int, 7> sx {};
};

//==============================================================================
ConsoleStripEditor::ConsoleStripEditor(ConsoleStripProcessor& p) : juce::AudioProcessorEditor(&p)
{
    setLookAndFeel(&lnf);
    content = std::make_unique<Content>(p);
    addAndMakeVisible(*content);
    constrainer.setFixedAspectRatio((double) kBaseW / kBaseH);
    constrainer.setSizeLimits(kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(kBaseW * 85 / 100, kBaseH * 85 / 100);
}
ConsoleStripEditor::~ConsoleStripEditor()
{
    setConstrainer(nullptr);
    content.reset();
    setLookAndFeel(nullptr);
}

void ConsoleStripEditor::resized()
{
    content->setBounds(0, 0, kBaseW, kBaseH);
    content->setTransform(juce::AffineTransform::scale((float) getWidth() / (float) kBaseW));
}
