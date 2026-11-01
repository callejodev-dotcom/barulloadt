// Dobre.h — A dobre artificial (ADT) de Barullo ADT, sen JUCE.
//
// O truco de Abbey Road (1966): a voz grávase nunha máquina e, na mestura, unha segunda máquina
// reprodúcea uns ms despois, coa velocidade movéndose un chisco (o varispeed). A copia chega
// sempre algo tarde e algo desafinada, e cada vez dun xeito: soa coma unha segunda toma.
//
//   entrada ─┬──────────────────────────────── (tal cal, sen latencia) ── colocación ─┐
//            └─ liña (L e R) ─┬─ ler (atraso + movemento) ─ cinta ─ graves ─ ton ─ nivel ─┴─ saída
//                             └─ (a segunda dobre: o seu movemento, ao revés)
//
// - O seco non pasa por nada: nin atraso nin filtros (no centro, as súas mostras saen tal cal).
// - A lectura: un sinc con xanela de Kaiser (16 puntos, 512 fases interpoladas): plano ata ~16 kHz
//   en calquera posición entre mostras, así que mover o atraso non fai «zipper» nin apaga agudos.
// - O movemento: un seno lento (o varispeed da segunda máquina) e unha deriva ao chou alisada
//   (coseno entre puntos ao chou), máis un flutter moi pequeno; o ton móvese uns cents.
// - Desafinar: dúas cabezas que len cun atraso que medra ou mingua sen parar (coma un harmonizador
//   de cinta) e se funden ao saltar; a primeira dobre sobe e a segunda baixa.
// - A cinta: un chisco de saturación asimétrica (2.º harmónico), un paso alto e un paso baixo.
// - Todo vai suavizado mostra a mostra; os saltos grandes do atraso (cambiar de tipo) fúndense en
//   40 ms entre a lectura vella e a nova, e os pequenos (mover o mando) esvaran.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace adt {

constexpr double PI = 3.14159265358979323846;

enum Tipo { ABBEYROAD = 0, DOBREREAL, SLAP, CORO, FLANXE, NUM_TIPOS };

constexpr double ATRASO_MIN_MS = 1.0, ATRASO_MAX_MS = 150.0;
constexpr double NIVEL_MIN_DB = -36.0;   // o mínimo do mando: a dobre calada
constexpr double SEGUNDA = 1.12;         // a segunda dobre chega un 12 % máis tarde (outro cantante)
constexpr double DESVIO_MAX = 0.06;      // o seno nunca move o ton máis dun 6 % (un semitono): o movemento tope é 0,06 / (2π · velocidade)

struct Axustes
{
    int tipo = ABBEYROAD;
    double atraso = 24;         // ms
    double variacion = 0.4;     // 0..1: canto se move
    double nivelDb = -3;        // dB da dobre (NIVEL_MIN_DB ou menos: calada)
    double ancho = 0;           // 0..1: 0, todo no centro; 1, cada un no seu lado
    double velocidade = 0.55;   // Hz do varispeed
    double desafinar = 0;       // cents (a primeira dobre, arriba; a segunda, abaixo)
    double ton = 9000;          // Hz do paso baixo da dobre
    double saturacion = 0.15;   // 0..1
    int dobres = 1;             // 1 ou 2
};

// o carácter de cada tipo: o que non se ve nos mandos
struct Caracter
{
    double profundidade;   // a variación ao 100 %, en fracción do atraso (± ms = variación × isto × atraso)
    double seno;           // canto do movemento é seno (o resto, deriva ao chou)
    double ritmoDeriva;    // a deriva cambia de rumbo a esta fracción da velocidade
    double flutter;        // ± ms de flutter (≈ 6 Hz) ao 100 % de variación
    double graves;         // Hz do paso alto da dobre
    double drive;          // saturación de base (a cinta)
};
inline constexpr Caracter CARACTER[NUM_TIPOS] = {
    //  prof   seno  deriva flutter graves drive
    { 0.12, 0.85, 0.70, 0.020, 100.0, 0.15 },   // Abbey Road: o seno do varispeed, lento
    { 0.40, 0.25, 0.60, 0.010,  90.0, 0.05 },   // Dobre real: sobre todo deriva, coma unha toma de verdade
    { 0.04, 0.60, 0.80, 0.030, 130.0, 0.30 },   // Slap: o eco de cinta, case quedo
    { 0.15, 0.70, 0.70, 0.015, 110.0, 0.10 },   // Coro: dúas dobres que se moven ao revés
    { 0.80, 1.00, 1.00, 0.000,  40.0, 0.05 },   // Flanxe: atraso curto que varre (Tomorrow Never Knows)
};

