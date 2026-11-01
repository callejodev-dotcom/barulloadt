// Proba sen interface: o atraso da dobre (medido), o movemento (profundidade, velocidade e cents),
// desafinar, a lectura entre mostras, a colocación (mono e estéreo), sen clics ao cambiar de tipo e
// ao mover os mandos, a mestura, o nivel e o bypass bit a bit, todos os presets, o estado, o A/B, os
// presets propios e o desfacer, o texto dos parámetros, o limitador, o tamaño de bloque e o consumo.
//   BDRenderTest [saida.wav]                          proba enteira (devolve o número de fallos)
//   BDRenderTest --snapshot saida.png [mais] [preset] a interface (con «mais», o panel Máis aberto)
//   BDRenderTest --render saida.wav [preset]          un preset cun son de proba
//   BDRenderTest --demo preset entrada.wav saida.wav  un preset sobre un son (sempre en estéreo)
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>

using escoita::U;
// o texto con tiles e signos (±, →) vai en UTF-8: o formato, a través de U()
static juce::String fmtUtf8 (const char* f, ...)
{
    char b[2048];
    va_list args;
    va_start (args, f);
    std::vsnprintf (b, sizeof (b), f, args);
    va_end (args);
    return juce::String::fromUTF8 (b);
}
#define FMT(f, ...) fmtUtf8 (f, __VA_ARGS__)

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            std::printf ("WAV: %s\n", f.getFullPathName().toRawUTF8());
        }
}

// pasa un son polo plugin en bloques (o son pode ser máis curto: despois, silencio); sen latencia
static juce::AudioBuffer<float> procesar (BDProcessor& p, const juce::AudioBuffer<float>& in, double len, int blockSize, double sr, bool preparar = true)
{
    if (preparar) { p.setPlayConfigDetails (2, 2, sr, blockSize); p.prepareToPlay (sr, blockSize); }
    const int total = (int) (len * sr);
    juce::AudioBuffer<float> out (2, total), buf (2, blockSize);
    out.clear();
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, total - pos);
        buf.setSize (2, n, false, false, true);
        buf.clear();
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                if (pos + i < in.getNumSamples()) buf.setSample (c, i, in.getSample (c % in.getNumChannels(), pos + i));
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }
    return out;
}

static void setParam (BDProcessor& p, const juce::String& id, float v)
{
    if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

// unha dobre limpa para medir: Abbey Road, quieta, a 0 dB, no centro, sen ton nin saturación
static void limpo (BDProcessor& p, float ms, int tipo = adt::ABBEYROAD)
{
    setParam (p, "tipo", (float) tipo); setParam (p, "atraso", ms); setParam (p, "variacion", 0.0f); setParam (p, "nivel", 0.0f);
    setParam (p, "ancho", 0.0f); setParam (p, "desafinar", 0.0f); setParam (p, "ton", 20000.0f); setParam (p, "saturacion", 0.0f);
    setParam (p, "dobres", 0.0f); setParam (p, "mestura", 100.0f); setParam (p, "volume", 0.0f);
}

static juce::AudioBuffer<float> seno (double sr, double len, double f, float amp)
{
    juce::AudioBuffer<float> b (2, (int) (len * sr));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float x = amp * (float) std::sin (2.0 * adt::PI * f * i / sr);
        b.setSample (0, i, x); b.setSample (1, i, x);
    }
    return b;
}

// un son de proba coma unha voz: notas cun vibrato leve, harmónicos e consoantes de ruído
static juce::AudioBuffer<float> sonDeProba (double sr, double len = 3.0)
{
    juce::AudioBuffer<float> b (2, (int) (len * sr));
    b.clear();
    juce::Random rnd (1966);
    const double notas[] = { 220.0, 246.9, 261.6, 293.7, 329.6, 293.7 };
    const double dur = len / 6.0;
    double fase = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const int k = juce::jmin (5, (int) (t / dur));
        const double tl = t - k * dur;
        const double f = notas[k] * (1.0 + 0.004 * std::sin (2 * adt::PI * 5.5 * t));
        fase += 2 * adt::PI * f / sr;
        const double env = juce::jmin (1.0, tl / 0.03) * juce::jmin (1.0, (dur - tl) / 0.08);
        double x = 0;
        for (int h = 1; h <= 8; ++h) x += std::sin (h * fase) / (h * 1.5);
        x *= 0.12 * env;
        if (tl < 0.04) x += 0.08 * (rnd.nextFloat() * 2 - 1) * (1.0 - tl / 0.04);   // a consoante
        b.setSample (0, i, (float) x); b.setSample (1, i, (float) x);
    }
    return b;
}

// un golpe curto sen media (unha ondiña de Ricker arredor de 3 kHz a 48 kHz, 97 mostras): o paso alto
// da dobre non o deforma, así que o seu sitio mídese ben
static juce::AudioBuffer<float> golpe (float nivel = 0.4f)
{
    const int n = 97;
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
    {
        const double t = (i - n / 2) * 3000.0 / 48000.0, a = adt::PI * adt::PI * t * t;
        const float x = nivel * (float) ((1.0 - 2.0 * a) * std::exp (-a));
        b.setSample (0, i, x); b.setSample (1, i, x);
    }
    return b;
}

