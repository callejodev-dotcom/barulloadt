#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    // A familia Barullo: o azul mariño da noite de Vigo, a crema da letra de Escoita e a luz laranxa,
    // nun aparello de estudio (paneis en relevo, pantallas de cristal, visores dourados).
    namespace col
    {
        const juce::Colour noite    { 0xff0c111f };
        const juce::Colour crema    { 0xfffff7c1 };
        const juce::Colour crema2   { 0xffbdb792 };
        const juce::Colour crema3   { 0xff857f67 };
        const juce::Colour laranxa  { 0xffe08b16 };
        const juce::Colour ouro     { 0xffedba23 };
        const juce::Colour verde    { 0xff9dc6a0 };
        const juce::Colour azul     { 0xff8fb0e8 };
        const juce::Colour panel1   { 0xff1c2540 }, panel2 { 0xff111830 };
        const juce::Colour tecla1   { 0xff27314f }, tecla2 { 0xff1a2239 };
        const juce::Colour visor    { 0xff070b16 };
        const juce::Colour cristal1 { 0xff142046 }, cristal2 { 0xff070c1c };
        const juce::Colour lina     { 0xff2a3350 };
    }

    juce::Font body (float size);    // Inter
    juce::Font semi (float size);    // Montserrat SemiBold
    juce::Font cond (float size);    // Montserrat Bold
    juce::Font caps (float size, float tracking = 0.18f);
    juce::String upper (const juce::String&);
    void drawCaps (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, float size, float tracking = 0.18f);
    void glowText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, juce::Colour);
    void paintPanel (juce::Graphics&, juce::Rectangle<float>, float radius);
    void paintWell (juce::Graphics&, juce::Rectangle<float>, float radius);
    void paintGlass (juce::Graphics&, juce::Rectangle<float>, float radius);
    void paintKey (juce::Graphics&, juce::Rectangle<float>, bool over, bool down);
    void paintLed (juce::Graphics&, juce::Point<float>, bool on, juce::Colour = col::laranxa);
    void groupTitle (juce::Graphics&, juce::Rectangle<float>, const juce::String&, juce::Colour = col::laranxa);

    struct Look : juce::LookAndFeel_V4
    {
        Look();
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
        void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
        juce::Font getPopupMenuFont() override { return body (15.0f); }
        void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    };

    // mando co anel de LED e o seu valor nun visor debaixo
    struct Knob : juce::Component
    {
        Knob();
        void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (14)); }
        void paint (juce::Graphics&) override;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::String name;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    // a pantalla: onde cae cada dobre no tempo (o seu atraso nos últimos segundos, coma un rexistro
    // que baixa), onde soa cada un no estéreo e os niveis
    class Pantalla : public juce::Component
    {
    public:
        explicit Pantalla (BDProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        void tick();
    private:
        BDProcessor& proc;
        float entrada = 0, dobre = 0, saida = 0;
        static constexpr int HIST = 120;   // 4 s a 30 Hz
        float hist[2][HIST] {}, histG[2][HIST] {};
        int pos = 0, cheo = 0;
    };

    class MainView : public juce::Component, private juce::Timer
    {
    public:
        static constexpr int W = 900, H = 600;
        explicit MainView (BDProcessor&);
        ~MainView() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;
        void mouseDown (const juce::MouseEvent&) override;   // (global) cada clic, un paso de desfacer
        void mostrarMais (bool);
        void refreshAll();

    private:
        void presetsMenu();
        void functionMenu();
        void showPresetsMenu();
        void refreshState();

        BDProcessor& proc;
        Look look;
        juce::TooltipWindow tooltips { this, 500 };
        juce::TextButton presetBtn, prevBtn, nextBtn, presetsBtn { "Presets" }, abBtn[2], undoBtn, redoBtn, funcBtn;
        juce::TextButton tipoBtn[adt::NUM_TIPOS], dobresBtn, maisBtn;
        std::unique_ptr<juce::ParameterAttachment> tipoAtt, dobresAtt;
        Knob atraso, variacion, nivel, ancho;                                  // os de sempre
        Knob velocidade, desafinar, ton, saturacion, mestura, volume;          // «Máis»
        Pantalla pantalla;
        juce::TextButton newVersion;
        juce::Rectangle<int> headerArea, screenArea, controlsArea;
        bool mais = false;
        juce::SharedResourcePointer<escoita::Actualizacions> versions;
    };
}

class BDEditor : public juce::AudioProcessorEditor
{
public:
    explicit BDEditor (BDProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (ui::col::noite); }
    ui::MainView& view() { return main; }
private:
    BDProcessor& proc;
    ui::MainView main;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BDEditor)
};