// o que pon cada tecla do tipo na interface (o resto queda coma estaba)
struct Partida { double atraso, variacion, velocidade, desafinar; int dobres; };
inline constexpr Partida PARTIDA[NUM_TIPOS] = {
    { 24.0, 0.40, 0.55, 0.0, 1 },
    { 28.0, 0.50, 0.35, 3.0, 1 },
    { 95.0, 0.30, 0.60, 0.0, 1 },
    { 22.0, 0.50, 0.70, 7.0, 2 },
    {  5.0, 0.60, 0.18, 0.0, 1 },
};

// ---------------------------------------------------------------- lectura entre mostras
// Sinc con xanela de Kaiser: 16 puntos e 512 fases (interpoladas entre si). Cada fase suma 1 (os graves
// pasan igual). Lese desde 8 mostras atrás (os 8 puntos de diante deben estar xa escritos).
struct Sinc
{
    static constexpr int N = 16, MEDIO = 8, FASES = 512;
    std::vector<float> h, dh;   // (FASES + 1) × N: os coeficientes e a diferenza coa fase seguinte

    static const Sinc& get() { static const Sinc s; return s; }

private:
    static double i0 (double x)   // Bessel modificada de orde 0
    {
        double s = 1, t = 1;
        for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; if (t < 1e-14 * s) break; }
        return s;
    }
    Sinc()
    {
        const double fc = 0.88, beta = 7.0;   // corte (fracción de Nyquist) e xanela
        h.assign ((size_t) (FASES + 1) * N, 0.0f);
        dh.assign ((size_t) (FASES + 1) * N, 0.0f);
        std::vector<double> fila ((size_t) N);
        for (int q = 0; q <= FASES; ++q)
        {
            const double f = (double) q / FASES;
            double suma = 0;
            for (int k = 0; k < N; ++k)
            {
                const double t = (double) (k - (MEDIO - 1)) - f;   // k = 0 → 7 mostras máis nova ca a posición
                const double x = PI * fc * t;
                const double s = std::abs (t) < 1e-12 ? fc : fc * std::sin (x) / x;
                const double u = t / MEDIO;
                const double w = std::abs (u) >= 1.0 ? 0.0 : i0 (beta * std::sqrt (1.0 - u * u)) / i0 (beta);
                fila[(size_t) k] = s * w;
                suma += s * w;
            }
            for (int k = 0; k < N; ++k) h[(size_t) (q * N + k)] = (float) (fila[(size_t) k] / suma);
        }
        for (int q = 0; q < FASES; ++q)
            for (int k = 0; k < N; ++k) dh[(size_t) (q * N + k)] = h[(size_t) ((q + 1) * N + k)] - h[(size_t) (q * N + k)];
    }
};

// liña de atraso: a mostra máis nova está en atrás = 0. Escríbese dúas veces (b[w] e b[w + n]) para
// ler os 16 puntos seguidos sen dar a volta.
struct Linea
{
    std::vector<float> b;
    int n = 8, w = 0;
    void preparar (int maxAtras) { n = std::max (64, maxAtras + 2 * Sinc::N); b.assign ((size_t) (2 * n), 0.0f); w = 0; }
    void limpar() { std::fill (b.begin(), b.end(), 0.0f); }
    void escribir (float x) { if (--w < 0) w = n - 1; b[(size_t) w] = x; b[(size_t) (w + n)] = x; }
    double minimo() const { return (double) Sinc::MEDIO; }
    double maximo() const { return (double) (n - Sinc::N - 2); }
    const float* desde (int atras) const { return &b[(size_t) (w + atras)]; }
};