static float pico (const juce::AudioBuffer<float>& b, int desde = 0, int ata = -1, int canle = -1)
{
    if (ata < 0) ata = b.getNumSamples();
    desde = juce::jlimit (0, b.getNumSamples(), desde); ata = juce::jlimit (desde, b.getNumSamples(), ata);
    if (canle >= 0) return b.getMagnitude (canle, desde, ata - desde);
    return juce::jmax (b.getMagnitude (0, desde, ata - desde), b.getMagnitude (1, desde, ata - desde));
}
static bool finito (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}
static bool mesmoBit (float a, float b) { return std::memcmp (&a, &b, sizeof (float)) == 0; }
static float diferenza (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float d = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < juce::jmin (a.getNumSamples(), b.getNumSamples()); ++i) d = juce::jmax (d, std::abs (a.getSample (c, i) - b.getSample (c, i)));
    return d;
}
// a dobre soa: a saída menos a entrada (co seco no centro, sae tal cal)
static std::vector<double> soDobre (const juce::AudioBuffer<float>& out, const juce::AudioBuffer<float>& in, int canle)
{
    std::vector<double> d ((size_t) out.getNumSamples(), 0.0);
    for (int i = 0; i < out.getNumSamples(); ++i)
        d[(size_t) i] = (double) out.getSample (canle, i) - (i < in.getNumSamples() ? (double) in.getSample (canle % in.getNumChannels(), i) : 0.0);
    return d;
}

// onde está o golpe dentro de x (en mostras, con decimais): correlación co golpe e parábola no pico
static double ondeGolpe (const std::vector<double>& x, int desde, int ata)
{
    const auto g = golpe();
    auto corr = [&] (int lag) { double s = 0; for (int k = 0; k < g.getNumSamples(); ++k) if (lag + k < (int) x.size()) s += x[(size_t) (lag + k)] * g.getSample (0, k); return s; };
    int mejor = desde; double mx = -1e30;
    for (int l = desde; l < ata; ++l) { const double c = corr (l); if (c > mx) { mx = c; mejor = l; } }
    const double a = corr (mejor - 1), b = mx, c = corr (mejor + 1);
    const double den = a - 2 * b + c;
    return mejor + (std::abs (den) > 1e-30 ? 0.5 * (a - c) / den : 0.0);
}

// o atraso da dobre no tempo (ms), coa fase dun seno de f0 (sr/f0 enteiro): media dun período de
// x·cos e x·sin; a fase, desenrolada, é −ω·atraso (máis unha constante dos filtros)
static std::vector<double> atrasoNoTempo (const std::vector<double>& x, double sr, double f0)
{
    const int P = (int) std::lround (sr / f0);
    const double w = 2 * adt::PI * f0 / sr;
    std::vector<double> d;
    double si = 0, sq = 0, antes = 0, volta = 0;
    for (int i = 0; i < (int) x.size(); ++i)
    {
        si += x[(size_t) i] * std::cos (w * i); sq += x[(size_t) i] * std::sin (w * i);
        if (i >= P) { si -= x[(size_t) (i - P)] * std::cos (w * (i - P)); sq -= x[(size_t) (i - P)] * std::sin (w * (i - P)); }
        if (i < P) { d.push_back (0); continue; }
        double f = std::atan2 (-sq, si);
        if (d.size() > (size_t) P) { while (f + volta - antes > adt::PI) volta -= 2 * adt::PI; while (f + volta - antes < -adt::PI) volta += 2 * adt::PI; }
        antes = f + volta;
        d.push_back (-(f + volta) / (2 * adt::PI * f0) * 1000.0);
    }
    return d;
}

// o movemento medido entre t0 e t1: ± ms (metade de máximo − mínimo), ciclos por segundo e o pico do ton (cents)
struct Movemento { double ms = 0, hz = 0, cents = 0; };
static Movemento medirMovemento (const std::vector<double>& d, double sr, double t0, double t1)
{
    const int a = (int) (t0 * sr), b = juce::jmin ((int) d.size(), (int) (t1 * sr));
    double mn = 1e9, mx = -1e9, media = 0;
    for (int i = a; i < b; ++i) { mn = std::min (mn, d[(size_t) i]); mx = std::max (mx, d[(size_t) i]); media += d[(size_t) i]; }
    media /= (b - a);
    Movemento m;
    m.ms = 0.5 * (mx - mn);
    // os cruces cara arriba pola media (con histérese), e o tempo entre o primeiro e o último
    const double hist = 0.2 * m.ms;
    int primeiro = -1, ultimo = -1, cruces = 0; bool abaixo = false;
    for (int i = a; i < b; ++i)
    {
        const double v = d[(size_t) i] - media;
        if (v < -hist) abaixo = true;
        if (abaixo && v > 0) { abaixo = false; if (primeiro < 0) primeiro = i; else { ultimo = i; ++cruces; } }
    }
    m.hz = cruces > 0 ? cruces / ((ultimo - primeiro) / sr) : 0.0;
    const int paso = (int) (0.01 * sr);
    for (int i = a; i + paso < b; i += paso / 4)
    {
        const double pendente = (d[(size_t) (i + paso)] - d[(size_t) i]) / 1000.0 / (paso / sr);   // s por s
        m.cents = std::max (m.cents, std::abs (1200.0 * std::log2 (1.0 - pendente)));
    }
    return m;
}

