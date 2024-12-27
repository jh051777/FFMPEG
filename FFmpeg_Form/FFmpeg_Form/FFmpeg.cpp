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
        std::cerr << "url�� �� �� �����ϴ�" << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        std::cerr << "��Ʈ�� ������ ã�� �� �����ϴ�." << std::endl;
        return -1;
    }

    av_dump_format(fmt_ctx, 0, url, 0);

    av_log(NULL, AV_LOG_INFO, "nb_stream: %d\n", fmt_ctx->nb_streams);

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vst_idx = i;        
            std::cout << "���� �ڵ�: " << fmt_ctx->streams[i]->codecpar->codec_type << std::endl;
            //break;
        }
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            ast_idx = i;
            std::cout << "����� �ڵ�: " << fmt_ctx->streams[i]->codecpar->codec_type << std::endl;
        }
    }

    vst_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    std::cout << "���� ��Ʈ��: " << vst_idx << std::endl;

    std::cout << "�ð� = " << fmt_ctx->duration / AV_TIME_BASE << std::endl;
    std::cout << "��Ʈ����Ʈ = " << fmt_ctx->bit_rate << std::endl;
    std::cout << "���� ����: " << fmt_ctx->streams[vst_idx]->codecpar->format << std::endl;
    std::cout << "�ڵ�: " << fmt_ctx->streams[vst_idx]->codecpar->codec_id << std::endl;

    if (vst_idx == -1) {
        std::cerr << "���� ��Ʈ���� ã�� �� �����ϴ�." << std::endl;
        avformat_close_input(&fmt_ctx);
    }
    vst_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    AVCodecParameters* codec_par = fmt_ctx->streams[vst_idx]->codecpar;
    codec = avcodec_find_decoder(codec_par->codec_id);
    if (!codec) {
        std::cerr << "�ش� �ڵ��� ã�� �� �����ϴ�." << std::endl;
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codec_ctx, codec_par) < 0) {
        std::cerr << "�ڵ� ���ؽ�Ʈ �ʱ�ȭ ����." << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        std::cerr << "�ڵ� ���� ����" << std::endl;
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

                    std::cout << "���ڵ��� ������: " << frame->pts << std::endl;
                    std::cout << "������: " << frame->buf << std::endl;
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
//    // SwsContext ���� (�ȼ� ���� ��ȯ ����)
//    SwsContext* sws_ctx = sws_getContext(
//        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, // �Է�: �ʺ�, ����, �ȼ� ����
//        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,  // ���: �ʺ�, ����, RGB24 ����
//        SWS_BILINEAR,                                          // ��ȯ �˰���
//        nullptr, nullptr, nullptr);
//
//    if (!sws_ctx) {
//        std::cerr << "SwsContext �ʱ�ȭ ����" << std::endl;
//        return;
//    }
//
//    // RGB ���� ũ�� ���
//    int rgb_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
//
//    // RGB ���� �Ҵ�
//    *rgb_buffer = (uint8_t*)av_malloc(rgb_buffer_size);
//    if (!(*rgb_buffer)) {
//        std::cerr << "RGB ���� �޸� �Ҵ� ����" << std::endl;
//        sws_freeContext(sws_ctx);
//        return;
//    }
//
//    // RGB ������ ����
//    *rgb_frame = av_frame_alloc();
//    if (!(*rgb_frame)) {
//        std::cerr << "RGB ������ �޸� �Ҵ� ����" << std::endl;
//        av_free(*rgb_buffer);
//        sws_freeContext(sws_ctx);
//        return;
//    }
//
//    // RGB �������� ������ ����
//    av_image_fill_arrays((*rgb_frame)->data, (*rgb_frame)->linesize, *rgb_buffer, AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
//
//    // YUV �������� RGB�� ��ȯ
//    sws_scale(
//        sws_ctx,                // SwsContext
//        frame->data,            // �Է� ������ (YUV)
//        frame->linesize,        // �Է� ���� ������
//        0,                      // ��ȯ ���� ��
//        codec_ctx->height,      // ��ȯ�� ������ ����
//        (*rgb_frame)->data,     // ��� ������ (RGB)
//        (*rgb_frame)->linesize  // ��� ���� ������
//    );
//
//    // SwsContext ����
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
//    // FFmpeg �ʱ�ȭ �� ���� ����
//    avformat_open_input(&fmt_ctx, url, nullptr, nullptr);
//    avformat_find_stream_info(fmt_ctx, nullptr);
//    av_dump_format(fmt_ctx, 0, url, 0);
//    // ���� ��Ʈ�� ã��
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
//    // ���ڵ� ����
//    AVPacket* pkt = av_packet_alloc();
//    frame = av_frame_alloc();
//
//    while (av_read_frame(fmt_ctx, pkt) >= 0) {
//        if (pkt->stream_index == video_stream_index) {
//            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
//                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
//                    // ���ڵ��� �������� RGB�� ��ȯ
//                    ConvertToRGB(frame, codec_ctx, &rgb_buffer, &rgb_frame);
//
//                    // RGB �������� ó�� (���⿡�� ȭ�� ��� ����)
//                    std::cout << "RGB ������ ��ȯ �Ϸ�, ù ��° �ȼ� ��: "
//                        << (int)rgb_frame->data[0][0] << std::endl;
//                }
//            }
//        }
//        av_packet_unref(pkt);
//    }
//
//    // ���ҽ� ����
//    av_frame_free(&frame);
//    av_frame_free(&rgb_frame);
//    av_packet_free(&pkt);
//    avcodec_free_context(&codec_ctx);
//    avformat_close_input(&fmt_ctx);
//    av_free(rgb_buffer);
//
//    return 0;
//}