// lé as dúas canles na mesma posición (atrás, en mostras, con decimais)
inline void lerPar (const Linea& l, const Linea& r, double atras, double& yl, double& yr)
{
    const auto& s = Sinc::get();
    atras = std::clamp (atras, l.minimo(), l.maximo());
    const int i = (int) atras;
    const double fq = (atras - i) * Sinc::FASES;
    const int q = std::min ((int) fq, Sinc::FASES - 1);
    const float a = (float) (fq - q);
    const float* h = &s.h[(size_t) (q * Sinc::N)];
    const float* d = &s.dh[(size_t) (q * Sinc::N)];
    const float* pl = l.desde (i - (Sinc::MEDIO - 1));
    const float* pr = r.desde (i - (Sinc::MEDIO - 1));
    float sl = 0, sr = 0;
    for (int k = 0; k < Sinc::N; ++k)
    {
        const float c = h[k] + a * d[k];
        sl += c * pl[k]; sr += c * pr[k];
    }
    yl = sl; yr = sr;
}

// ---------------------------------------------------------------- filtros
// SVF de dous polos en forma TPT (Zavalishin): pódese mover a frecuencia mostra a mostra sen
// transitorios (un biquad recalculado a pasos fai «zipper» ao xirar o ton)
struct SvfCoef
{
    double k = 1.41421356, a1 = 1, a2 = 0, a3 = 0;
    void fixarFrec (double fs, double f, double q = 0.70710678)
    {
        const double g = std::tan (PI * std::clamp (f, 10.0, fs * 0.45) / fs);
        k = 1.0 / q; a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
};
struct Svf
{
    double ic1 = 0, ic2 = 0;
    // devolve o paso baixo; en «alto», o paso alto
    double run (const SvfCoef& c, double x, double& alto)
    {
        const double v3 = x - ic2, v1 = c.a1 * ic1 + c.a2 * v3, v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        alto = x - c.k * v1 - v2;
        return v2;
    }
    double baixo (const SvfCoef& c, double x) { double a; return run (c, x, a); }
    double altoDe (const SvfCoef& c, double x) { double a; run (c, x, a); return a; }
};

// a cinta: tanh cun chisco de asimetría (2.º harmónico), coa pendente 1 no cero (en baixo nivel, nada)
inline double cinta (double x, double drive)
{
    if (drive < 1e-6) return x;
    constexpr double b = 0.15;
    const double t0 = std::tanh (drive * b);
    return (std::tanh (drive * (x + b)) - t0) / (drive * (1.0 - t0 * t0));
}

// a colocación (balance): no centro, 1 e 1 xusto; á esquerda (p < 0) baixa a dereita e ao revés
inline double balL (double p) { return p > 0 ? 1.0 - p : 1.0; }
inline double balR (double p) { return p < 0 ? 1.0 + p : 1.0; }

// ---------------------------------------------------------------- a dobre
class Dobre
{
public:
    void preparar (double frecuencia)
    {
        fs = frecuencia;
        const double maxMs = ATRASO_MAX_MS * SEGUNDA * (1.0 + 0.45) + 25.0;   // atraso, movemento, cabezas e marxe
        linL.preparar ((int) (maxMs / 1000.0 * fs)); linR.preparar ((int) (maxMs / 1000.0 * fs));
        k20 = coef (0.020); k30 = coef (0.030); k100 = coef (0.100); k150 = coef (0.150); kGlide = coef (0.060);
        fundido = 1.0 / (0.040 * fs);
        salto = 2.0 * fs / 1000.0;   // máis de 2 ms dunha vez: fundir en vez de esvarar
        limpar();   // (o primeiro axustar() pon todo no seu sitio, sen esvarar)
    }