// o ton dunha dobre fronte ao orixinal (cents): a mediana da pendente do atraso en anacos de 20 ms
static double centsMedianos (const std::vector<double>& d, double sr, double t0, double t1)
{
    std::vector<double> c;
    const int paso = (int) (0.02 * sr);
    for (int i = (int) (t0 * sr); i + paso < juce::jmin ((int) d.size(), (int) (t1 * sr)); i += paso)
        c.push_back (1200.0 * std::log2 (1.0 - (d[(size_t) (i + paso)] - d[(size_t) i]) / 1000.0 / (paso / sr)));
    if (c.empty()) return 0;
    std::nth_element (c.begin(), c.begin() + (long) c.size() / 2, c.end());
    return c[c.size() / 2];
}

// un biquad (RBJ) para medir
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    Biquad (double fs, double f, bool alto, double q = 0.7071)
    {
        const double w = 2 * adt::PI * f / fs, cw = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        if (alto) { b0 = (1 + cw) / 2 / a0; b1 = -(1 + cw) / a0; b2 = b0; }
        else      { b0 = (1 - cw) / 2 / a0; b1 = (1 - cw) / a0;  b2 = b0; }
        a1 = -2 * cw / a0; a2 = (1 - al) / a0;
    }
    double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

// os clics: o que queda por riba de 5 kHz (paso alto de 4.ª orde) cando o que entra é un seno de 220 Hz
// (un salto de 0,01 nun seno de 0,15 dá uns −40 dB; pídese menos de −70)
static float clics (const juce::AudioBuffer<float>& b, double sr, int desde)
{
    float mx = 0;
    for (int ch = 0; ch < 2; ++ch)
    {
        Biquad f1 (sr, 5000.0, true), f2 (sr, 5000.0, true);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const double y = f2.run (f1.run (b.getSample (ch, i)));
            if (i >= desde) mx = juce::jmax (mx, (float) std::abs (y));
        }
    }
    return mx;
}

// a pegada dun son: FNV-1a de 64 bits das mostras
static uint64_t pegada (const juce::AudioBuffer<float>& b)
{
    uint64_t h = 1469598103934665603ull;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = b.getSample (c, i);
            uint32_t u; std::memcpy (&u, &x, 4);
            for (int k = 0; k < 4; ++k) { h ^= (u >> (8 * k)) & 0xFF; h *= 1099511628211ull; }
        }
    return h;
}

