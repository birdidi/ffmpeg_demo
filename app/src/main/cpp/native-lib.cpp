#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

extern "C" {
#include <libavcodec/version.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

#define LOG_TAG "daniel"
#define LOG(LEVEL, ...) __android_log_print(LEVEL, LOG_TAG, __VA_ARGS__)
#define LOGI(...) LOG(ANDROID_LOG_INFO, __VA_ARGS__)
#define LOGE(...) LOG(ANDROID_LOG_ERROR, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jstring JNICALL
Java_com_bir_ffmpeg_demo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_bir_ffmpeg_demo_MainActivity_getFFmpegVersion(JNIEnv *env, jobject thiz) {
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    return env->NewStringUTF(strBuffer);
}

void _render(JNIEnv *env, jobject thiz, AVCodecContext *avCodecContext) {
    const int videoWidth = avCodecContext->width;
    const int videoHeight = avCodecContext->height;
//    LOGI("video frame width %d, height: %d", videoWidth, videoHeight);
    AVFrame *yuvFrame = av_frame_alloc();
    AVFrame *frame = av_frame_alloc();
    uint8_t *frameBuffer;

    //计算buffer大小
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    // 为RGBA Frame分配空间
    frameBuffer = (uint8_t *) av_malloc(bufferSize);
    av_image_fill_arrays(yuvFrame->data, yuvFrame->linesize, frameBuffer, AV_PIX_FMT_RGBA,
                         videoWidth, videoHeight, 1);

    //获取转换的上下文
    SwsContext *swsContext = sws_getContext(videoWidth, videoHeight, avCodecContext->pix_fmt,
                                            videoWidth, videoHeight, AV_PIX_FMT_YUV420P,
                                            SWS_BICUBIC, NULL, NULL, NULL);

    //格式转换
    sws_scale(swsContext, yuvFrame->data, yuvFrame->linesize, 0, videoHeight, frame->data,
              frame->linesize);

    //释放资源
    if (yuvFrame != nullptr) {
        av_frame_free(&yuvFrame);
        yuvFrame = nullptr;
    }
    if (frameBuffer != nullptr) {
        free(frameBuffer);
        frameBuffer = nullptr;
    }
    if (swsContext != nullptr) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
}

jstring _decodec(JNIEnv *env, jobject thiz, jstring path, jobject surface) {
    std::string hello = "Hello from ffmpeg";
    hello += avcodec_configuration();

    AVFormatContext *avFormatContext;
    int tIndex = -1;
    AVCodecParameters *avCodecParameters;
    AVCodecContext *avCodecContext;
    const AVCodec *avCodec;
    AVPacket *avPacket;
    AVFrame *avFrame;

    avformat_network_init();
    // 1. 创建封装格式上下文
    avFormatContext = avformat_alloc_context();
    // 2. 打开输入文件，解封装
    const char *filePath = env->GetStringUTFChars(path, 0);
    env->ReleaseStringUTFChars(path, filePath);
    int demux = avformat_open_input(&avFormatContext, filePath, 0, 0);
    if (demux != 0) {
        LOGE("avformat_open_input() called failed: %s", av_err2str(demux));
    } else {
        LOGI("avformat_open_input() called success");
    }

    // 3. 获取音视频流信息
    int siCode = avformat_find_stream_info(avFormatContext, 0);
    if (siCode < 0) {
        LOGE("avformat_find_stream_info called failed %s", av_err2str(siCode));
    } else {
        LOGI("avformat_find_stream_info called success");
    }

    // 4. 获取音视频流索引
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        if (avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            tIndex = i;
            break;
        }
    }

    if (tIndex == -1) {
        LOGE("fail to find video stream index.");
    } else {
        LOGI("found out video stream index.");
    }
    //5.获取解码器参数
    avCodecParameters = avFormatContext->streams[tIndex]->codecpar;

    //6.根据codec_id获取解码器
    avCodec = avcodec_find_decoder(avCodecParameters->codec_id);
    if (avCodec == nullptr) {
        LOGE("avcodec_find_decoder called failed");
    } else {
        LOGI("avcodec_find_decoder called success.");
    }

    //7.创建解码器上下文
    avCodecContext = avcodec_alloc_context3(avCodec);
    int codecErrorCode = avcodec_parameters_to_context(avCodecContext, avCodecParameters);
    if (codecErrorCode != 0) {
        LOGE("avcodec_parameters_to_context called failed %s", av_err2str(codecErrorCode));
    } else {
        LOGI("avcodec_parameters_to_context called success.");
    }

    //8.打开解码器
    int openErrorCode = avcodec_open2(avCodecContext, avCodec, nullptr);
    if (openErrorCode != 0) {
        LOGE("avcodec_open2 called failed %s", av_err2str(openErrorCode));
    } else {
        LOGI("avcodec_open2 called success.");
    }

    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    ANativeWindow_Buffer windowBuffer;

    int dstWidth, dstHeight;
    const int videoWidth = avCodecContext->width;
    const int videoHeight = avCodecContext->height;
//    LOGI("video frame width %d, height: %d", videoWidth, videoHeight);
    int windowWidth = ANativeWindow_getWidth(nativeWindow);
    int windowHeight = ANativeWindow_getHeight(nativeWindow);

    if (windowWidth < windowHeight * videoWidth / videoHeight) {
        dstWidth = windowWidth;
        dstHeight = windowWidth * videoHeight / videoWidth;
    } else {
        dstWidth = windowHeight * videoWidth / videoHeight;
        dstHeight = windowHeight;
    }

    ANativeWindow_setBuffersGeometry(nativeWindow, dstWidth, dstHeight,
                                     WINDOW_FORMAT_RGBA_8888);

    openErrorCode = avcodec_open2(avCodecContext, avCodec, nullptr);
    if (openErrorCode != 0) {
        LOGE("avcodec_open2 called failed %s", av_err2str(openErrorCode));
    } else {
        LOGI("avcodec_open2 called success.");
    }

    AVFrame *pFrameRGBA = av_frame_alloc();
    AVFrame *pFrame = av_frame_alloc();
    uint8_t *frameBuffer;

    //计算buffer大小
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, dstWidth, dstHeight, 1);
    // 为RGBA Frame分配空间
    frameBuffer = (uint8_t *) av_malloc(bufferSize * sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data, pFrameRGBA->linesize, frameBuffer, AV_PIX_FMT_RGBA,
                         dstWidth, dstHeight, 1);

    //获取转换的上下文
    SwsContext *swsContext = sws_getContext(videoWidth,
                                            videoHeight,
                                            avCodecContext->pix_fmt,
                                            dstWidth,
                                            dstHeight,
                                            AV_PIX_FMT_RGBA,
                                            SWS_BILINEAR,
                                            NULL,
                                            NULL,
                                            NULL);

    //9.创建存储编码数据和解码数据的结构体
    avPacket = av_packet_alloc();
