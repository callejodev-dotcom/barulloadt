#!/bin/bash
# As probas de Barullo ADT na CI común (escoita-nucleo): chámase desde a raíz con
# mac, windows, linux ou sanitizadores, coa compilación en build/
set -eo pipefail
SO="$1"
EXE=""; [ "$SO" = windows ] && EXE=".exe"
CONF=Release; [ "$SO" = sanitizadores ] && CONF=RelWithDebInfo
RT="build/BDRenderTest_artefacts/$CONF/BDRenderTest$EXE"

echo "::group::Son (o atraso, o movemento, desafinar, a colocación, sen clics, os presets e o estado)"
"$RT" proba.wav
echo "::endgroup::"