static void snapshot (const juce::File& png, bool mais, int preset)
{
    BDProcessor p;
    const double sr = 48000;
    p.setPlayConfigDetails (2, 2, sr, 512);
    p.prepareToPlay (sr, 512);
    p.cargarPreset (preset >= 0 ? preset : (mais ? 12 : 0));
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* ce = dynamic_cast<BDEditor*> (ed.get());
    ed->setSize (ui::MainView::W, ui::MainView::H);
    if (mais) ce->view().mostrarMais (true);
    const auto son = sonDeProba (sr, 4.0);
    juce::AudioBuffer<float> buf (2, 1600);   // un bloque por cada tick da pantalla (30 Hz)
    for (int blk = 0; blk < 110; ++blk)
    {
        buf.clear();
        for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, son, c, (blk * 1600) % (son.getNumSamples() - 1600), 1600);
        juce::MidiBuffer m; p.processBlock (buf, m);
        ce->view().timerCallback();
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    png.deleteFile();
    juce::FileOutputStream os (png);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("PNG: %s\n", png.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000;
    const juce::String mode = argc > 1 ? juce::String (argv[1]) : juce::String();

    if (mode == "--snapshot" && argc > 2)
    {
        const bool mais = argc > 3 && juce::String (argv[3]) == "mais";
        const int pr = argc > 4 ? atoi (argv[4]) : (argc > 3 && ! mais ? atoi (argv[3]) : -1);
        snapshot (juce::File (juce::String (argv[2])), mais, pr);
        return 0;
    }

    if (mode == "--render" && argc > 2)
    {
        BDProcessor p;
        p.cargarPreset (argc > 3 ? atoi (argv[3]) : 0);
        writeWav (juce::File (juce::String (argv[2])), procesar (p, sonDeProba (sr), 3.3, 512, sr), sr);
        return 0;
    }

    if (mode == "--demo" && argc > 4)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::String (argv[3]))));
        if (r == nullptr) { std::printf ("non se pode ler %s\n", argv[3]); return 1; }
        juce::AudioBuffer<float> in ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&in, 0, (int) r->lengthInSamples, 0, true, true);
        BDProcessor p;
        p.cargarPreset (atoi (argv[2]));
        const double len = (double) in.getNumSamples() / r->sampleRate + p.getTailLengthSeconds();
        writeWav (juce::File (juce::String (argv[4])), procesar (p, in, len, 512, r->sampleRate), r->sampleRate);
        return 0;
    }

    int fails = 0;
    auto check = [&fails] (bool ok, const char* what, const juce::String& extra = {})
    {
        std::printf ("%-58s %s %s\n", what, ok ? "OK" : "FALLO", extra.toRawUTF8());
        fails += ok ? 0 : 1;
    };

    {   // ao abrir: o primeiro preset, cos parámetros nos seus valores de partida (sen editar); sen latencia
        BDProcessor p;
        juce::String mal;
        for (auto* prm : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (std::abs (r->getValue() - r->getDefaultValue()) > 1e-6f) mal << r->getParameterID() << " ";
        p.setPlayConfigDetails (2, 2, sr, 512); p.prepareToPlay (sr, 512);
        check (mal.isEmpty() && p.getPreset() == 0 && ! p.presetEditado() && p.nomePreset() == U (adt::PRESETS[0].nome) && p.getLatencySamples() == 0,
               "ao abrir: o primeiro preset; latencia 0", mal);
    }
    {   // o texto dos parámetros: texto → valor → texto dá o mesmo (coma o clap-validator), e as opcións polo comezo
        BDProcessor p;
        juce::String mal;
        for (auto* prm : p.getParameters())
            for (float v : { 0.0f, 0.013f, 0.25f, 0.5f, 0.777f, 1.0f })
            {
                const auto t = prm->getText (v, 1024), t2 = prm->getText (prm->getValueForText (t), 1024);
                if (t != t2) mal << prm->getName (64) << " (" << t << " / " << t2 << ") ";
            }
        auto* tipo = p.apvts.getParameter ("tipo");
        const bool comezo = std::abs (tipo->getValueForText ("Flan") - tipo->convertTo0to1 (4.0f)) < 1e-6f
                         && std::abs (tipo->getValueForText ("Dobre real") - tipo->convertTo0to1 (1.0f)) < 1e-6f;
        check (mal.isEmpty() && comezo, "texto dos parámetros: ida e volta", mal);
    }
    {   // a lectura entre mostras: plana en calquera posición (sen «zipper» ao mover o atraso) e limpa
        adt::Linea l, r;
        l.preparar (4000); r.preparar (4000);
        double peorDb[3] = { 0, 0, 0 }, peorErro = -300;
        const double fs[3] = { 5000.0, 10000.0, 16000.0 };
        for (int k = 0; k < 3; ++k)
        {
            double mn = 1e9, mx = -1e9;
            for (int q = 0; q <= 16; ++q)
            {
                l.limpar(); r.limpar();
                const double frac = q / 16.0;
                double suma = 0, sumaIdeal = 0, erro = 0;
                for (int i = 0; i < 6000; ++i)
                {
                    const float x = (float) std::sin (2 * adt::PI * fs[k] * i / 48000.0);
                    l.escribir (x); r.escribir (x);
                    if (i < 2000) continue;
                    double yl, yr;
                    adt::lerPar (l, r, 100.0 + frac, yl, yr);
                    const double ideal = std::sin (2 * adt::PI * fs[k] * (i - 100.0 - frac) / 48000.0);
                    suma += yl * yl; sumaIdeal += ideal * ideal; erro += (yl - ideal) * (yl - ideal);
                }
                const double db = 10 * std::log10 (suma / sumaIdeal);
                mn = std::min (mn, db); mx = std::max (mx, db);
                if (k == 0) peorErro = std::max (peorErro, 10 * std::log10 (erro / sumaIdeal));
            }
            peorDb[k] = mx - mn;
        }
        check (peorDb[0] < 0.01 && peorDb[1] < 0.05 && peorDb[2] < 0.3 && peorErro < -70.0, "lectura entre mostras: plana e limpa",
               FMT ("(variación co decimal: %.3f / %.3f / %.3f dB a 5 / 10 / 16 kHz; erro a 5 kHz %.0f dB)", peorDb[0], peorDb[1], peorDb[2], peorErro));
    }
    {   // o atraso medido é o do mando (a 48, 44,1 e 96 kHz)
        juce::String que; bool ok = true;
        struct Caso { double fs; float ms; };
        for (auto c : { Caso { 48000, 1.5f }, Caso { 48000, 12.34f }, Caso { 48000, 24.0f }, Caso { 48000, 95.0f }, Caso { 48000, 150.0f },
                        Caso { 44100, 24.0f }, Caso { 96000, 24.0f } })
        {
            BDProcessor p;
            limpo (p, c.ms);
            const auto in = golpe();
            const auto o = procesar (p, in, 0.25, 512, c.fs);
            const auto d = soDobre (o, in, 0);
            const double ms = ondeGolpe (d, 0, (int) (0.2 * c.fs)) * 1000.0 / c.fs;
            ok = ok && std::abs (ms - c.ms) < 0.03;
            que << juce::String (ms, 3) << " ";
        }
        check (ok, "atraso medido = mando (1,5 / 12,34 / 24 / 95 / 150 ms; 44,1 e 96 kHz)", "(" + que.trim() + " ms)");
    }
    {   // o movemento cun seno: no flanxe (só seno), a profundidade, a velocidade e os cents son os dos mandos
        BDProcessor p;
        limpo (p, 6.0f, adt::FLANXE);
        setParam (p, "variacion", 50.0f); setParam (p, "velocidade", 0.5f);
        const auto in = seno (sr, 9.0, 1000.0, 0.3f);
        const auto o = procesar (p, in, 9.0, 512, sr);
        const auto m = medirMovemento (atrasoNoTempo (soDobre (o, in, 0), sr, 1000.0), sr, 1.0, 9.0);
        const auto a = p.axustes();
        const double ms = adt::Dobre::movementoMs (a), cents = adt::Dobre::centsSeno (a);
        check (std::abs (m.ms / ms - 1) < 0.03 && std::abs (m.hz - 0.5) < 0.01 && std::abs (m.cents / cents - 1) < 0.06,
               "movemento: profundidade, velocidade e cents (flanxe)",
               FMT ("(±%.3f ms de ±%.3f; %.3f Hz de 0,5; ±%.1f cents de ±%.1f)", m.ms, ms, m.hz, m.cents, cents));
    }
    {   // Abbey Road a 1,2 Hz e Dobre real: o movemento queda dentro do que din os mandos e o ton móvese uns cents
        juce::String que; bool ok = true;
        for (int t : { (int) adt::ABBEYROAD, (int) adt::DOBREREAL })
        {
            BDProcessor p;
            limpo (p, t == adt::ABBEYROAD ? 24.0f : 28.0f, t);
            setParam (p, "variacion", t == adt::ABBEYROAD ? 60.0f : 100.0f); setParam (p, "velocidade", t == adt::ABBEYROAD ? 1.2f : 0.35f);
            const auto in = seno (sr, 30.0, 1000.0, 0.3f);
            const auto o = procesar (p, in, 30.0, 4096, sr);
            const auto m = medirMovemento (atrasoNoTempo (soDobre (o, in, 0), sr, 1000.0), sr, 1.0, 30.0);
            const double ms = adt::Dobre::movementoMs (p.axustes());
            // (a deriva ao chou non chega sempre ao extremo; o flutter suma uns 0,02 ms)
            ok = ok && m.ms <= ms * 1.02 + 0.03 && m.ms >= ms * 0.5 && m.cents > 2.0 && m.cents < 30.0;
            que << FMT ("%s ±%.2f de ±%.2f ms, ±%.1f cents; ", t == adt::ABBEYROAD ? "Abbey Road" : "Dobre real", m.ms, ms, m.cents);
        }
        check (ok, "movemento: Abbey Road e Dobre real (deriva)", "(" + que.trim() + ")");
    }
    {   // desafinar: dúas dobres, unha a cada lado: a esquerda sobe e a dereita baixa o que di o mando
        BDProcessor p;
        limpo (p, 24.0f);
        setParam (p, "desafinar", 10.0f); setParam (p, "dobres", 1.0f); setParam (p, "ancho", 100.0f);
        const auto in = seno (sr, 12.0, 1000.0, 0.3f);
        const auto o = procesar (p, in, 12.0, 512, sr);
        const double cl = centsMedianos (atrasoNoTempo (soDobre (o, in, 0), sr, 1000.0), sr, 0.5, 12.0);
        const double cr = centsMedianos (atrasoNoTempo (soDobre (o, in, 1), sr, 1000.0), sr, 0.5, 12.0);
        check (std::abs (cl - 10.0) < 0.3 && std::abs (cr + 10.0) < 0.3, "desafinar: +10 e -10 cents", FMT ("(%+.2f / %+.2f cents)", cl, cr));
    }
    {   // a mestura a 0, o nivel calado (no centro e con dúas dobres) e o bypass: o son tal cal, bit a bit
        const auto son = sonDeProba (sr, 2.0);
        auto forte = [] (BDProcessor& q) { q.cargarPreset (13); setParam (q, "saturacion", 100.0f); setParam (q, "nivel", 6.0f); };
        BDProcessor a; forte (a); setParam (a, "mestura", 0.0f);
        BDProcessor b; forte (b); setParam (b, "nivel", (float) adt::NIVEL_MIN_DB); setParam (b, "ancho", 0.0f); setParam (b, "dobres", 0.0f);
        BDProcessor c; forte (c); setParam (c, "nivel", (float) adt::NIVEL_MIN_DB);   // dúas dobres abertas: o seco segue no centro
        BDProcessor d; forte (d); setParam (d, "bypass", 1.0f); setParam (d, "volume", 6.0f);
        const float da = diferenza (procesar (a, son, 2.0, 512, sr), son), db = diferenza (procesar (b, son, 2.0, 512, sr), son);
        const float dc = diferenza (procesar (c, son, 2.0, 512, sr), son), dd = diferenza (procesar (d, son, 2.0, 512, sr), son);
        check (da == 0.0f && db == 0.0f && dc == 0.0f && dd == 0.0f, "mestura 0, nivel calado e bypass: o son tal cal",
               FMT ("(%g, %g, %g, %g)", da, db, dc, dd));
    }
    {   // cambiar de tipo mentres soa (coa tecla: atraso, variación… nun intre), de cada tipo a cada tipo: sen clics
        float peor = 0, base = 0; juce::String cal; bool fin = true;
        for (int de = 0; de < adt::NUM_TIPOS; ++de)
            for (int a = 0; a < adt::NUM_TIPOS; ++a)
            {
                if (a == de) continue;
                BDProcessor p;
                p.escollerTipo (de);
                setParam (p, "ancho", 60.0f); setParam (p, "nivel", 0.0f);
                p.setPlayConfigDetails (2, 2, sr, 512); p.prepareToPlay (sr, 512);
                const auto in = seno (sr, 1.0, 220.0, 0.15f);
                const auto o1 = procesar (p, in, 1.0, 512, sr, false);
                p.escollerTipo (a);
                juce::AudioBuffer<float> in2 (2, in.getNumSamples());
                for (int i = 0; i < in2.getNumSamples(); ++i)   // o seno segue sen saltar
                    for (int c = 0; c < 2; ++c) in2.setSample (c, i, 0.15f * (float) std::sin (2.0 * adt::PI * 220.0 * (i + in.getNumSamples()) / sr));
                const auto o2 = procesar (p, in2, 1.0, 512, sr, false);
                juce::AudioBuffer<float> o (2, o1.getNumSamples() + o2.getNumSamples());
                for (int c = 0; c < 2; ++c) { o.copyFrom (c, 0, o1, c, 0, o1.getNumSamples()); o.copyFrom (c, o1.getNumSamples(), o2, c, 0, o2.getNumSamples()); }
                base = juce::jmax (base, clics (o1, sr, (int) (0.2 * sr)));
                const float c = clics (o, sr, o1.getNumSamples() - 1);
                fin = fin && finito (o1) && finito (o2);
                if (c > peor) { peor = c; cal = juce::String (de) + "->" + juce::String (a); }
            }
        check (peor < 3e-4f && fin, "cambio de tipo sen clics (os 20 cambios)",
               FMT ("(o peor, %s: %.1f dB sobre 5 kHz; quedo, %.1f dB)", cal.toRawUTF8(), juce::Decibels::gainToDecibels (peor, -200.0f),
                                        juce::Decibels::gainToDecibels (base, -200.0f)));
    }
    {   // automatizar todos os mandos a saltos (cada bloque de 512, valores ao chou, 20 s): sen clics, estable
        BDProcessor p;
        p.cargarPreset (12);
        p.setPlayConfigDetails (2, 2, sr, 512); p.prepareToPlay (sr, 512);
        juce::Random rnd (42);
        const int bloques = (int) (20.0 * sr / 512);
        juce::AudioBuffer<float> todo (2, bloques * 512), buf (2, 512);
        for (int k = 0; k < bloques; ++k)
        {
            if (k % 3 == 0)   // un mando ao chou, a un valor ao chou (coma unha automatización a saltos)
            {
                const char* ids[] = { "atraso", "variacion", "nivel", "ancho", "velocidade", "desafinar", "ton", "saturacion", "dobres", "tipo", "mestura", "volume" };
                auto* prm = p.apvts.getParameter (ids[rnd.nextInt (12)]);
                prm->setValueNotifyingHost (rnd.nextFloat());
                if (prm->getParameterID() == "volume") prm->setValueNotifyingHost (juce::jmin (prm->getValue(), prm->convertTo0to1 (0.0f)));
            }
            for (int i = 0; i < 512; ++i)
                for (int c = 0; c < 2; ++c) buf.setSample (c, i, 0.15f * (float) std::sin (2.0 * adt::PI * 220.0 * (k * 512 + i) / sr));
            juce::MidiBuffer m; p.processBlock (buf, m);
            for (int c = 0; c < 2; ++c) todo.copyFrom (c, k * 512, buf, c, 0, 512);
        }
        const float c = clics (todo, sr, 4096);
        check (c < 3e-4f && finito (todo) && pico (todo) <= 0.8913f, "automatización a saltos (todos os mandos): sen clics",
               FMT ("(%.1f dB sobre 5 kHz; pico %.2f)", juce::Decibels::gainToDecibels (c, -200.0f), pico (todo)));
    }
    {   // unha entrada mono: no centro sae igual polos dous lados; abrindo, a voz a un lado e a dobre ao outro;
        // con dúas dobres abertas, a voz no centro e unha dobre a cada lado (a segunda, un 12 % máis tarde)
        auto mono = [&] (std::function<void (BDProcessor&)> axustar, juce::AudioBuffer<float>& in)
        {
            BDProcessor p;
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (juce::AudioChannelSet::mono()); l.outputBuses.add (juce::AudioChannelSet::stereo());
            const bool vale = p.setBusesLayout (l);
            axustar (p);
            p.prepareToPlay (sr, 512);
            juce::AudioBuffer<float> out (2, in.getNumSamples()), buf (2, 512);
            out.clear();
            for (int pos = 0; pos + 512 <= in.getNumSamples(); pos += 512)
            {
                buf.clear(); buf.copyFrom (0, 0, in, 0, pos, 512);
                juce::MidiBuffer m; p.processBlock (buf, m);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, 512);
            }
            if (! vale) out.clear();
            return out;
        };
        juce::AudioBuffer<float> g (1, 28 * 512);   // (bloques enteiros)
        g.clear();
        const auto gl = golpe();
        for (int i = 0; i < gl.getNumSamples(); ++i) g.setSample (0, 4800 + i, gl.getSample (0, i));
        const auto centro = mono ([] (BDProcessor& p) { limpo (p, 24.0f); }, g);
        bool igual = true;
        for (int i = 0; i < centro.getNumSamples(); ++i) igual = igual && mesmoBit (centro.getSample (0, i), centro.getSample (1, i));
        const auto aberto = mono ([] (BDProcessor& p) { limpo (p, 24.0f); setParam (p, "ancho", 100.0f); }, g);
        bool esquerdaSeca = true;
        for (int i = 0; i < aberto.getNumSamples(); ++i) esquerdaSeca = esquerdaSeca && mesmoBit (aberto.getSample (0, i), g.getSample (0, i));
        std::vector<double> dereita ((size_t) aberto.getNumSamples());
        for (int i = 0; i < aberto.getNumSamples(); ++i) dereita[(size_t) i] = aberto.getSample (1, i);
        const double msR = (ondeGolpe (dereita, 0, (int) (0.2 * sr)) - 4800) * 1000.0 / sr;
        const float secoR = pico (aberto, 4800, 4800 + 97, 1);   // onde estaría o seco pola dereita
        const auto coro = mono ([] (BDProcessor& p) { limpo (p, 24.0f); setParam (p, "dobres", 1.0f); setParam (p, "ancho", 100.0f); }, g);
        juce::AudioBuffer<float> g2 (2, g.getNumSamples());
        g2.copyFrom (0, 0, g, 0, 0, g.getNumSamples()); g2.copyFrom (1, 0, g, 0, 0, g.getNumSamples());
        const double m1 = (ondeGolpe (soDobre (coro, g2, 0), 0, (int) (0.2 * sr)) - 4800) * 1000.0 / sr;
        const double m2 = (ondeGolpe (soDobre (coro, g2, 1), 0, (int) (0.2 * sr)) - 4800) * 1000.0 / sr;
        check (igual && esquerdaSeca && secoR < 1e-4f && std::abs (msR - 24.0) < 0.03 && std::abs (m1 - 24.0) < 0.03 && std::abs (m2 - 24.0 * adt::SEGUNDA) < 0.03,
               "mono: centro, voz e dobre abertas, e dúas dobres",
               FMT ("(dobre pola dereita a %.2f ms; dúas: %.2f / %.2f ms)", msR, m1, m2));
    }
    {   // bloques: con bloques de 512 e de 37 mostras sae o mesmo, mostra a mostra
        auto render = [&] (int bloque)
        {
            BDProcessor p;
            p.cargarPreset (12);   // Tres voces: dúas dobres, desafinar e deriva
            setParam (p, "variacion", 80.0f);
            return procesar (p, sonDeProba (sr), 4.0, bloque, sr);
        };
        const auto x = render (512), y = render (37);
        check (diferenza (x, y) == 0.0f && finito (x) && pico (x) > 0.05f, "mesmo son con bloques de 512 e de 37");
        std::printf ("pegada da saída: %016llx\n", (unsigned long long) pegada (x));
        if (argc > 1 && ! mode.startsWith ("--")) writeWav (juce::File (mode), x, sr);
    }
    {   // os presets: todos soan (a dobre está), sen valores non numéricos e sen tocar o teito
        int mal = 0; float peor = 0; juce::String cal;
        const auto son = sonDeProba (sr);
        for (int i = 0; i < adt::NUM_PRESETS; ++i)
        {
            BDProcessor p;
            p.cargarPreset (i);
            const auto o = procesar (p, son, 3.3, 512, sr);
            double eDobre = 0, eSon = 0;
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < o.getNumSamples(); ++k)
                {
                    const double s = k < son.getNumSamples() ? son.getSample (c, k) : 0.0;
                    eDobre += (o.getSample (c, k) - s) * (o.getSample (c, k) - s); eSon += s * s;
                }
            const double db = 10 * std::log10 (eDobre / eSon);
            if (! finito (o) || db < -24.0 || p.presetEditado()) { ++mal; std::printf ("   mal: %s (%.1f dB)\n", adt::PRESETS[i].nome, db); }
            const float pk = pico (o);
            if (pk > peor) { peor = pk; cal = U (adt::PRESETS[i].nome); }
        }
        check (mal == 0 && peor < 0.8913f, "os presets soan", "(" + juce::String (adt::NUM_PRESETS) + U ("; o máis alto, ") + cal
               + FMT (": %.1f dBFS)", juce::Decibels::gainToDecibels (peor)));
    }
    {   // estado: os parámetros, o preset e a escala gárdanse e recupéranse; soa igual
        BDProcessor a;
        a.cargarPreset (8);
        setParam (a, "ton", 4321.0f); setParam (a, "volume", -3.0f); setParam (a, "atraso", 87.3f);
        a.uiScale = 0.8f;
        juce::MemoryBlock mb; a.getStateInformation (mb);
        BDProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool ok = b.getPreset() == 8 && b.nomePreset() == a.nomePreset() && b.presetEditado() && std::abs (b.uiScale - 0.8f) < 1e-6f;
        const auto x = procesar (a, sonDeProba (sr), 3.0, 512, sr), y = procesar (b, sonDeProba (sr), 3.0, 512, sr);
        const float d = diferenza (x, y);
        // (os mandos con curva poden volver cun bit de menos: 1e-6 é inaudible)
        check (ok && d < 1e-6f, "estado gardado/recuperado (soa igual)", "(" + juce::String ((int) mb.getSize()) + " bytes)");
    }
    {   // A/B e presets propios (do núcleo), co nome do preset
        BDProcessor p;
        p.cargarPreset (0);
        p.estado.cambiarLado();
        p.cargarPreset (10);
        p.estado.cambiarLado();
        bool ok = p.estado.lado() == 0 && p.getPreset() == 0 && ! p.presetEditado();
        p.estado.cambiarLado();
        ok = ok && p.getPreset() == 10 && p.nomePreset() == U (adt::PRESETS[10].nome);
        check (ok, "A/B (os mandos e o preset)");

        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("barulloadt_presets_proba");
        dir.deleteRecursively();
        p.estado.setCarpeta (dir);
        setParam (p, "variacion", 77.0f);
        const bool saved = p.estado.gardar ("Proba: dobre solto");
        p.cargarPreset (3);
        const auto list = p.estado.presets();
        const bool loaded = list.size() == 1 && list[0].hasFileExtension ("bdpreset") && p.estado.cargar (list[0]);
        const float v = p.apvts.getRawParameterValue ("variacion")->load();
        check (saved && loaded && std::abs (v - 77.0f) < 0.01f && p.getPreset() == 10, "presets propios: gardar e cargar");
        dir.deleteRecursively();
    }
    {   // desfacer e refacer: cargar un preset, a tecla dun tipo e mover un mando, cada un no seu paso
        BDProcessor p;
        p.undo.clearUndoHistory();
        p.estado.novoPaso(); p.cargarPreset (5); p.apvts.copyState(); p.estado.novoPaso();
        p.escollerTipo (adt::FLANXE); p.apvts.copyState(); p.estado.novoPaso();
        setParam (p, "nivel", -12.0f); p.apvts.copyState(); p.estado.novoPaso();
        auto valor = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        const bool tipoOk = (int) valor ("tipo") == adt::FLANXE && std::abs (valor ("atraso") - 5.0f) < 1e-4f;
        p.undo.undo();
        const bool u1 = std::abs (valor ("nivel") + 12.0f) > 0.01f && (int) valor ("tipo") == adt::FLANXE;
        p.undo.undo();
        const bool u2 = p.getPreset() == 5 && ! p.presetEditado();
        p.undo.undo();
        const bool u3 = p.getPreset() == 0 && ! p.presetEditado();
        p.undo.redo(); p.undo.redo(); p.undo.redo();
        check (tipoOk && u1 && u2 && u3 && p.getPreset() == 5 && p.presetEditado() && std::abs (valor ("nivel") + 12.0f) < 0.01f,
               "desfacer e refacer (preset, tipo e mandos)");
    }
    {   // os programas do DAW son os presets; a cola é a da dobre máis tarde
        BDProcessor p;
        p.setCurrentProgram (12);
        const bool prog = p.getNumPrograms() == adt::NUM_PRESETS && p.getCurrentProgram() == 12 && p.getProgramName (12) == U (adt::PRESETS[12].nome);
        p.cargarPreset (0);
        const double c0 = p.getTailLengthSeconds();
        p.cargarPreset (9);
        const double c9 = p.getTailLengthSeconds();
        check (prog && c0 > 0.024 && c0 < 0.1 && c9 > 0.115 && c9 < 0.25, "programas do DAW = presets e a cola", FMT ("(%.3f s, %.3f s)", c0, c9));
    }
    {   // o limitador do núcleo: un son forte, coa dobre a +6 dB e o volume a +6 dB, non pasa de −1 dBFS
        BDProcessor p;
        p.cargarPreset (13);
        setParam (p, "nivel", 6.0f); setParam (p, "volume", 6.0f);
        auto son = sonDeProba (sr);
        son.applyGain (4.0f);
        const float pk = pico (procesar (p, son, 3.0, 512, sr));
        check (pk <= 0.8913f, "teito a -1 dBFS (sen latencia)", FMT ("(pico %.4f)", pk));
    }
    {   // o consumo: unha instancia, 20 s de son en estéreo a 48 kHz, co máis pesado (dúas dobres desafinadas)
        auto medir = [&] (int preset)
        {
            BDProcessor p;
            p.cargarPreset (preset);
            const auto son = sonDeProba (sr, 20.0);
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            const auto o = procesar (p, son, 20.0, 512, sr);
            const double t1 = juce::Time::getMillisecondCounterHiRes();
            juce::ignoreUnused (o);
            return (t1 - t0) / 20000.0 * 100.0;   // % dun núcleo
        };
        const double c0 = medir (0), c13 = medir (13);
       #ifdef BARULLO_SANITIZADORES
        const double limite = 1000.0;   // cos sanitizadores vai dez veces máis lento: só se informa
       #else
        const double limite = 5.0;
       #endif
        check (c13 < limite, "consumo por instancia", FMT ("(Abbey Road %.2f %%, Coro ancho %.2f %% dun núcleo)", c0, c13));
        std::printf ("{\"cpu_abbey_road_pct\": %.3f, \"cpu_coro_ancho_pct\": %.3f}\n", c0, c13);
    }
    return fails;
}
