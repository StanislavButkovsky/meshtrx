#include <jni.h>
#include <android/log.h>

extern "C" {
#include "codec2/codec2.h"
}

#define TAG "Codec2JNI"

extern "C" JNIEXPORT jlong JNICALL
Java_com_meshtrx_app_Codec2Wrapper_nativeInit(JNIEnv* env, jobject, jint mode) {
    int c2mode;
    switch (mode) {
        case 0: c2mode = CODEC2_MODE_1200; break;
        case 1: c2mode = CODEC2_MODE_3200; break;
        default: c2mode = CODEC2_MODE_3200; break;
    }
    struct CODEC2* state = codec2_create(c2mode);
    if (!state) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "codec2_create failed");
        return 0;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "codec2_create OK, mode=%d", c2mode);
    return (jlong)(intptr_t)state;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_meshtrx_app_Codec2Wrapper_nativeEncode(JNIEnv* env, jobject, jlong handle, jshortArray pcm) {
    struct CODEC2* state = (struct CODEC2*)(intptr_t)handle;
    if (!state) return nullptr;

    int nsam = codec2_samples_per_frame(state);
    int nbytes = codec2_bytes_per_frame(state);

    jshort* pcmBuf = env->GetShortArrayElements(pcm, nullptr);
    jbyteArray result = env->NewByteArray(nbytes);
    jbyte* outBuf = env->GetByteArrayElements(result, nullptr);

    codec2_encode(state, (unsigned char*)outBuf, pcmBuf);

    env->ReleaseShortArrayElements(pcm, pcmBuf, JNI_ABORT);
    env->ReleaseByteArrayElements(result, outBuf, 0);
    return result;
}

extern "C" JNIEXPORT jshortArray JNICALL
Java_com_meshtrx_app_Codec2Wrapper_nativeDecode(JNIEnv* env, jobject, jlong handle, jbyteArray encoded) {
    struct CODEC2* state = (struct CODEC2*)(intptr_t)handle;
    if (!state) return nullptr;

    int nsam = codec2_samples_per_frame(state);

    jbyte* inBuf = env->GetByteArrayElements(encoded, nullptr);
    jshortArray result = env->NewShortArray(nsam);
    jshort* outBuf = env->GetShortArrayElements(result, nullptr);

    codec2_decode(state, outBuf, (const unsigned char*)inBuf);

    env->ReleaseByteArrayElements(encoded, inBuf, JNI_ABORT);
    env->ReleaseShortArrayElements(result, outBuf, 0);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_meshtrx_app_Codec2Wrapper_nativeFree(JNIEnv* env, jobject, jlong handle) {
    struct CODEC2* state = (struct CODEC2*)(intptr_t)handle;
    if (state) {
        codec2_destroy(state);
        __android_log_print(ANDROID_LOG_INFO, TAG, "codec2_destroy OK");
    }
}
