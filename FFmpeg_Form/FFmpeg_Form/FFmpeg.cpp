#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

int vst_idx = -1, ast_idx = -1;

bool init_ffmpeg(AVFormatContext* fmt_ctx, const AVCodec* codec)
{

    return true;
}
int main() {
    AVFormatContext* fmt_ctx = NULL;
    AVCodecContext* codec_ctx = NULL;
    const AVCodec* codec = NULL;

    avformat_network_init();

    av_log_set_level(AV_LOG_DEBUG);
    const char* url = "rtsp://211.189.132.125:554/AIXTEAM03?mt=2&res=004";
    //const char* url = "C:/Users/kimss/Desktop/tank01.mp4";

    av_log(NULL, AV_LOG_INFO, "AVFormat major version = %d\n", LIBAVFORMAT_VERSION_MAJOR);

    if (avformat_open_input(&fmt_ctx, url, NULL, NULL) != 0) {
        std::cerr << "url을 열 수 없습니다" << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        std::cerr << "스트림 정보를 찾을 수 없습니다." << std::endl;
        return -1;
    }

    av_dump_format(fmt_ctx, 0, url, 0);

    av_log(NULL, AV_LOG_INFO, "nb_stream: %d\n", fmt_ctx->nb_streams);

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vst_idx = i;        
            std::cout << "비디오 코덱: " << fmt_ctx->streams[i]->codecpar->codec_type << std::endl;
            //break;
        }
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            ast_idx = i;
            std::cout << "오디오 코덱: " << fmt_ctx->streams[i]->codecpar->codec_type << std::endl;
        }
    }

    vst_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    std::cout << "최적 스트림: " << vst_idx << std::endl;

    std::cout << "시간 = " << fmt_ctx->duration / AV_TIME_BASE << std::endl;
    std::cout << "비트레이트 = " << fmt_ctx->bit_rate << std::endl;
    std::cout << "색상 포맷: " << fmt_ctx->streams[vst_idx]->codecpar->format << std::endl;
    std::cout << "코덱: " << fmt_ctx->streams[vst_idx]->codecpar->codec_id << std::endl;

    if (vst_idx == -1) {
        std::cerr << "비디오 스트림을 찾을 수 없습니다." << std::endl;
        avformat_close_input(&fmt_ctx);
    }
    vst_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    AVCodecParameters* codec_par = fmt_ctx->streams[vst_idx]->codecpar;
    codec = avcodec_find_decoder(codec_par->codec_id);
    if (!codec) {
        std::cerr << "해당 코덱을 찾을 수 없습니다." << std::endl;
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codec_ctx, codec_par) < 0) {
        std::cerr << "코덱 컨텍스트 초기화 실패." << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        std::cerr << "코덱 열기 실패" << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == vst_idx) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {

                    std::cout << "디코딩된 프레임: " << frame->pts << std::endl;
                    std::cout << "데이터: " << frame->buf << std::endl;
                }
            }
        }
        av_packet_unref(pkt);
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}

//extern "C" {
//#include <libavcodec/avcodec.h>
//#include <libavformat/avformat.h>
//#include <libswscale/swscale.h>
//#include <libavutil/imgutils.h>
//}
//#include <iostream>
//
//void ConvertToRGB(AVFrame* frame, AVCodecContext* codec_ctx, uint8_t** rgb_buffer, AVFrame** rgb_frame) {
//    // SwsContext 생성 (픽셀 형식 변환 설정)
//    SwsContext* sws_ctx = sws_getContext(
//        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, // 입력: 너비, 높이, 픽셀 포맷
//        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,  // 출력: 너비, 높이, RGB24 포맷
//        SWS_BILINEAR,                                          // 변환 알고리즘
//        nullptr, nullptr, nullptr);
//
//    if (!sws_ctx) {
//        std::cerr << "SwsContext 초기화 실패" << std::endl;
//        return;
//    }
//
//    // RGB 버퍼 크기 계산
//    int rgb_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
//
//    // RGB 버퍼 할당
//    *rgb_buffer = (uint8_t*)av_malloc(rgb_buffer_size);
//    if (!(*rgb_buffer)) {
//        std::cerr << "RGB 버퍼 메모리 할당 실패" << std::endl;
//        sws_freeContext(sws_ctx);
//        return;
//    }
//
//    // RGB 프레임 생성
//    *rgb_frame = av_frame_alloc();
//    if (!(*rgb_frame)) {
//        std::cerr << "RGB 프레임 메모리 할당 실패" << std::endl;
//        av_free(*rgb_buffer);
//        sws_freeContext(sws_ctx);
//        return;
//    }
//
//    // RGB 프레임의 데이터 연결
//    av_image_fill_arrays((*rgb_frame)->data, (*rgb_frame)->linesize, *rgb_buffer, AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
//
//    // YUV 프레임을 RGB로 변환
//    sws_scale(
//        sws_ctx,                // SwsContext
//        frame->data,            // 입력 데이터 (YUV)
//        frame->linesize,        // 입력 라인 사이즈
//        0,                      // 변환 시작 줄
//        codec_ctx->height,      // 변환할 라인의 높이
//        (*rgb_frame)->data,     // 출력 데이터 (RGB)
//        (*rgb_frame)->linesize  // 출력 라인 사이즈
//    );
//
//    // SwsContext 해제
//    sws_freeContext(sws_ctx);
//}
//
//int main() {
//    AVFormatContext* fmt_ctx = nullptr;
//    AVCodecContext* codec_ctx = nullptr;
//    AVFrame* frame = nullptr;
//    AVFrame* rgb_frame = nullptr;
//    uint8_t* rgb_buffer = nullptr;
//    const char* url = "rtsp://211.189.132.124:554/AIXTEAM03?mt=2&res=004";
//
//    // FFmpeg 초기화 및 파일 열기
//    avformat_open_input(&fmt_ctx, url, nullptr, nullptr);
//    avformat_find_stream_info(fmt_ctx, nullptr);
//    av_dump_format(fmt_ctx, 0, url, 0);
//    // 비디오 스트림 찾기
//    int video_stream_index = -1;
//    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
//        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
//            video_stream_index = i;
//            break;
//        }
//    }
//    const AVCodec* codec = avcodec_find_decoder(fmt_ctx->streams[video_stream_index]->codecpar->codec_id);
//    codec_ctx = avcodec_alloc_context3(codec);
//    avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);
//    avcodec_open2(codec_ctx, codec, nullptr);
//
//    // 디코딩 루프
//    AVPacket* pkt = av_packet_alloc();
//    frame = av_frame_alloc();
//
//    while (av_read_frame(fmt_ctx, pkt) >= 0) {
//        if (pkt->stream_index == video_stream_index) {
//            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
//                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
//                    // 디코딩된 프레임을 RGB로 변환
//                    ConvertToRGB(frame, codec_ctx, &rgb_buffer, &rgb_frame);
//
//                    // RGB 프레임을 처리 (여기에서 화면 출력 가능)
//                    std::cout << "RGB 데이터 변환 완료, 첫 번째 픽셀 값: "
//                        << (int)rgb_frame->data[0][0] << std::endl;
//                }
//            }
//        }
//        av_packet_unref(pkt);
//    }
//
//    // 리소스 정리
//    av_frame_free(&frame);
//    av_frame_free(&rgb_frame);
//    av_packet_free(&pkt);
//    avcodec_free_context(&codec_ctx);
//    avformat_close_input(&fmt_ctx);
//    av_free(rgb_buffer);
//
//    return 0;
//}