    void limpar()
    {
        linL.limpar(); linR.limpar();
        for (int v = 0; v < 2; ++v)
        {
            auto& z = voz[(size_t) v];
            z = Voz();
            z.semente = v == 0 ? 0x9E3779B9u : 0x85EBCA6Bu;
            z.faseSeno = v == 0 ? 0.0 : 0.5;   // a segunda, ao revés
            z.derB = aoChou (z.semente);
        }
        xf = 0;
        primeira = true;
    }

    // unha vez por bloque: os obxectivos (o resto vai suavizado mostra a mostra)
    void axustar (const Axustes& x)
    {
        a = x;
        const int t = std::clamp (a.tipo, 0, NUM_TIPOS - 1);
        carT = CARACTER[t];
        T = std::clamp (a.atraso, ATRASO_MIN_MS, ATRASO_MAX_MS) * fs / 1000.0;
        vT = std::clamp (a.variacion, 0.0, 1.0);
        rateT = std::clamp (a.velocidade, 0.01, 10.0);
        gT = a.nivelDb <= NIVEL_MIN_DB + 0.05 ? 0.0 : std::pow (10.0, std::min (a.nivelDb, 12.0) / 20.0);
        aT = std::clamp (a.ancho, 0.0, 1.0);
        uT = a.dobres >= 2 ? 1.0 : 0.0;
        cT = std::clamp (a.desafinar, 0.0, 50.0);
        tonT = std::clamp (a.ton, 1000.0, 22000.0);
        satT = std::clamp (a.saturacion, 0.0, 1.0) * 3.0;
        if (primeira)   // ao empezar, xa no sitio (sen esvarar desde cero)
        {
            car = carT; D = Dn = T; vS = vT; rateS = rateT; gS = gT; aS = aT; uS = uT; cS = cT; tonS = tonT; satS = satT;
            detMix = cT > 0 ? 1.0 : 0.0;
            primeira = false;
            filtros();
        }
    }

    // unha mostra: entra o son (L e R) e sae o procesado (o seco colocado e as dobres)
    void procesar (double inL, double inR, double& oL, double& oR)
    {
        suavizar();
        if (std::abs (tonS - frecLp) > 0.0 || std::abs (car.graves - frecHp) > 0.0) filtros();   // (só mentres se moven)

        // o atraso base: os saltos grandes fúndense, os pequenos esvaran
        if (xf <= 0)
        {
            if (std::abs (T - D) > salto) { Dn = T; xf = 1e-12; }
            else D += (T - D) * kGlide;
        }
        else Dn += (T - Dn) * kGlide;

        linL.escribir ((float) inL); linR.escribir ((float) inR);

        double d[2][2] = { { 0, 0 }, { 0, 0 } };
        const int voces = (uS > 0 || uT > 0) ? 2 : 1;
        for (int v = 0; v < voces; ++v)
        {
            auto& z = voz[(size_t) v];
            const double forma = car.seno * std::sin (2 * PI * z.faseSeno) + (1.0 - car.seno) * deriva (z);
            const double fl = vS * car.flutter * fs / 1000.0 * std::sin (2 * PI * z.faseFlutter);
            const double esc = v == 0 ? 1.0 : SEGUNDA;
            double l, r;
            if (xf > 0)
            {
                double l2, r2;
                ler (v, D * esc, forma, fl, l, r);
                ler (v, Dn * esc, forma, fl, l2, r2);
                const double gv = std::cos (xf * PI / 2), gn = std::sin (xf * PI / 2);
                l = l * gv + l2 * gn; r = r * gv + r2 * gn;
            }
            else ler (v, D * esc, forma, fl, l, r);

            // a cinta, os graves e o ton
            l = cinta (l, satS + car.drive); r = cinta (r, satS + car.drive);
            l = z.lpL.baixo (lp, z.hpL.altoDe (hp, l)); r = z.lpR.baixo (lp, z.hpR.altoDe (hp, r));
            d[v][0] = l; d[v][1] = r;

            // avanzar o movemento
            z.faseSeno += rateS * (v == 0 ? 1.0 : 0.87) / fs; if (z.faseSeno >= 1) z.faseSeno -= 1;
            z.faseFlutter += (v == 0 ? 5.7 : 6.3) / fs; if (z.faseFlutter >= 1) z.faseFlutter -= 1;
            z.derPos += rateS * car.ritmoDeriva * (v == 0 ? 1.0 : 0.9) / fs;
            if (z.derPos >= 1) { z.derPos -= 1; z.derA = z.derB; z.derB = aoChou (z.semente); }
            if (detMix > 0)
            {
                const double W = xanela (D * esc);
                z.faseDet += std::abs (std::exp2 (cS / 1200.0) - 1.0) / W;
                if (z.faseDet >= 1) z.faseDet -= 1;
            }
            z.atrasoAgora = (D * esc + profundidade (D * esc) * forma + fl) * 1000.0 / fs;
        }
        if (xf > 0) { xf += fundido; if (xf >= 1) { D = Dn; xf = 0; } }

        // a colocación: cunha dobre, o seco vai cara á esquerda e a dobre cara á dereita; con dúas,
        // o seco no centro e unha a cada lado. Ancho 0: todo no centro (o seco, tal cal)
        const double pSeco = -aS * (1.0 - uS), p1 = aS * (1.0 - 2.0 * uS), p2 = aS;
        oL = inL * balL (pSeco) + gS * (d[0][0] * balL (p1) + uS * d[1][0] * balL (p2));
        oR = inR * balR (pSeco) + gS * (d[0][1] * balR (p1) + uS * d[1][1] * balR (p2));
    }

