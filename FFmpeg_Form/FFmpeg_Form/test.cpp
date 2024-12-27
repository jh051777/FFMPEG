#include <iostream>
#include <Windows.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
}

int main() {
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* codec_ctx = nullptr;
	const AVCodec* codec = nullptr;
	AVStream* stream = nullptr;
	AVPacket* pkt = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();

	const char* url = "rtsp://211.189.132.124:554/AIXTEAM03?mt=2&res=004";

	avformat_network_init();

	av_log_set_level(AV_LOG_DEBUG);

	if (avformat_open_input(&fmt_ctx, url, NULL, NULL) != 0) {
		std::cerr << "Failed open url" << std::endl;
		return -1;
	}
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		std::cerr << "스트림 찾기 실패" << std::endl;
		return -1;
	}

	av_dump_format(fmt_ctx, 0, url, 0);

	int vst_idx = -1, ast_idx = -1;

	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			vst_idx = i;
			break;
		}
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			ast_idx = i;
			break;
		}
	}
	
	vst_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	ast_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, vst_idx, NULL, 0);

	AVCodecParameters* codec_par= nullptr;
	
	codec_par = fmt_ctx->streams[vst_idx]->codecpar;
	codec = avcodec_find_decoder(codec_par->codec_id);
	
	if (!codec) {
		std::cerr << "코덱 찾기 실패" << std::endl;
		avformat_close_input(&fmt_ctx);
		return -1;
	}

	codec_ctx = avcodec_alloc_context3(codec);

	if (avcodec_parameters_to_context(codec_ctx, codec_par) < 0) {
		std::cerr << "코덱 컨텍스트 초기화 실패" << std::endl;
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
	int count = 0;
	while (av_read_frame(fmt_ctx, pkt) >= 0) {
		if (pkt->stream_index == vst_idx) {
			if (avcodec_send_packet(codec_ctx, pkt) == 0) {
				while (avcodec_receive_frame(codec_ctx, frame) == 0) {
					std::cout << count <<" - pts: " << frame->pts << std::endl;
					std::cout << count << " - size: " << pkt->size << std::endl;
				}
			}
		}
		av_packet_unref(pkt);
		av_frame_unref(frame);
		std::cout << count << std::endl;
		count++;
	}

	av_frame_free(&frame);
	av_packet_free(&pkt);
	avcodec_free_context(&codec_ctx);
	avformat_close_input(&fmt_ctx);

	return 0;
}