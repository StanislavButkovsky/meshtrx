cmake_minimum_required(VERSION 3.22)
project(codec2jni)

set(CODEC2_SOURCES
    codec2/codec2.c
    codec2/kiss_fft.c
    codec2/kiss_fftr.c
    codec2/nlp.c
    codec2/lpc.c
    codec2/postfilter.c
    codec2/quantise.c
    codec2/pack.c
    codec2/sine.c
    codec2/phase.c
    codec2/interp.c
    codec2/lsp.c
    codec2/mbest.c
    codec2/codec2_fft.c
    codec2/machdep.c
    codec2/newamp1.c
    codec2/codebookjmv.c
    codec2/codebookge.c
    codec2/codebooknewamp1.c
    codec2/codebooknewamp1_energy.c
    codec2/codebooknewamp2.c
    codec2/codebooknewamp2_energy.c
    codec2/codebook.c
    codec2/codebookd.c
)

add_library(codec2jni SHARED codec2_jni.cpp ${CODEC2_SOURCES})
target_include_directories(codec2jni PRIVATE codec2/)
target_compile_options(codec2jni PRIVATE -w -DCODEC2_MODE_EN_DEFAULT=0 -DCODEC2_MODE_1200_EN=1 -DCODEC2_MODE_3200_EN=1)
target_link_libraries(codec2jni android log m)