    // para a pantalla
    double atrasoAgora (int v) const { return voz[(size_t) (v & 1)].atrasoAgora; }
    double ganancia (int v) const { return v == 0 ? gS : gS * uS; }
    double pan (int v) const { return v < 0 ? -aS * (1.0 - uS) : v == 0 ? aS * (1.0 - 2.0 * uS) : aS; }

    // o que se ve nos mandos: ± ms de movemento e, aproximado, canto se move o ton (cents)
    static double movementoMs (const Axustes& x)
    {
        const auto& c = CARACTER[std::clamp (x.tipo, 0, NUM_TIPOS - 1)];
        return std::min (std::clamp (x.variacion, 0.0, 1.0) * c.profundidade * std::clamp (x.atraso, ATRASO_MIN_MS, ATRASO_MAX_MS),
                         1000.0 * DESVIO_MAX / (2 * PI * std::clamp (x.velocidade, 0.01, 10.0)));
    }
    static double centsSeno (const Axustes& x)   // o pico do seno (a deriva vai aparte, máis lenta)
    {
        const auto& c = CARACTER[std::clamp (x.tipo, 0, NUM_TIPOS - 1)];
        const double desv = 2 * PI * x.velocidade * c.seno * movementoMs (x) / 1000.0;
        return 1200.0 * std::log2 (1.0 + std::min (desv, 0.5));
    }

private:
    struct Voz
    {
        double faseSeno = 0, faseFlutter = 0, faseDet = 0.25, derA = 0, derB = 0, derPos = 0, atrasoAgora = 0;
        uint32_t semente = 1;
        Svf hpL, hpR, lpL, lpR;
    };

    double coef (double seg) const { return 1.0 - std::exp (-1.0 / (seg * fs)); }

    static double aoChou (uint32_t& s)   // xorshift32: o mesmo en todos os sistemas
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (double) s / 2147483647.5 - 1.0;
    }
    static double deriva (const Voz& z)   // entre dous puntos ao chou, en coseno (o ton non salta)
    {
        return z.derA + (z.derB - z.derA) * 0.5 * (1.0 - std::cos (PI * z.derPos));
    }

    // o movemento (± mostras) para un atraso base B: a variación, co tope do ton
    double profundidade (double B) const { return std::min (vS * car.profundidade * B, DESVIO_MAX * fs / (2 * PI * rateS)); }

