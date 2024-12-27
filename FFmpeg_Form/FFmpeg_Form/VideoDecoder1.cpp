#include "VideoDecoder1.h"
#include <stdexcept>

VideoDecoder::VideoDecoder(const std::string& filename) {
	if (avformat_open_input(&fmt_ctx, filename.c_str(), nullptr, nullptr) < 0) {
		throw std::runtime_error("파일을 열수 없습니다.");
	}

	if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
		throw std::runtime_error("스트림 정보를 찾을 수 없습니다.");
	}

	int video_stream_index = -1;
	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_index = i;
			break;
		}
	}

	if (video_stream_index == -1) {
		throw std::runtime_error("비디오 스트림을 찾을 수 없습니다.");
	}

	video_stream = fmt_ctx->streams[video_stream_index];
	codec_ctx = avcodec_alloc_context3(avcodec_find_decoder(video_stream->codecpar->codec_id));
	avcodec_parameters_to_context(codec_ctx, video_stream->codecpar);

	if (avcodec_open2(codec_ctx, avcodec_find_decoder(video_stream->codecpar->codec_id), nullptr) < 0) {
		throw std::runtime_error("코덱을 열 수 없습니다.");
	}

	frame = av_frame_alloc();
	pkt = av_packet_alloc();
	sws_ctx = sws_getContext(
		codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
		codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
		SWS_BILINEAR, nullptr, nullptr, nullptr);
}

VideoDecoder::~VideoDecoder() {
	av_frame_free(&frame);
	av_packet_free(&pkt);
	avcodec_free_context(&codec_ctx);
	avformat_close_input(&fmt_ctx);
	sws_freeContext(sws_ctx);
}

int VideoDecoder::GetWidth() const {
	return codec_ctx->width;
}

int VideoDecoder::GetHeight() const {
	return codec_ctx->height;
}

AVFrame* VideoDecoder::DecoderNextFrame() {
	while (av_read_frame(fmt_ctx, pkt) >= 0) {
		if (pkt->stream_index == video_stream->index) {
			if (avcodec_send_packet(codec_ctx, pkt) == 0) {
				if (avcodec_receive_frame(codec_ctx, frame) == 0) {
					return frame;
				}
			}
		}
		av_packet_unref(pkt);
	}
	return nullptr;
}