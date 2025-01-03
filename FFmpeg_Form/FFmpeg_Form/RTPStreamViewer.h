#pragma once
#include "OpenFile.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace FFmpegForm {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace System::Threading;

	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* vcodec_ctx = nullptr;
	AVCodecContext* acodec_ctx = nullptr;
	const AVCodec* vcodec = nullptr;
	const AVCodec* acodec = nullptr;
	AVPacket* pkt = nullptr;
	AVFrame* vframe = av_frame_alloc();
	AVFrame* rgb_frame = av_frame_alloc();
	SwsContext* sws_ctx = nullptr;
	std::queue<AVFrame*> frame_q;
	std::mutex q_mutex;
	volatile bool isStreaming;
	char* url;

	int frame_count;


	/// <summary>
	/// StreamViewer에 대한 요약입니다.
	/// </summary>
	public ref class StreamViewer : public System::Windows::Forms::Form
	{
		DateTime last_fps_time;
	private: System::Windows::Forms::TableLayoutPanel^ tlp_media;

		   int vst_idx = -1;
		   /*ref class FFmpeg {
		   public:
			   AVFormatContext* format_ctx = nullptr;
			   AVCodecContext* video_codec_ctx = nullptr;
			   AVCodecContext* audio_codec_ctx = nullptr;
			   const AVCodec* video_codec = nullptr;
			   const AVCodec* audio_codec = nullptr;
			   char* url;
			   int vst_idx = -1, ast_idx = -1;

			   void Format() {
				   AVFormatContext* fmt_ctx = nullptr;

				   format_ctx = fmt_ctx;

				   if (avformat_open_input(&fmt_ctx, url, NULL, NULL) != 0) {
					   MessageBox::Show("url을 열 수 없습니다.");
					   return;
				   }
				   if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
					   MessageBox::Show("스트림 정보를 찾을 수 없습니다.");
					   avformat_close_input(&fmt_ctx);
					   return;
				   }

				   vst_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
				   ast_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
			   }

			   void Codec() {
				   const AVCodec* vCodec = nullptr;
				   const AVCodec* aCodec = nullptr;
				   AVCodecContext* vCodec_ctx = nullptr;
				   AVCodecContext* aCodec_ctx = nullptr;

				   if (vst_idx >= 0) {
					   vCodec = avcodec_find_decoder(format_ctx->streams[vst_idx]->codecpar->codec_id);
					   video_codec = vCodec;

					   vCodec_ctx = avcodec_alloc_context3(vCodec);
					   avcodec_parameters_to_context(vCodec_ctx, format_ctx->streams[vst_idx]->codecpar);
					   video_codec_ctx = vCodec_ctx;

					   avcodec_open2(video_codec_ctx, video_codec, 0);
				   }

				   if (ast_idx >= 0) {
					   aCodec = avcodec_find_decoder(format_ctx->streams[ast_idx]->codecpar->codec_id);
					   audio_codec = aCodec;

					   aCodec_ctx = avcodec_alloc_context3(aCodec);
					   avcodec_parameters_to_context(aCodec_ctx, format_ctx->streams[ast_idx]->codecpar);
					   audio_codec_ctx = aCodec_ctx;

					   avcodec_open2(audio_codec_ctx, audio_codec, 0);
				   }
			   }
		   };*/
	public:
		StreamViewer(void)
		{
			InitializeComponent();
			//
			//TODO: 생성자 코드를 여기에 추가합니다.
			//
			isStreaming = false;
			//format_ctx = avformat_alloc_context();
		}

	protected:
		/// <summary>
		/// 사용 중인 모든 리소스를 정리합니다.
		/// </summary>
		~StreamViewer()
		{
			if (components)
			{
				delete components;
			}
			Stop_stream();
		}

	protected:

		// 파일 열기
		void open_file(Object^ sender, EventArgs^ e) {
			Exit_media(sender, e);
			OpenFile^ open_file = gcnew OpenFile();
			if (open_file->ShowDialog() == System::Windows::Forms::DialogResult::OK) {
				Stop_stream();
				String^ input_str = open_file->input_url;
				IntPtr ptrToNative = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(input_str);
				url = static_cast<char*>(ptrToNative.ToPointer());
				//Start_stream();
				Receive_stream();
			};
		}

		void clear_q() {
			std::lock_guard<std::mutex> lock(q_mutex);
			while (!frame_q.empty()) {
				AVFrame* frame = frame_q.front();
				Console::WriteLine("Clear Queue count: " + frame_q.size());
				frame_q.pop();
				av_frame_free(&frame);
			}
		}


		// 스트림 정지
		void Stop_stream() {
			isStreaming = false;
			clear_q();
		}

		void Receive_stream() {
			avformat_network_init();

			if (avformat_open_input(&fmt_ctx, url, NULL, NULL) != 0) {
				ShowError("Failed to open stream.");
				rtb_log->Text += gcnew String("url을 열 수 없습니다") + Environment::NewLine;
				return;
			}

			if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
				ShowError("Failed to retrieve stream info.");
				avformat_close_input(&fmt_ctx);
				return;
			}

			for (unsigned i = 0; i < fmt_ctx->nb_streams; i++) {
				if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
					vst_idx = i;
					vcodec = avcodec_find_decoder(fmt_ctx->streams[i]->codecpar->codec_id);
					break;
				}
			}

			if (!vcodec || vst_idx == -1) {
				ShowError("No video stream found.");
				avformat_close_input(&fmt_ctx);
				return;
			}

			vcodec_ctx = avcodec_alloc_context3(vcodec);
			avcodec_parameters_to_context(vcodec_ctx, fmt_ctx->streams[vst_idx]->codecpar);

			avcodec_open2(vcodec_ctx, vcodec, 0);

			float res_fps = float(fmt_ctx->streams[vst_idx]->r_frame_rate.num) / (fmt_ctx->streams[vst_idx]->r_frame_rate.den);
			String^ codec_info = "네트워크 주소" + Environment::NewLine +
				gcnew String(fmt_ctx->url) + Environment::NewLine + Environment::NewLine +
				"비디오 스트림 인덱스" + Environment::NewLine +
				gcnew int(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0)) + Environment::NewLine + Environment::NewLine +
				"코덱" + Environment::NewLine +
				gcnew String(vcodec->name) + " - " + gcnew String(vcodec->long_name) + Environment::NewLine + Environment::NewLine +
				/*"오디오 스트림 인덱스: " + gcnew int(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0));*/
				"해상도" + Environment::NewLine +
				gcnew int(vcodec_ctx->width) + " x " + gcnew int(vcodec_ctx->height) + Environment::NewLine + Environment::NewLine +
				"프레임 레이트" + Environment::NewLine +
				res_fps.ToString("F2") + " FPS" + Environment::NewLine + Environment::NewLine +
				"비트 레이트" + Environment::NewLine +
				gcnew int64_t(fmt_ctx->streams[vst_idx]->codecpar->bit_rate / 1000) + " kb/s" + Environment::NewLine + Environment::NewLine;

			rtb_log->Invoke(gcnew Action<String^>(this, &StreamViewer::Update_log), codec_info);



			if (isStreaming) return;

			isStreaming = true;

			Thread^ read_thread = gcnew Thread(gcnew ThreadStart(this, &StreamViewer::read_frame));
			Thread^ draw_thread = gcnew Thread(gcnew ThreadStart(this, &StreamViewer::draw_frame));
			read_thread->IsBackground = true;
			draw_thread->IsBackground = true;
			read_thread->Start();
			draw_thread->Start();
		}
		/*ref class AVFrameWrapper {
		public:
			AVFrame* Frame;
			AVFrameWrapper(AVFrame* frame) {
				Frame = frame;
			}
		};*/

		// 프레임 읽기 // 쓰레드 시작
		void read_frame() {
			if (!fmt_ctx) {
				Console::WriteLine("Error: fmt_ctx is nullptr");
			}
			vframe = av_frame_alloc();
			pkt = av_packet_alloc();
			if (!pkt) {
				ShowError("Failed to allocate AVPacket");
				return;
			}
			while (isStreaming)
			{
				int ret = av_read_frame(fmt_ctx, pkt);
				if (ret < 0) {
					Console::WriteLine("Error: Failed to read frame. Code: " + ret);
					break;
				}
				if (pkt->stream_index == vst_idx)
				{
					ret = avcodec_send_packet(vcodec_ctx, pkt);
					if (ret != 0)
					{
						continue;
					}
					while ((ret = avcodec_receive_frame(vcodec_ctx, vframe)) == 0)
					{
						if (ret == AVERROR(EAGAIN)) {
							break;
						}
						std::lock_guard<std::mutex> lock(q_mutex);
						frame_q.push(av_frame_clone(vframe));
						Console::WriteLine("Frame added to queue. Current count: " + frame_q.size());
					}
				}
				av_packet_unref(pkt);
			}
			av_frame_free(&vframe);
			av_frame_free(&rgb_frame);
			av_packet_free(&pkt);
			avcodec_free_context(&vcodec_ctx);
			avcodec_free_context(&acodec_ctx);
			avformat_close_input(&fmt_ctx);

			this->Invoke(gcnew Action(this, &StreamViewer::Stop_stream));

		}

		// 큐에서 프레임 가져오기
		AVFrame* Dequeue_frame() {
			std::lock_guard<std::mutex> lock(q_mutex);
			if (frame_q.empty()) return nullptr;
			AVFrame* frame = frame_q.front();
			frame_q.pop();
			return frame;
		}

		// PictureBox에 비트맵 이미지 띄우기
		void Update_pb(Bitmap^ dequeue_bitmap) {
			if (pb_media->Image != nullptr) {
				delete pb_media->Image;
			}
			pb_media->Image = dequeue_bitmap;
			Console::WriteLine("Frame dequeued and displayed. Current count: " + frame_q.size());
			frame_count++;
			this->Invoke(gcnew Action(this, &StreamViewer::FPS));
		}

		// 초당 프레임 출력
		void FPS() {
			DateTime now = DateTime::Now;
			TimeSpan elapsed = now - last_fps_time;
			if (elapsed.TotalSeconds >= 1.0) {
				int fps = frame_count;
				frame_count = 0;
				last_fps_time = now;
				lbl_fps->Text = "FPS: " + fps;
			}
		}

		void Update_pb_bitmap() {
			if (vcodec_ctx == nullptr || rgb_frame == nullptr) return;

			if (sws_ctx) {
				sws_freeContext(sws_ctx);
				sws_ctx = nullptr;
			}

			sws_ctx = sws_getContext(
				vcodec_ctx->width, vcodec_ctx->height, vcodec_ctx->pix_fmt,
				pb_media->Width, pb_media->Height, AV_PIX_FMT_BGR24,
				SWS_BILINEAR, NULL, NULL, NULL
			);

			if (sws_ctx == nullptr)
			{
				ShowError("Error reinitializing sws context.");
				return;
			}

			int buf_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, pb_media->Width, pb_media->Height, 1);
			uint8_t* rgb_buf = (uint8_t*)av_malloc(buf_size);

			if (!rgb_buf)
			{
				ShowError("Failed to allocate RGB buffer.");
				return;
			}

			av_frame_free(&rgb_frame);
			rgb_frame = av_frame_alloc();
			av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buf, AV_PIX_FMT_BGR24, pb_media->Width, pb_media->Height, 1);

			Console::WriteLine("Updated sws context and RGB buffer for new PictureBox size.");
		}

		void form_resize(Object^ sender, EventArgs^ e) {
			int form_width = tlp_media->ClientSize.Width;
			int form_height = tlp_media->ClientSize.Height;

			int target_width = form_width;
			int target_height = (form_width * 9) / 16;

			if (target_height > form_height) {
				target_height = form_height;
				target_width = (form_height * 16) / 9;
			}

			target_width = (target_width / 4) * 4;
			target_height = (target_height / 4) * 4;

			pb_media->Width = target_width;
			pb_media->Height = target_height;
			pb_media->Left = (form_width - target_width) / 2;
			pb_media->Top = (form_height - target_height) / 2;

			pb_media->SizeMode = PictureBoxSizeMode::Zoom;

			Update_pb_bitmap();
		}

		// 비트맵 이미지 그리기 // 쓰레드 시작
		void draw_frame() {
			rgb_frame = av_frame_alloc();
			while (isStreaming) {
				AVFrame* frame = Dequeue_frame();
				if (frame == nullptr) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));	// CPU 사용 줄이기
					continue;
				}

				if (sws_ctx == NULL) {
					sws_ctx = sws_getContext(
						frame->width, frame->height, vcodec_ctx->pix_fmt,
						pb_media->Width, pb_media->Height, AV_PIX_FMT_BGR24,
						SWS_BILINEAR, NULL, NULL, NULL
					);
					if (sws_ctx == NULL) {
						ShowError("Error initializing sws context.");
						av_frame_free(&frame);
						continue;
					}
					int buf_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
					uint8_t* rgb_buf = (uint8_t*)av_malloc(buf_size);
					if (!rgb_buf)
					{
						ShowError("Failed to allocate RGB buffer.");
						av_frame_free(&frame);
						continue;
					}
					/*av_frame_free(&rgb_frame);
					rgb_frame = av_frame_alloc();*/
					av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buf, AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
				}
				//Update_pb_bitmap();
				//av_frame_free(&rgb_frame);

				sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, rgb_frame->data, rgb_frame->linesize);
				Bitmap^ bitmap = gcnew Bitmap(frame->width, frame->height, rgb_frame->linesize[0],
					Imaging::PixelFormat::Format24bppRgb, IntPtr(rgb_frame->data[0])
				);

				pb_media->Invoke(gcnew Action<Bitmap^>(this, &StreamViewer::Update_pb), bitmap);
				/*int width = pb_media->Width;
				int height = pb_media->Height;

				if (width > 0 && height > 0) {
					try {
						Bitmap^ bitmap = gcnew Bitmap(
							vcodec_ctx->width, vcodec_ctx->height, rgb_frame->linesize[0],
							Imaging::PixelFormat::Format24bppRgb, IntPtr(rgb_frame->data[0])
						);

						pb_media->Invoke(gcnew Action<Bitmap^>(this, &StreamViewer::Update_pb), bitmap);
					}
					catch (System::ArgumentException^ ex) {
						Console::WriteLine("Bitmap creation error: " + ex->Message);
					}
				}
				else {
					Console::WriteLine("Invalid dimensions for Bitmap creation. Width: " + width + ", Height: " + height);
				}*/

				av_frame_free(&frame);
			}

			av_frame_free(&rgb_frame);
			sws_freeContext(sws_ctx);
			sws_ctx = NULL;

			this->Invoke(gcnew Action(this, &StreamViewer::Stop_stream));
		}



		// 스트림 정보 출력
		void Update_log(String^ codec_info) {
			rtb_log->Text += codec_info;
		}

		// 우클릭 이벤트
		void OnMouseDown(Object^ sender, MouseEventArgs^ e) {
			if (e->Button == System::Windows::Forms::MouseButtons::Right) {
				ctms_right->Show(pb_media, e->Location);
			}
		}

		// 스크린 샷
		void Screenshot(Object^ sender, EventArgs^ e) {
			DateTime^ date = DateTime::Now;
			String^ name = date->ToString("yyyy-MM-dd-HH.mm.ss");
			pb_media->Image->Save("C:/Users/kimss/Desktop/MediaPlayer Screenshot/" + name + ".jpg", Imaging::ImageFormat::Jpeg);
			MessageBox::Show("스크린샷 완료");
		}

		// 스트리밍 종료
		void Exit_media(Object^ sender, EventArgs^ e) {
			Stop_stream();
			clearTimer = gcnew System::Windows::Forms::Timer();
			clearTimer->Interval = 500;

			clearTimer->Tick += gcnew EventHandler(this, &StreamViewer::remove_pb);

			clearTimer->Start();
		}

		// PictureBox 및 로그 지우기
		void remove_pb(Object^ sender, EventArgs^ e) {
			clearTimer->Stop();
			pb_media->Image = nullptr;
			delete clearTimer;
			pb_media->Invalidate();
			rtb_log->Clear();
			lbl_fps->Text = "FPS:";
		}















		//void Start_stream() {
		//	if (isStreaming) return;

		//	isStreaming = true;
		//	// Launch stream processing in a separate thread
		//	Thread^ start_thread = gcnew Thread(gcnew ThreadStart(this, &StreamViewer::StreamRTP));
		//	//Thread^ dequeue_thread = gcnew Thread(gcnew ThreadStart(this, &StreamViewer::Dequeue_bitmap));
		//	start_thread->IsBackground = true;
		//	//dequeue_thread->IsBackground = true;
		//	start_thread->Start();
		//	//dequeue_thread->Sleep(3000);
		//	//dequeue_thread->Start();
		//	frame_timer = gcnew System::Windows::Forms::Timer();
		//	frame_timer->Interval = 33;
		//	frame_timer->Tick += gcnew EventHandler(this, &StreamViewer::Dequeue_bitmap);
		//	//frame_timer->Start();
		//	timer_delay = gcnew System::Windows::Forms::Timer();
		//	timer_delay->Interval = 1000;
		//	timer_delay->Tick += gcnew EventHandler(this, &StreamViewer::Timer_delay);

		//	timer_delay->Start();
		//}

		// 스트림
		//void StreamRTP() {
		//	// FFmpeg 초기화
		//	avformat_network_init();

		//	//FFmpeg^ call_format = gcnew FFmpeg();
		//	//call_format->Format();

		//	//FFmpeg^ call_codec = gcnew FFmpeg();
		//	//call_codec->Codec();
		//
		//	AVFormatContext* fmt_ctx = avformat_alloc_context();
		//	const AVCodec* codec = nullptr;
		//	AVCodecContext* codec_ctx = nullptr;
		//	SwsContext* sws_ctx = nullptr;
		//	SwsContext* screenshot_ctx = nullptr;
		//	AVPacket* pkt = av_packet_alloc();
		//	AVFrame* frame = av_frame_alloc();
		//	AVFrame* rgb_frame = av_frame_alloc();
		//	AVFrame* screenshot_rgb_frame = av_frame_alloc();
		//	//frame_queue = gcnew System::Collections::Concurrent::ConcurrentQueue<IntPtr>();
		//	bitmap_queue = gcnew Queue();
		//	
		//	queue_mutex = gcnew System::Threading::Mutex();

		//	if (avformat_open_input(&fmt_ctx, url, NULL, NULL) != 0) {
		//		ShowError("Failed to open stream.");
		//		rtb_log->Text += gcnew String("url을 열 수 없습니다") + Environment::NewLine;
		//		return;
		//	}

		//	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		//		ShowError("Failed to retrieve stream info.");
		//		avformat_close_input(&fmt_ctx);
		//		return;
		//	}

		//	int vst_idx = -1;
		//	for (unsigned i = 0; i < fmt_ctx->nb_streams; i++) {
		//		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
		//			vst_idx = i;
		//			codec = avcodec_find_decoder(fmt_ctx->streams[i]->codecpar->codec_id);
		//			vcodec = codec;
		//			break;
		//		}
		//	}

		//	if (!codec || vst_idx == -1) {
		//		ShowError("No video stream found.");
		//		avformat_close_input(&fmt_ctx);
		//		return;
		//	}

		//	codec_ctx = avcodec_alloc_context3(codec);
		//	vcodec_ctx = codec_ctx;
		//	avcodec_parameters_to_context(vcodec_ctx, fmt_ctx->streams[vst_idx]->codecpar);

		//	/*AVDictionary* options = nullptr;
		//	av_dict_set(&options, "framedrop", "1", 0);
		//	av_dict_set(&options, "max_delay", "500000", 0);*/


		//	int res = avcodec_open2(vcodec_ctx, codec, 0);
		//	if (res < 0) {
		//		MessageBox::Show("코덱 오픈 오류");
		//	}
		//	float res_fps = float(fmt_ctx->streams[vst_idx]->r_frame_rate.num) / (fmt_ctx->streams[vst_idx]->r_frame_rate.den);
		//	String^ codec_info = "네트워크 주소" + Environment::NewLine +
		//		gcnew String(fmt_ctx->url) + Environment::NewLine + Environment::NewLine +
		//		"비디오 스트림 인덱스" + Environment::NewLine +
		//		gcnew int(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0)) + Environment::NewLine + Environment::NewLine +
		//		"코덱" + Environment::NewLine +
		//		gcnew String(codec->name) + " - " + gcnew String(codec->long_name) + Environment::NewLine + Environment::NewLine +
		//		/*"오디오 스트림 인덱스: " + gcnew int(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0));*/
		//		"해상도" + Environment::NewLine +
		//		gcnew int(vcodec_ctx->width) + " x " + gcnew int(vcodec_ctx->height) + Environment::NewLine + Environment::NewLine +
		//		"프레임 레이트" + Environment::NewLine +
		//		res_fps.ToString("F2") + " FPS" + Environment::NewLine + Environment::NewLine +
		//		"비트 레이트" + Environment::NewLine +
		//		gcnew int64_t(fmt_ctx->streams[vst_idx]->codecpar->bit_rate / 1000) + " kb/s" + Environment::NewLine + Environment::NewLine;
		//		
		//	rtb_log->Invoke(gcnew Action<String^>(this, &StreamViewer::Update_log), codec_info);

		//	//int width = frame->width;
		//	//int height = frame->height;
		//	//uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
		//	//uint8_t* screenshot_buffer = (uint8_t*)av_malloc(bufferSize);
		//	//av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, buffer, AV_PIX_FMT_BGR24, width, height, 1);
		//	//av_image_fill_arrays(screenshot_rgb_frame->data, screenshot_rgb_frame->linesize, screenshot_buffer, AV_PIX_FMT_BGR24, width, height, 1);

		//	static auto last_frame_time = std::chrono::high_resolution_clock::now();
		//	//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		//	auto current_time = std::chrono::high_resolution_clock::now();
		//	int ret;

		//	while ((av_read_frame(fmt_ctx, pkt) == 0) && (isStreaming != false)) {
		//		if (pkt->stream_index == vst_idx) {
		//			
		//			ret = avcodec_send_packet(vcodec_ctx, pkt);
		//			if (ret != 0) {
		//				continue;
		//			}
		//			for (;;) {
		//				ret = avcodec_receive_frame(vcodec_ctx, frame);
		//				if (ret == AVERROR(EAGAIN)) {
		//					break;
		//				}
		//				if (isStreaming == false) {
		//					break;
		//				}
		//				//video_frame = frame;

		//				//{
		//					//queue_mutex->WaitOne();
		//					//IntPtr ptr = IntPtr(frame);
		//					//Console::WriteLine("Frame added to queue. Current count: " + frame_queue->Count);
		//					//frame_queue->Enqueue(ptr);
		//					//queue_mutex->ReleaseMutex();
		//				//}
		//				// 스케일링 컨텍스트 생성
		//				if (sws_ctx == NULL) {
		//					sws_ctx = sws_getContext(
		//						frame->width, frame->height, vcodec_ctx->pix_fmt,
		//						pb_media->Width, pb_media->Height, AV_PIX_FMT_BGR24,
		//						SWS_BILINEAR, NULL, NULL, NULL
		//					);
		//					screenshot_ctx = sws_getContext(
		//						frame->width, frame->height, vcodec_ctx->pix_fmt,
		//						frame->width, frame->height, AV_PIX_FMT_BGR24,
		//						SWS_BILINEAR, NULL, NULL, NULL
		//					);
		//					// 변환 결과를 저장할 프레임 버퍼 할당
		//					int buf_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
		//					uint8_t* rgb_buf = (uint8_t*)av_malloc(buf_size);
		//					uint8_t* screenshot_buffer = (uint8_t*)av_malloc(buf_size);
		//					av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buf, AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
		//					av_image_fill_arrays(screenshot_rgb_frame->data, screenshot_rgb_frame->linesize, screenshot_buffer, AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
		//				}

		//				// 변환 수행
		//				sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, rgb_frame->data, rgb_frame->linesize);
		//				sws_scale(screenshot_ctx, frame->data, frame->linesize, 0, frame->height, screenshot_rgb_frame->data, screenshot_rgb_frame->linesize);

		//				// 비트맵으로 뿌리기
		//				Bitmap^ bitmap = gcnew Bitmap(frame->width, frame->height, rgb_frame->linesize[0],
		//					Imaging::PixelFormat::Format24bppRgb, IntPtr(rgb_frame->data[0]));

		//				screenshot_bitmap = gcnew Bitmap(frame->width, frame->height, screenshot_rgb_frame->linesize[0],
		//					Imaging::PixelFormat::Format24bppRgb, IntPtr(screenshot_rgb_frame->data[0]));

		//				{
		//					queue_mutex->WaitOne();
		//					bitmap_queue->Enqueue(bitmap);
		//					Console::WriteLine("Frame added to queue. Current count: " + bitmap_queue->Count);
		//					queue_mutex->ReleaseMutex();
		//				}
		//				// Update PictureBox in UI thread
		//			}
		//		}
		//		av_packet_unref(pkt);
		//	}

		//	// Cleanup
		//	
		//	//av_free(screenshot_buffer);
		//	av_frame_free(&frame);
		//	av_frame_free(&rgb_frame);
		//	av_frame_free(&screenshot_rgb_frame);
		//	av_packet_free(&pkt);
		//	sws_freeContext(sws_ctx);
		//	sws_freeContext(screenshot_ctx);
		//	avcodec_free_context(&codec_ctx);
		//	avformat_close_input(&fmt_ctx);

		//	this->Invoke(gcnew Action(this, &StreamViewer::Stop_stream));
		//}



		/*AVFrame* GetAVFrameFromQueue(IntPtr ptr) {
			return static_cast<AVFrame*>(ptr.ToPointer());
		}*/

		/*Bitmap^ ConvertAVFrameToBitmap(AVFrame* frame) {
			AVFrame* rgb_frame = av_frame_alloc();
			SwsContext* sws_ctx = sws_getContext(frame->width, frame->height, video_codec_ctx->pix_fmt,
				pb_media->Width, pb_media->Height, AV_PIX_FMT_BGR24, SWS_BILINEAR, NULL, NULL, NULL);
			int buf_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
			uint8_t* rgb_buf = (uint8_t*)av_malloc(buf_size);
			av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buf, AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
			sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, rgb_frame->data, rgb_frame->linesize);

			Bitmap^ bitmap = gcnew Bitmap(frame->width, frame->height, rgb_frame->linesize[0], Imaging::PixelFormat::Format24bppRgb, IntPtr(rgb_frame->data[0]));
			av_frame_free(&frame);
			av_frame_free(&rgb_frame);
			return bitmap;
		}*/

		/*void RenderFrame(AVFrame* frame) {
			if (frame == nullptr) return;
			Bitmap^ bitmap = ConvertAVFrameToBitmap(frame);
			if (bitmap != nullptr) {
				Console::WriteLine("Frame dequeued and displayed. Current count: " + frame_queue->Count);
				pb_media->Image = bitmap;
			}
			av_frame_free(&frame);
		}*/

		/*void Process_frames() {
			IntPtr ptr;
			if (frame_queue->TryDequeue(ptr)) {
				AVFrame* frame = GetAVFrameFromQueue(ptr);
				RenderFrame(frame);
			}
		}*/

		/*Bitmap^ GetBitmapQueue() {
			if (bitmap_queue->Count > 0)
			{
				return (Bitmap^)bitmap_queue->Dequeue();
			}
			return nullptr;
		}*/

		//void Dequeue_bitmap(Object^ sender, EventArgs^ e) {
		//	Bitmap^ dequeue_bitmap = GetBitmapQueue();
		//	if (dequeue_bitmap != nullptr && isStreaming != false /* && pb_media->InvokeRequired*/) {
		//		//pb_media->Invoke(gcnew Action<Bitmap^>(this, &StreamViewer::Update_pb), dequeue_bitmap);
		//		Update_pb(dequeue_bitmap);
		//		//System::Threading::Tasks::Task::Delay(1000)->ContinueWith(gcnew Action<System::Threading::Tasks::Task^>(this, &StreamViewer::StartDelay), dequeue_bitmap);
		//	}
		//}
		/*void Dequeue_timer() {
			frame_timer = gcnew System::Windows::Forms::Timer();
			frame_timer->Interval = 33;
			frame_timer->Tick += gcnew EventHandler(this, &StreamViewer::Process_frames);

			timer_delay = gcnew System::Windows::Forms::Timer();
			timer_delay->Interval = 1000;
			timer_delay->Tick += gcnew EventHandler(this, &StreamViewer::Timer_delay);

			timer_delay->Start();
		}*/

		//void Timer_delay(Object^ sender, EventArgs^ e) {
		//	timer_delay->Stop();
		//	//System::Threading::Thread::Sleep(4000);
		//	frame_timer->Start();
		//}

		void ShowError(String^ message) {
			MessageBox::Show(message);
		}

	protected:

	protected:
	private: System::Windows::Forms::TableLayoutPanel^ tlp_main;
	private: System::Windows::Forms::PictureBox^ pb_media;
	private: System::Windows::Forms::TableLayoutPanel^ tlp_left;
	private: System::Windows::Forms::Button^ btn_open;

	private: System::Windows::Forms::ContextMenuStrip^ ctms_right;
	private: System::Windows::Forms::ToolStripMenuItem^ cms_screenshot;
	private: System::Windows::Forms::ToolStripMenuItem^ cms_exit;
	private: System::Windows::Forms::OpenFileDialog^ openFileDialog;
	private: System::Windows::Forms::Timer^ clearTimer;
	private: System::Windows::Forms::RichTextBox^ rtb_log;
	private: System::ComponentModel::IContainer^ components;
	private: System::Windows::Forms::Label^ lbl_time;
	private: System::Windows::Forms::Label^ lbl_fps;
	private: System::Windows::Forms::Timer^ frame_timer;
	private: System::Windows::Forms::Timer^ timer_delay;
		   //private: System::Windows::Forms::Timer^ frame_timer;
	private:
		/// <summary>
		/// 필수 디자이너 변수입니다.
		/// </summary>


