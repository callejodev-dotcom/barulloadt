// Presets.h — Os presets de fábrica de Barullo ADT, por grupos (un por tipo). O primeiro é o de
// partida (os parámetros saen del).
#pragma once
#include "Dobre.h"

namespace adt {

struct Preset
{
    const char* nome;
    const char* grupo;
    const char* descricion;
    int tipo;
    double atraso, variacion, nivel, ancho, velocidade, desafinar, ton, saturacion;
    int dobres;
};

inline const Preset PRESETS[] = {
    //                                                   tipo        atraso var   nivel ancho vel   desaf ton    sat   dobres
    { "Abbey Road", "Abbey Road", "O ADT de 1966: unha segunda máquina a 24 ms co varispeed movéndose amodo. A dobre, no centro coa voz.",
      ABBEYROAD,  24.0, 0.40, -3.0, 0.00, 0.55, 0.0,  9000, 0.15, 1 },
    { "Abbey Road en estéreo", "Abbey Road", "Coma nas mesturas estéreo de entón: a voz cara a un lado e a dobre cara ao outro.",
      ABBEYROAD,  24.0, 0.40, -2.0, 0.80, 0.55, 0.0,  9000, 0.15, 1 },
    { "Dobre sutil", "Abbey Road", "Unha dobre baixiña e pouco movida: a voz engorda sen que se note o truco.",
      ABBEYROAD,  20.0, 0.25, -9.0, 0.20, 0.40, 0.0,  8000, 0.10, 1 },
    { "Cinta cansa", "Abbey Road", "A segunda máquina xa non vai fina: a dobre balancea máis e soa máis morna.",
      ABBEYROAD,  30.0, 0.70, -3.0, 0.30, 1.20, 0.0,  7000, 0.35, 1 },

    { "Segunda toma", "Dobre real", "Coma se cantases outra vez: ás veces antes, ás veces despois, e un chisco desafinada.",
      DOBREREAL,  28.0, 0.50, -4.0, 0.25, 0.35, 3.0, 10000, 0.05, 1 },
    { "Dobre solto", "Dobre real", "Unha segunda toma con menos coidado: o tempo baila máis.",
      DOBREREAL,  34.0, 0.70, -4.0, 0.50, 0.30, 5.0,  9000, 0.05, 1 },
    { "Dúas tomas", "Dobre real", "Dúas tomas máis, unha a cada lado da voz.",
      DOBREREAL,  26.0, 0.50, -5.0, 0.70, 0.35, 4.0, 10000, 0.05, 2 },
    { "Ombro con ombro", "Dobre real", "Dous cantantes moi xuntos, ao mesmo micro.",
      DOBREREAL,  18.0, 0.35, -6.0, 0.10, 0.25, 2.0, 11000, 0.00, 1 },

    { "Slap de cinta", "Slap", "A dobre cae tarde, coma o eco de cinta dos cincuenta.",
      SLAP,       95.0, 0.30, -6.0, 0.00, 0.60, 0.0,  6500, 0.35, 1 },
    { "Slap do 58", "Slap", "Máis tarde e máis escuro: rockabilly.",
      SLAP,      115.0, 0.25, -5.0, 0.00, 0.70, 0.0,  6000, 0.45, 1 },
    { "Slap aberto", "Slap", "Dous ecos de cinta, un a cada lado.",
      SLAP,       85.0, 0.30, -6.0, 0.70, 0.60, 0.0,  7000, 0.30, 2 },
    { "Eco de sala", "Slap", "Un rebote curto e escuro, coma a parede do estudio.",
      SLAP,       70.0, 0.20, -9.0, 0.40, 0.50, 0.0,  5000, 0.20, 1 },

    { "Tres voces", "Coro", "Dúas dobres, unha arriba e outra abaixo de ton, á esquerda e á dereita.",
      CORO,       22.0, 0.50, -5.0, 0.60, 0.70, 7.0,  9000, 0.10, 2 },
    { "Coro ancho", "Coro", "As dobres abertas de todo: a voz no medio dun coro.",
      CORO,       26.0, 0.60, -4.0, 1.00, 0.60, 10.0, 9000, 0.10, 2 },
    { "Coro escuro", "Coro", "Dúas dobres mornas por detrás da voz.",
      CORO,       30.0, 0.45, -6.0, 0.80, 0.45, 6.0,  5000, 0.25, 2 },
    { "Coro mono", "Coro", "Dúas dobres no centro: máis grosa, sen abrir.",
      CORO,       22.0, 0.50, -6.0, 0.00, 0.70, 7.0,  9000, 0.10, 2 },

    { "Mañá nunca se sabe", "Flanxe", "O ADT levado ao extremo: atraso curto que varre amodo, coma na voz de 1966 que soaba desde unha montaña.",
      FLANXE,      5.0, 0.60,  0.0, 0.00, 0.18, 0.0, 14000, 0.10, 1 },
    { "Flanxe de cinta", "Flanxe", "Dúas cintas que case van á par: o son baleira e enche.",
      FLANXE,      4.0, 0.70,  0.0, 0.00, 0.12, 0.0, 12000, 0.20, 1 },
    { "Avión", "Flanxe", "Moi curto e moi fondo: o son dun avión que pasa.",
      FLANXE,      3.0, 0.85,  0.0, 0.00, 0.08, 0.0, 16000, 0.05, 1 },
    { "Flanxe estéreo", "Flanxe", "Dúas dobres curtas que varren ao revés, unha a cada lado.",
      FLANXE,      6.0, 0.60, -1.0, 0.70, 0.20, 0.0, 14000, 0.10, 2 },
};

constexpr int NUM_PRESETS = (int) (sizeof (PRESETS) / sizeof (PRESETS[0]));

} // namespace adt
