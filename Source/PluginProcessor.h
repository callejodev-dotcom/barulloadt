#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <escoita_nucleo/escoita_nucleo.h>
#include "Dobre.h"

// Barullo ADT: a dobre artificial (Abbey Road, 1966). Catro mandos (atraso, variación, nivel da
// dobre e ancho) e o tipo para o día a día; detrás de «Máis», velocidade, desafinar, ton,
// saturación, mestura e volume. Sen latencia: o seco sae tal cal. Os presets de fábrica son os programas.
class BDProcessor : public juce::AudioProcessor
{
public:
    BDProcessor();
    ~BDProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Barullo ADT"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return preset; }
    void setCurrentProgram (int index) override { cargarPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::UndoManager undo;   // desfacer e refacer (declárase antes ca o apvts)
    juce::AudioProcessorValueTreeState apvts;
    // presets propios (.bdpreset en Documentos/Barullo ADT/Presets), A/B e desfacer, do núcleo
    escoita::Estado estado { apvts, &undo, opcionsEstado() };

    // ---- presets de fábrica (desde o fío da interface)
    void cargarPreset (int i);
    int getPreset() const { return preset; }
    juce::String nomePreset() const { return nome; }
    bool presetEditado() const;   // os mandos xa non son os do preset
    // a tecla do tipo: o tipo e o seu atraso, variación, velocidade, desafinar e dobres (nun paso)
    void escollerTipo (int t);

    adt::Axustes axustes() const;   // os parámetros de agora

    // para a pantalla (escríbese desde o fío de audio)
    std::atomic<float> nivelEntrada { 0 }, nivelDobre { 0 }, nivelSaida { 0 };
    std::atomic<float> atrasoAgora[2] { { 0 }, { 0 } }, presenza[2] { { 0 }, { 0 } }, panAgora[3] { { 0 }, { 0 }, { 0 } };
    float uiScale = 1.0f;

private:
    escoita::Estado::Opcions opcionsEstado();
    void fixar (const char* id, float valor);   // un parámetro, coma se o movese o usuario
    int preset = 0;
    juce::String nome;

    adt::Dobre dobre;
    escoita::Teito teito;   // sen anticipación: sen latencia
    double sr = 48000, mestS = 1, volS = 1, bypassS = 0;
    std::atomic<float> *tipo = nullptr, *atraso = nullptr, *variacion = nullptr, *nivel = nullptr, *ancho = nullptr, *velocidade = nullptr,
                       *desafinar = nullptr, *ton = nullptr, *saturacion = nullptr, *dobres = nullptr, *mestura = nullptr,
                       *volume = nullptr, *bypass = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BDProcessor)
};