//    avFrame = av_frame_alloc();

    LOGI("pending to decode.");
    //10.解码循环
    while (av_read_frame(avFormatContext, avPacket) >= 0) {
        if (avPacket->stream_index == tIndex) {
            if (avcodec_send_packet(avCodecContext, avPacket) != 0) {
                LOGE("avcodec_send_packet called failed.");
                break;
            }
            while (avcodec_receive_frame(avCodecContext, pFrame) == 0) {
                //格式转换
                sws_scale(swsContext, pFrame->data, pFrame->linesize, 0, videoHeight,
                          pFrameRGBA->data, pFrameRGBA->linesize);

                ANativeWindow_lock(nativeWindow, &windowBuffer, 0);
                // 获取stride
                auto *dstBuffer = (uint8_t *) windowBuffer.bits;
                uint8_t *src = (pFrameRGBA->data[0]);

                int srcLineSize = dstWidth * 4;
                int dstLineSize = windowBuffer.stride * 4;

                //由于window的stride和帧的stride不同，因此需要进行逐行复制
                int h;
                for (h = 0; h < dstHeight; h++) {
                    memcpy(dstBuffer + h * dstLineSize, src + h * srcLineSize, srcLineSize);
                }
                ANativeWindow_unlockAndPost(nativeWindow);
            }
        }
        //释放packet引用，防止内存泄漏
        av_packet_unref(avPacket);
    }

    if (pFrameRGBA != nullptr) {
        av_frame_free(&pFrameRGBA);
        pFrameRGBA = nullptr;
    }
    if (frameBuffer != nullptr) {
        free(frameBuffer);
        frameBuffer = nullptr;
    }
    if (swsContext != nullptr) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }

    //11.释放资源，解码完成
    if (avFrame != nullptr) {
        av_frame_unref(avFrame);
        avFrame = nullptr;
    }

    if (avPacket != nullptr) {
        av_packet_unref(avPacket);
        avPacket = nullptr;
    }

    if (avCodecContext != nullptr) {
        avcodec_close(avCodecContext);
        avcodec_free_context(&avCodecContext);
        avCodecContext = nullptr;
        avCodec = nullptr;
    }

    if (avFormatContext != nullptr) {
        avformat_close_input(&avFormatContext);
        avformat_free_context(avFormatContext);
        avFormatContext = nullptr;
    }


    return env->NewStringUTF(hello.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_bir_ffmpeg_demo_MainActivity_play(JNIEnv *env, jobject thiz, jstring file,
                                           jobject surface) {
    return _decodec(env, thiz, file, surface);
}

#ifdef __cplusplus
}
#endif