#pragma region Windows Form Designer generated code
		/// <summary>
		/// 디자이너 지원에 필요한 메서드입니다. 
		/// 이 메서드의 내용을 코드 편집기로 수정하지 마세요.
		/// </summary>
		void InitializeComponent(void)
		{
			this->components = (gcnew System::ComponentModel::Container());
			this->tlp_main = (gcnew System::Windows::Forms::TableLayoutPanel());
			this->tlp_left = (gcnew System::Windows::Forms::TableLayoutPanel());
			this->btn_open = (gcnew System::Windows::Forms::Button());
			this->lbl_fps = (gcnew System::Windows::Forms::Label());
			this->rtb_log = (gcnew System::Windows::Forms::RichTextBox());
			this->lbl_time = (gcnew System::Windows::Forms::Label());
			this->tlp_media = (gcnew System::Windows::Forms::TableLayoutPanel());
			this->pb_media = (gcnew System::Windows::Forms::PictureBox());
			this->ctms_right = (gcnew System::Windows::Forms::ContextMenuStrip(this->components));
			this->cms_screenshot = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->cms_exit = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->openFileDialog = (gcnew System::Windows::Forms::OpenFileDialog());
			this->frame_timer = (gcnew System::Windows::Forms::Timer(this->components));
			this->timer_delay = (gcnew System::Windows::Forms::Timer(this->components));
			this->tlp_main->SuspendLayout();
			this->tlp_left->SuspendLayout();
			this->tlp_media->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pb_media))->BeginInit();
			this->ctms_right->SuspendLayout();
			this->SuspendLayout();
			// 
			// tlp_main
			// 
			this->tlp_main->AutoSizeMode = System::Windows::Forms::AutoSizeMode::GrowAndShrink;
			this->tlp_main->BackColor = System::Drawing::Color::FromArgb(static_cast<System::Int32>(static_cast<System::Byte>(64)), static_cast<System::Int32>(static_cast<System::Byte>(64)),
				static_cast<System::Int32>(static_cast<System::Byte>(64)));
			this->tlp_main->ColumnCount = 2;
			this->tlp_main->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 16.64549F)));
			this->tlp_main->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 83.35451F)));
			this->tlp_main->Controls->Add(this->tlp_left, 0, 0);
			this->tlp_main->Controls->Add(this->lbl_time, 1, 1);
			this->tlp_main->Controls->Add(this->tlp_media, 1, 0);
			this->tlp_main->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tlp_main->Location = System::Drawing::Point(0, 0);
			this->tlp_main->Name = L"tlp_main";
			this->tlp_main->RowCount = 2;
			this->tlp_main->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 88.29916F)));
			this->tlp_main->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 11.70084F)));
			this->tlp_main->Size = System::Drawing::Size(1574, 829);
			this->tlp_main->TabIndex = 4;
			// 
			// tlp_left
			// 
			this->tlp_left->ColumnCount = 1;
			this->tlp_left->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 100)));
			this->tlp_left->Controls->Add(this->btn_open, 0, 0);
			this->tlp_left->Controls->Add(this->lbl_fps, 0, 1);
			this->tlp_left->Controls->Add(this->rtb_log, 0, 2);
			this->tlp_left->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tlp_left->Location = System::Drawing::Point(3, 3);
			this->tlp_left->Name = L"tlp_left";
			this->tlp_left->RowCount = 4;
			this->tlp_left->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 80)));
			this->tlp_left->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 40)));
			this->tlp_left->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
			this->tlp_left->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 80)));
			this->tlp_left->Size = System::Drawing::Size(256, 726);
			this->tlp_left->TabIndex = 1;
			// 
			// btn_open
			// 
			this->btn_open->Dock = System::Windows::Forms::DockStyle::Fill;
			this->btn_open->Font = (gcnew System::Drawing::Font(L"맑은 고딕", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->btn_open->Location = System::Drawing::Point(20, 20);
			this->btn_open->Margin = System::Windows::Forms::Padding(20);
			this->btn_open->Name = L"btn_open";
			this->btn_open->Size = System::Drawing::Size(216, 40);
			this->btn_open->TabIndex = 0;
			this->btn_open->Text = L"Open";
			this->btn_open->UseVisualStyleBackColor = true;
			this->btn_open->Click += gcnew System::EventHandler(this, &StreamViewer::open_file);
			// 
			// lbl_fps
			// 
			this->lbl_fps->AutoSize = true;
			this->lbl_fps->Dock = System::Windows::Forms::DockStyle::Fill;
			this->lbl_fps->Font = (gcnew System::Drawing::Font(L"맑은 고딕", 18, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->lbl_fps->ForeColor = System::Drawing::Color::Lime;
			this->lbl_fps->Location = System::Drawing::Point(20, 80);
			this->lbl_fps->Margin = System::Windows::Forms::Padding(20, 0, 20, 0);
			this->lbl_fps->Name = L"lbl_fps";
			this->lbl_fps->Size = System::Drawing::Size(216, 40);
			this->lbl_fps->TabIndex = 1;
			this->lbl_fps->Text = L"FPS:";
			// 
			// rtb_log
			// 
			this->rtb_log->BackColor = System::Drawing::Color::Silver;
			this->rtb_log->Dock = System::Windows::Forms::DockStyle::Fill;
			this->rtb_log->Font = (gcnew System::Drawing::Font(L"맑은 고딕", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->rtb_log->Location = System::Drawing::Point(20, 140);
			this->rtb_log->Margin = System::Windows::Forms::Padding(20, 20, 0, 20);
			this->rtb_log->Name = L"rtb_log";
			this->rtb_log->Size = System::Drawing::Size(236, 486);
			this->rtb_log->TabIndex = 2;
			this->rtb_log->Text = L"";
			// 
			// lbl_time
			// 
			this->lbl_time->AutoSize = true;
			this->lbl_time->Dock = System::Windows::Forms::DockStyle::Left;
			this->lbl_time->Font = (gcnew System::Drawing::Font(L"맑은 고딕", 10.125F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->lbl_time->ForeColor = System::Drawing::Color::White;
			this->lbl_time->ImageAlign = System::Drawing::ContentAlignment::MiddleLeft;
			this->lbl_time->Location = System::Drawing::Point(265, 732);
			this->lbl_time->Name = L"lbl_time";
			this->lbl_time->Size = System::Drawing::Size(106, 97);
			this->lbl_time->TabIndex = 2;
			this->lbl_time->Text = L"00 : 00";
			// 
			// tlp_media
			// 
			this->tlp_media->ColumnCount = 1;
			this->tlp_media->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 100)));
			this->tlp_media->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Absolute, 20)));
			this->tlp_media->Controls->Add(this->pb_media, 0, 0);
			this->tlp_media->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tlp_media->Location = System::Drawing::Point(265, 3);
			this->tlp_media->Name = L"tlp_media";
			this->tlp_media->RowCount = 1;
			this->tlp_media->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
			this->tlp_media->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 20)));
			this->tlp_media->Size = System::Drawing::Size(1306, 726);
			this->tlp_media->TabIndex = 3;
			// 
			// pb_media
			// 
			this->pb_media->Anchor = System::Windows::Forms::AnchorStyles::None;
			this->pb_media->BackColor = System::Drawing::Color::Gray;
			this->pb_media->Location = System::Drawing::Point(20, 20);
			this->pb_media->Margin = System::Windows::Forms::Padding(20);
			this->pb_media->Name = L"pb_media";
			this->pb_media->Size = System::Drawing::Size(1266, 686);
			this->pb_media->TabIndex = 0;
			this->pb_media->TabStop = false;
			this->pb_media->MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &StreamViewer::OnMouseDown);
			// 
			// ctms_right
			// 
			this->ctms_right->ImageScalingSize = System::Drawing::Size(32, 32);
			this->ctms_right->Items->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(2) { this->cms_screenshot, this->cms_exit });
			this->ctms_right->Name = L"contextMenuStrip1";
			this->ctms_right->Size = System::Drawing::Size(217, 80);
			// 
			// cms_screenshot
			// 
			this->cms_screenshot->Name = L"cms_screenshot";
			this->cms_screenshot->Size = System::Drawing::Size(216, 38);
			this->cms_screenshot->Text = L"스크린샷";
			this->cms_screenshot->Click += gcnew System::EventHandler(this, &StreamViewer::Screenshot);
			// 
			// cms_exit
			// 
			this->cms_exit->Name = L"cms_exit";
			this->cms_exit->Size = System::Drawing::Size(216, 38);
			this->cms_exit->Text = L"스트림 종료";
			this->cms_exit->Click += gcnew System::EventHandler(this, &StreamViewer::Exit_media);
			// 
			// openFileDialog
			// 
			this->openFileDialog->FileName = L"openFileDialog";
			// 
			// frame_timer
			// 
			this->frame_timer->Enabled = true;
			this->frame_timer->Interval = 33;
			// 
			// timer_delay
			// 
			this->timer_delay->Interval = 5000;
			// 
			// StreamViewer
			// 
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Inherit;
			this->ClientSize = System::Drawing::Size(1574, 829);
			this->Controls->Add(this->tlp_main);
			this->Name = L"StreamViewer";
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->Text = L"미디어 플레이어";
			this->Resize += gcnew System::EventHandler(this, &StreamViewer::form_resize);
			this->tlp_main->ResumeLayout(false);
			this->tlp_main->PerformLayout();
			this->tlp_left->ResumeLayout(false);
			this->tlp_left->PerformLayout();
			this->tlp_media->ResumeLayout(false);
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pb_media))->EndInit();
			this->ctms_right->ResumeLayout(false);
			this->ResumeLayout(false);

		}

#pragma endregion
	};
	[STAThread]
		int main(array<System::String^>^ args) {
		Application::EnableVisualStyles();
		Application::SetCompatibleTextRenderingDefault(false);
		Application::Run(gcnew StreamViewer());
		return 0;
	}
}