    // a xanela das cabezas de desafinar: un 40 % do atraso, entre 1 e 20 ms
    double xanela (double B) const { return std::clamp (0.4 * B, fs * 0.001, fs * 0.020); }

    // o peso de cada cabeza: enteira media volta e fúndese no 10 % ao chegar e ao marchar (as dúas suman 1)
    static double peso (double p)
    {
        constexpr double x = 0.1;
        if (p < x) { const double s = std::sin (PI / 2 * p / x); return s * s; }
        if (p < 0.5) return 1.0;
        if (p < 0.5 + x) { const double c = std::cos (PI / 2 * (p - 0.5) / x); return c * c; }
        return 0.0;
    }

    // unha dobre con atraso base B (mostras): o movemento e, se desafina, as dúas cabezas
    void ler (int v, double B, double forma, double fl, double& l, double& r) const
    {
        const auto& z = voz[(size_t) v];
        const double pos = B + profundidade (B) * forma + fl;
        l = r = 0;
        if (detMix < 1.0)
        {
            double pl, pr;
            lerPar (linL, linR, pos, pl, pr);
            l += pl * (1.0 - detMix); r += pr * (1.0 - detMix);
        }
        if (detMix > 0.0)
        {
            const double W = xanela (B), signo = v == 0 ? 1.0 : -1.0;   // a primeira sobe (o atraso mingua), a segunda baixa
            for (int h = 0; h < 2; ++h)
            {
                double p = z.faseDet + 0.5 * h; if (p >= 1) p -= 1;
                const double w = peso (p);
                if (w <= 0) continue;
                double hl, hr;
                lerPar (linL, linR, pos + signo * W * (0.25 - p), hl, hr);
                l += hl * w * detMix; r += hr * w * detMix;
            }
        }
    }

    void suavizar()
    {
        auto ir = [] (double& s, double t, double k) { s += (t - s) * k; if (std::abs (t - s) < 1e-9) s = t; };
        ir (car.profundidade, carT.profundidade, k150); ir (car.seno, carT.seno, k150); ir (car.ritmoDeriva, carT.ritmoDeriva, k150);
        ir (car.flutter, carT.flutter, k150); ir (car.graves, carT.graves, k150); ir (car.drive, carT.drive, k150);
        ir (vS, vT, k100); ir (rateS, rateT, k100); ir (gS, gT, k20); ir (aS, aT, k20); ir (uS, uT, k30);
        ir (cS, cT, k100); ir (satS, satT, k20);
        tonS += (tonT - tonS) * k30; if (std::abs (tonT - tonS) < 1e-6) tonS = tonT;
        ir (detMix, cT > 0 ? 1.0 : 0.0, k30);
        if (detMix <= 0.0) for (auto& z : voz) z.faseDet = 0.25;   // así, ao volver, as cabezas empezan onde le a dobre sen desafinar
    }

    void filtros()
    {
        hp.fixarFrec (fs, car.graves);
        lp.fixarFrec (fs, std::min (tonS, fs * 0.45));
        frecHp = car.graves; frecLp = tonS;
    }

    double fs = 48000;
    Axustes a;
    Caracter car = CARACTER[0], carT = CARACTER[0];
    Linea linL, linR;
    std::array<Voz, 2> voz;
    SvfCoef hp, lp;
    double T = 1000, D = 1000, Dn = 1000, xf = 0, salto = 96, fundido = 0.0005;
    double vT = 0, vS = 0, rateT = 0.5, rateS = 0.5, gT = 1, gS = 1, aT = 0, aS = 0, uT = 0, uS = 0, cT = 0, cS = 0;
    double tonT = 9000, tonS = 9000, satT = 0, satS = 0, detMix = 0;
    double k20 = 0.001, k30 = 0.001, k100 = 0.001, k150 = 0.001, kGlide = 0.001, frecHp = 0, frecLp = 0;
    bool primeira = true;
};

} // namespace adt
