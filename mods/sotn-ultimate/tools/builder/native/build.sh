#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# The builder's audio module: FFmpeg 7.1.1 cut down to what the mod decodes
# (ATRAC3/ATRAC3plus and PCM WAV; the filters psp_dub.py / gen_saturn_music.py
# use) + at3dec.c + mods/sotn-ultimate/tools/satxa.c (its main: callMain) + dsp.c (the
# maths functions src/dsp.ts takes from here, not from JavaScript), as one
# self-contained JS file (SINGLE_FILE: the wasm inside; works from file://),
# for Node (the release build) and the builder page alike:
#   $MOD_WORK/builder/audio/audio_core.js (MOD_WORK: mods/sotn-ultimate/tools/mod_work.py)
# Needs emsdk 4.0.10 ($EMSDK, default $MOD_WORK/builder/emsdk) and the FFmpeg
# 7.1.1 source ($FFMPEG_SRC, default $MOD_WORK/builder/ffmpeg).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
MOD_WORK=${MOD_WORK:-$HERE/../../../../../build/mod-work}
B=$MOD_WORK/builder
EMSDK=${EMSDK:-$B/emsdk}
FFMPEG_SRC=${FFMPEG_SRC:-$B/ffmpeg}
OUT=$B/audio
SATXA=$HERE/../../satxa.c
source $EMSDK/emsdk_env.sh >/dev/null 2>&1
mkdir -p $OUT

if [ ! -f $OUT/ffmpeg/lib/libavcodec.a ]; then
    rm -rf $OUT/ffmpeg-src && cp -r $FFMPEG_SRC $OUT/ffmpeg-src && cd $OUT/ffmpeg-src
    emconfigure ./configure --prefix=$OUT/ffmpeg \
        --disable-everything --disable-programs --disable-doc --disable-network \
        --disable-autodetect --disable-asm --disable-pthreads --disable-w32threads \
        --disable-os2threads --disable-debug --disable-avdevice --disable-swscale \
        --disable-postproc \
        --enable-decoder=atrac3p,atrac3,pcm_s16le --enable-demuxer=wav \
        --enable-protocol=file \
        --enable-filter=abuffer,abuffersink,highpass,volume,aresample,aformat,anull \
        --enable-swresample \
        --target-os=none --arch=x86_32 --enable-cross-compile --cc=emcc --cxx=em++ \
        --ar=emar --ranlib=emranlib --nm="$(dirname $(which emcc))/../bin/llvm-nm" \
        --extra-cflags="-O2" > $OUT/ffmpeg-configure.log
    emmake make -j"$(nproc)" install > $OUT/ffmpeg-make.log 2>&1
fi

cd $OUT
LIBS="-lavfilter -lavformat -lavcodec -lswresample -lavutil -lm"
emcc -O2 -I ffmpeg/include $HERE/at3dec.c -c -o at3dec.o
emcc -O2 $SATXA -c -o satxa.o
emcc -O2 $HERE/dsp.c -c -o dsp.o
emcc -O2 at3dec.o satxa.o dsp.o -L ffmpeg/lib $LIBS -o audio_core.js \
    -sMODULARIZE=1 -sEXPORT_NAME=AudioCore -sSINGLE_FILE=1 \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sENVIRONMENT=web,node \
    -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 \
    -sEXPORTED_FUNCTIONS=_main,_at3_decode,_dsp_sin,_dsp_cos,_dsp_pow,_dsp_log2,_dsp_log10,_dsp_exp \
    -sEXPORTED_RUNTIME_METHODS=callMain,ccall,FS
ls -la audio_core.js
