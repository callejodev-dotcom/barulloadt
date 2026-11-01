#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using escoita::U;

BDProcessor::BDProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "BarulloADT", createLayout())
{
    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    tipo = p ("tipo"); atraso = p ("atraso"); variacion = p ("variacion"); nivel = p ("nivel"); ancho = p ("ancho");
    velocidade = p ("velocidade"); desafinar = p ("desafinar"); ton = p ("ton"); saturacion = p ("saturacion");
    dobres = p ("dobres"); mestura = p ("mestura"); volume = p ("volume"); bypass = p ("bypass");
    nome = U (adt::PRESETS[0].nome);
}

juce::AudioProcessorValueTreeState::ParameterLayout BDProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const auto& p0 = adt::PRESETS[0];   // os valores de partida son os do primeiro preset
    const StringArray onOff { "Off", "On" };
    // o texto cun número fixo de decimais (texto → valor → texto dá o mesmo texto)
    auto real = [] (const char* id, const String& nomeP, NormalisableRange<float> r, float def, const char* unidade, int decimais = 1)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, nomeP, r, def, AudioParameterFloatAttributes().withLabel (unidade)
                   .withStringFromValueFunction ([decimais] (float v, int) { return String (v, decimais); }));
    };
    auto centro = [] (float mn, float mx, float c) { NormalisableRange<float> r (mn, mx); r.setSkewForCentre (c); return r; };
    // as opcións: o texto pode volver do DAW noutra codificación, así que abonda co comezo (catro letras)
    auto escolla = [] (const char* id, const String& nomeP, const StringArray& opcions, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, nomeP, opcions, def,
                   AudioParameterChoiceAttributes().withValueFromStringFunction ([opcions] (const String& t)
                   {
                       for (int k = 0; k < opcions.size(); ++k)
                           if (t == opcions[k] || (t.length() >= 4 && opcions[k].startsWith (t.substring (0, 4)))) return (float) k;
                       return (float) jlimit (0, opcions.size() - 1, t.getIntValue() - 1);
                   }));
    };
    l.add (escolla ("tipo", "Tipo", { "Abbey Road", "Dobre real", "Slap", "Coro", "Flanxe" }, p0.tipo));
    l.add (real ("atraso", "Atraso", centro ((float) adt::ATRASO_MIN_MS, (float) adt::ATRASO_MAX_MS, 25.0f), (float) p0.atraso, "ms"));
    l.add (real ("variacion", U ("Variación"), { 0.0f, 100.0f }, (float) p0.variacion * 100.0f, "%"));
    l.add (real ("nivel", "Nivel da dobre", { (float) adt::NIVEL_MIN_DB, 6.0f, 0.1f }, (float) p0.nivel, "dB"));
    l.add (real ("ancho", "Ancho", { 0.0f, 100.0f }, (float) p0.ancho * 100.0f, "%"));
    l.add (real ("velocidade", "Velocidade", centro (0.05f, 5.0f, 0.6f), (float) p0.velocidade, "Hz", 2));
    l.add (real ("desafinar", "Desafinar", { 0.0f, 25.0f }, (float) p0.desafinar, "cents"));
    l.add (real ("ton", "Ton", centro (2000.0f, 20000.0f, 8000.0f), (float) p0.ton, "Hz", 0));
    l.add (real ("saturacion", U ("Saturación"), { 0.0f, 100.0f }, (float) p0.saturacion * 100.0f, "%"));
    l.add (escolla ("dobres", "Dobres", { "1", "2" }, p0.dobres - 1));
    l.add (real ("mestura", "Mestura", { 0.0f, 100.0f }, 100.0f, "%"));
    l.add (real ("volume", "Volume", { -24.0f, 6.0f, 0.1f }, 0.0f, "dB"));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bypass", 1 }, "Bypass", onOff, 0));
    return l;
}

// ================================================================ presets
int BDProcessor::getNumPrograms() { return adt::NUM_PRESETS; }
const juce::String BDProcessor::getProgramName (int i) { return U (adt::PRESETS[juce::jlimit (0, adt::NUM_PRESETS - 1, i)].nome); }

