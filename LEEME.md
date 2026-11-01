**Barullo ADT** é unha dobre artificial (ADT, *Artificial Double Tracking*): o truco que se inventou en Abbey Road en 1966 para que a voz soase dobrada sen cantala dúas veces. Unha segunda máquina de cinta reproduce a voz uns ms despois e coa velocidade movéndose un chisco, así que a copia chega sempre algo tarde e algo desafinada, e cada vez dun xeito, coma unha segunda toma. Para o día a día abondan un preset, o tipo e catro mandos (atraso, variación, nivel da dobre e ancho). Detrás de **MÁIS** están a velocidade, desafinar, o ton, a saturación, a mestura e o volume.

Unha ferramenta de [Escoita](https://www.escoita.link).

## Descargas

| Sistema | Instalador | Á man |
|---|---|---|
| **Mac** (Apple Silicon e Intel, macOS 11+): AU, VST3, CLAP e app | `BarulloADT-…-mac.pkg` | `BarulloADT-…-mac.zip` |
| **Windows** (64 bits): VST3, CLAP e app | `BarulloADT-…-windows-setup.exe` | `BarulloADT-…-windows-x64.zip` |
| **Linux** (x86_64): VST3, LV2, CLAP e app | — | `BarulloADT-…-linux-x86_64.tar.gz` → `./instalar.sh` |

Se o sistema avisa ao abrilo:
- **Mac:** clic dereito sobre o instalador → *Abrir* → *Abrir*.
- **Windows:** «Windows protexeu o teu PC» → *Máis información* → *Executar de todos os xeitos*.

Avisan porque os instaladores non levan sinatura de pago, non porque haxa nada raro.

## Que ten

- **20 presets de fábrica**, en grupos: *Abbey Road* (Abbey Road, Abbey Road en estéreo, Dobre sutil, Cinta cansa), *Dobre real* (Segunda toma, Dobre solto, Dúas tomas, Ombro con ombro), *Slap* (Slap de cinta, Slap do 58, Slap aberto, Eco de sala), *Coro* (Tres voces, Coro ancho, Coro escuro, Coro mono) e *Flanxe* (Mañá nunca se sabe, Flanxe de cinta, Avión, Flanxe estéreo). Son tamén os programas do DAW.
- **O tipo** (a tecla pon tamén o atraso, a variación, a velocidade, o desafinar e as dobres do tipo; o resto queda):
  - ABBEY ROAD: o ADT de 1966, unha segunda máquina a uns 24 ms co varispeed movéndose amodo (un seno lento);
  - DOBRE REAL: coma unha segunda toma, o atraso vai e vén sen regra (entre uns 15 e 40 ms) e o ton desvía uns cents;
  - SLAP: a dobre cae tarde (uns 100 ms), coma o eco de cinta dos cincuenta;
  - CORO: dúas dobres, unha un chisco arriba e outra abaixo de ton, que se moven ao revés;
  - FLANXE: o ADT levado ao extremo, un atraso moi curto (2-8 ms) que varre: o son baleira e enche.
- **Os catro mandos**: *Atraso* (1-150 ms), *Variación* (canto se move a dobre no tempo e no ton), *Nivel da dobre* (de calada a +6 dB) e *Ancho* (0 %, todo no centro coma en mono; cunha dobre, a voz a un lado e a dobre ao outro; con dúas, a voz no centro e unha dobre a cada lado).
- **DÚAS DOBRES**: unha segunda copia, un 12 % máis tarde e co movemento ao revés.
- **MÁIS**: *a segunda máquina* (velocidade do movemento, desafinar, ton e saturación) e *a saída* (mestura e volume).
- **A pantalla**: o atraso de cada dobre nos últimos catro segundos (vese como abanea), onde soa cada un no estéreo, o atraso, o movemento (± ms, Hz e cents) e os niveis de entrada, dobre e saída.
- **Presets propios, A/B e desfacer** (do núcleo común): os presets van en `.bdpreset`, en Documentos/Barullo ADT/Presets. Cada clic é un paso de desfacer.
- **Sen latencia**: o son seco sae tal cal, sen atraso ningún (co ancho a 0, mostra a mostra).
- **Bypass** de verdade: o son pasa tal cal e a dobre segue a soar por dentro.
- Un **limitador de seguridade** á saída: nunca pasa de −1 dBFS (sen anticipación, para non engadir latencia).
- Entrada mono ou estéreo.

## Como se fixo

`Source/Dobre.h`, sen JUCE:

```
entrada ─┬──────────────────────────────── (tal cal, sen latencia) ── colocación ─┐
         └─ liña (L e R) ─┬─ ler (atraso + movemento) ─ cinta ─ graves ─ ton ─ nivel ─┴─ saída
                          └─ (a segunda dobre: o seu movemento, ao revés)
```

- **A lectura entre mostras**: un sinc con xanela de Kaiser (16 puntos, 512 fases interpoladas). En calquera posición entre mostras, o nivel varía menos de 0,002 dB ata 16 kHz e o erro a 5 kHz queda en −81 dB: mover o atraso non fai «zipper» nin apaga agudos. O seco non pasa por ela.
- **O movemento**: un seno lento (o varispeed da segunda máquina, coa velocidade do mando) e unha deriva ao chou alisada (en coseno entre puntos ao chou, así o ton non salta nunca), máis un flutter moi pequeno (≈ 6 Hz, 0,02 ms). Canto de cada un depende do tipo: Abbey Road é case todo seno; Dobre real, case todo deriva. O movemento é unha fracción do atraso (a variación) e, como moito, o que move o ton un 6 % (un semitono).
- **Desafinar**: dúas cabezas que len cun atraso que mingua (sobe o ton) ou medra (baixa) sen parar, coma un harmonizador de cinta; cada unha le media volta e fúndense nun 10 % da volta (sen² e cos², suman 1). Sen desafinar, unha soa lectura.
- **A cinta**: un chisco de saturación asimétrica (tanh cun desprazamento, coa pendente 1 no cero: en baixo nivel, nada; dá o segundo harmónico), un paso alto (40-130 Hz segundo o tipo) e un paso baixo (o ton). Os filtros son SVF en forma TPT: o ton pódese mover mostra a mostra sen transitorios.
- **Sen clics**: todo vai suavizado mostra a mostra (o carácter do tipo, a variación, a velocidade, o nivel, o ancho, as dobres, o desafinar e o ton). Os saltos grandes do atraso (máis de 2 ms dunha vez, coma ao cambiar de tipo) fúndense en 40 ms entre a lectura vella e a nova; os pequenos (mover o mando) esvaran.
- **A colocación** é un balance: no centro, as dúas canles co 1 xusto (o seco sae bit a bit); a un lado, baixa a outra canle.

## Estrutura

| Ficheiro | Que é |
|---|---|
| `Source/Dobre.h` | A dobre |
| `Source/Presets.h` | Os presets de fábrica |
| `Source/PluginProcessor.*` | O plugin: parámetros, presets, estado e a mestura |
| `Source/PluginEditor.*` | A interface |
| `tools/RenderTest.cpp` | A proba |

Usa o núcleo común de Escoita ([escoita-nucleo](https://github.com/callejodev-dotcom/escoita-nucleo), v0.5.0): o texto en galego, o aviso das versións novas, os presets propios, o A/B e o desfacer (`escoita::Estado`), o limitador de seguridade (`escoita::Teito`, sen anticipación), a CI e os instaladores.

## Probas

- **Ao abrir**: o primeiro preset, cos parámetros nos seus valores de partida; latencia 0.
- **Texto dos parámetros**: texto → valor → texto dá o mesmo en todos (coma o clap-validator); as opcións, tamén polo comezo.
- **Lectura entre mostras**: plana con calquera decimal (5, 10 e 16 kHz) e limpa (o erro).
- **Atraso**: medido (correlación cun golpe sen media) é o do mando: 1,5, 12,34, 24, 95 e 150 ms, a 48, 44,1 e 96 kHz.
- **Movemento**: cun seno de 1 kHz, a fase da dobre dá o atraso no tempo; no flanxe, a profundidade, a velocidade e os cents son os dos mandos; en Abbey Road e Dobre real, o movemento queda dentro do que din os mandos e o ton móvese uns cents.
- **Desafinar**: dúas dobres abertas, +10 e −10 cents medidos.
- **Mestura a 0, nivel calado (cunha e con dúas dobres) e bypass**: o son sae tal cal, bit a bit.
- **Sen clics**: os 20 cambios de tipo (coa tecla) e 20 s de automatización a saltos de todos os mandos, cun seno de 220 Hz: o que queda por riba de 5 kHz, por baixo de −70 dB.
- **Mono**: no centro, as dúas canles iguais bit a bit; abertas, a voz tal cal pola esquerda e a dobre (24 ms) pola dereita; con dúas dobres, unha a cada lado (24 e 26,88 ms).
- **Bloques**: con bloques de 512 e de 37 mostras sae o mesmo, mostra a mostra (e a súa pegada).
- **Os 20 presets** soan, sen valores non numéricos e sen tocar o teito.
- **Estado**, **A/B**, **presets propios**, **desfacer** (preset, tecla do tipo e mandos), **programas do DAW e cola**, **limitador** e **consumo** por instancia.
- **Validación**: pluginval ao nivel máximo (10), auval no Mac e clap-validator.

Na CI van en Mac, Windows e Linux, e tamén con AddressSanitizer e UBSan (`tools/probas-ci.sh`).

## Compilar

Nun Mac: `./build_mac.sh` (compila e instala no teu usuario). Á man:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBARULLO_TESTS=ON
cmake --build build --config Release
build/BDRenderTest_artefacts/Release/BDRenderTest
```

Outras ordes da proba: `--snapshot saida.png [mais] [preset]` (a interface), `--render saida.wav [preset]` (un preset cun son de proba) e `--demo preset entrada.wav saida.wav` (un preset sobre un son teu; un son mono sae en estéreo).

## Publicar unha versión

```
git tag v0.1.0 && git push origin v0.1.0
```

A versión sae da liña `project(BarulloADT VERSION …)` de `CMakeLists.txt`; o texto da versión, de `packaging/NOTAS-VERSION.md`.

## Créditos e licenzas

- **Barullo ADT é software libre**, baixo a licenza **GNU Affero General Public License v3.0** (ficheiro `LICENSE`): podes usalo, estudalo, modificalo e compartilo; se distribúes unha versión modificada, ten que ir co seu código e coa mesma licenza. Usa JUCE 8 coa súa licenza AGPLv3.
- Tipografías Montserrat e Inter (licenza OFL, `Resources/Montserrat-OFL.txt` e `Resources/Inter-OFL.txt`).