static std::vector<std::pair<const char*, float>> valoresDe (const adt::Preset& p)
{
    return { { "tipo", (float) p.tipo }, { "atraso", (float) p.atraso }, { "variacion", (float) p.variacion * 100.0f },
             { "nivel", (float) p.nivel }, { "ancho", (float) p.ancho * 100.0f }, { "velocidade", (float) p.velocidade },
             { "desafinar", (float) p.desafinar }, { "ton", (float) p.ton }, { "saturacion", (float) p.saturacion * 100.0f },
             { "dobres", (float) (p.dobres - 1) }, { "mestura", 100.0f } };
}

void BDProcessor::fixar (const char* id, float v)
{
    if (auto* prm = apvts.getParameter (id))
    {
        const float n = prm->convertTo0to1 (v);
        if (std::abs (prm->getValue() - n) < 1.0e-6f) return;
        prm->beginChangeGesture(); prm->setValueNotifyingHost (n); prm->endChangeGesture();
    }
}

void BDProcessor::cargarPreset (int i)
{
    i = juce::jlimit (0, adt::NUM_PRESETS - 1, i);
    const auto antes = std::make_pair (preset, nome);
    for (auto [id, v] : valoresDe (adt::PRESETS[i])) fixar (id, v);
    const auto despois = std::make_pair (i, U (adt::PRESETS[i].nome));
    // o número e o nome do preset tamén se desfán (os parámetros xa os leva o desfacer do APVTS)
    estado.cambio (U ("preset"), [this, despois] { preset = despois.first; nome = despois.second; },
                                 [this, antes] { preset = antes.first; nome = antes.second; });
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void BDProcessor::escollerTipo (int t)
{
    t = juce::jlimit (0, adt::NUM_TIPOS - 1, t);
    const auto& p = adt::PARTIDA[t];
    fixar ("tipo", (float) t);
    fixar ("atraso", (float) p.atraso);
    fixar ("variacion", (float) p.variacion * 100.0f);
    fixar ("velocidade", (float) p.velocidade);
    fixar ("desafinar", (float) p.desafinar);
    fixar ("dobres", (float) (p.dobres - 1));
}

bool BDProcessor::presetEditado() const
{
    const int i = juce::jlimit (0, adt::NUM_PRESETS - 1, preset);
    if (nome != U (adt::PRESETS[i].nome)) return false;   // un preset propio
    for (auto [id, v] : valoresDe (adt::PRESETS[i]))
        if (auto* prm = apvts.getParameter (id))
            if (std::abs (prm->getValue() - prm->convertTo0to1 (v)) > 0.002f) return true;
    return false;
}

escoita::Estado::Opcions BDProcessor::opcionsEstado()
{
    escoita::Estado::Opcions o;
    o.extension = "bdpreset";
    o.excluir = { "bypass" };
    // o nome do que se está a escoitar vai co estado (un preset propio leva o seu)
    o.extra = [this] { juce::ValueTree t ("Preset"); t.setProperty ("numero", preset, nullptr); t.setProperty ("nome", nome, nullptr); return t; };
    o.aplicarExtra = [this] (const juce::ValueTree& t) { preset = juce::jlimit (0, adt::NUM_PRESETS - 1, (int) t.getProperty ("numero", 0)); nome = t.getProperty ("nome", nome).toString(); };
    return o;
}

// os dB en pasos de 0,1: a 0 dB, a ganancia é 1 xusto
static double redondoDb (float db) { return std::round ((double) db * 10.0) / 10.0; }
static double ganancia (float db) { return juce::Decibels::decibelsToGain (redondoDb (db)); }

adt::Axustes BDProcessor::axustes() const
{
    adt::Axustes a;
    a.tipo = (int) std::lround (tipo->load());
    a.atraso = atraso->load(); a.variacion = variacion->load() / 100.0; a.nivelDb = redondoDb (nivel->load());
    a.ancho = ancho->load() / 100.0; a.velocidade = velocidade->load(); a.desafinar = desafinar->load();
    a.ton = ton->load(); a.saturacion = saturacion->load() / 100.0; a.dobres = dobres->load() > 0.5f ? 2 : 1;
    return a;
}

double BDProcessor::getTailLengthSeconds() const
{
    const auto a = axustes();
    return (a.atraso * adt::SEGUNDA + adt::Dobre::movementoMs (a) * adt::SEGUNDA + 25.0) / 1000.0;
}

// ================================================================ audio
bool BDProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void BDProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    dobre.preparar (sampleRate);
    dobre.axustar (axustes());
    teito.preparar (sampleRate);   // sen anticipación: o ADT non engade latencia ningunha
    setLatencySamples (0);
    mestS = mestura->load() / 100.0;
    volS = ganancia (volume->load());
    bypassS = bypass->load() > 0.5f ? 1.0 : 0.0;
}

void BDProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    float* L = buffer.getWritePointer (0);
    float* R = outs > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R != nullptr && ins < 2) std::copy (L, L + n, R);   // mono → estéreo

    dobre.axustar (axustes());
    const double mestT = mestura->load() / 100.0, volT = ganancia (volume->load()), bypassT = bypass->load() > 0.5f ? 1.0 : 0.0;
    const double sm = 1.0 - std::exp (-1.0 / (0.02 * sr));
    auto ir = [sm] (double& s, double t) { s += (t - s) * sm; if (std::abs (t - s) < 1e-9) s = t; };
    float pe = 0, pd = 0, ps = 0;
    for (int i = 0; i < n; ++i)
    {
        ir (mestS, mestT); ir (volS, volT); ir (bypassS, bypassT);
        const double dl = L[i], dr = R != nullptr ? R[i] : L[i];
        double wl, wr;
        dobre.procesar (dl, dr, wl, wr);
        // a mestura: o son tal cal ou o procesado (a 0 %, o son tal cal; a 100 %, só o procesado)
        double l = (dl * (1.0 - mestS) + wl * mestS) * volS, r = (dr * (1.0 - mestS) + wr * mestS) * volS;
        if (R != nullptr) teito.procesar (l, r);
        else l = r = teito.procesar (0.5 * (l + r));
        // o bypass deixa pasar o son tal cal (despois do teito); a dobre segue a soar por dentro
        l = l * (1.0 - bypassS) + dl * bypassS; r = r * (1.0 - bypassS) + dr * bypassS;
        L[i] = (float) l;
        if (R != nullptr) R[i] = (float) r;
        pe = std::max ({ pe, (float) std::abs (dl), (float) std::abs (dr) });
        pd = std::max ({ pd, (float) std::abs (wl - dl), (float) std::abs (wr - dr) });
        ps = std::max ({ ps, (float) std::abs (l), (float) std::abs (r) });
    }
    for (int ch = 2; ch < outs; ++ch) buffer.clear (ch, 0, n);
    nivelEntrada.store (pe); nivelDobre.store (pd); nivelSaida.store (ps);
    for (int v = 0; v < 2; ++v) { atrasoAgora[v].store ((float) dobre.atrasoAgora (v)); presenza[v].store ((float) dobre.ganancia (v)); }
    for (int v = -1; v < 2; ++v) panAgora[v + 1].store ((float) dobre.pan (v));
}

// ================================================================ estado
void BDProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    st.setProperty ("preset", preset, nullptr);
    st.setProperty ("nomePreset", nome, nullptr);
    st.setProperty ("uiScale", uiScale, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, destData);
}

void BDProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    uiScale = juce::jlimit (0.5f, 2.0f, (float) tree.getProperty ("uiScale", 1.0f));
    preset = juce::jlimit (0, adt::NUM_PRESETS - 1, (int) tree.getProperty ("preset", 0));
    nome = tree.getProperty ("nomePreset", U (adt::PRESETS[preset].nome)).toString();
    apvts.replaceState (tree);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

juce::AudioProcessorEditor* BDProcessor::createEditor() { return new BDEditor (*this); }

#ifndef BARULLO_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BDProcessor(); }
#endif
