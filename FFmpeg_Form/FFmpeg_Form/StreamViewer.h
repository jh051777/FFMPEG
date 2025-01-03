#pragma once
#include "OpenFile.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

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

	AVDictionary* options = nullptr;
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* vcodec_ctx = nullptr;
	AVCodecContext* acodec_ctx = nullptr;
	const AVCodec* vcodec = nullptr;
	const AVCodec* acodec = nullptr;
	AVPacket* pkt = nullptr;
	AVPixelFormat pix_fmt;
	AVFrame* vframe = nullptr;
	AVFrame* rgb_frame = nullptr;
	AVFrame* screenshot_frame = nullptr;
	SwsContext* sws_ctx = nullptr;
	SwsContext* screenshot_ctx = nullptr;
	std::queue<AVFrame*> frame_q;
	std::mutex q_mutex;
	std::mutex scale_mutex;
	std::condition_variable cv;
	volatile bool isStreaming;
	bool isResizing = false;
	Point resize_start_point;
	bool ready = false;
	char* url;

	int frame_count;
	std::atomic<int> second_counter(0);

	delegate void update_time(int minutes, int seconds);

	/// <summary>
	/// StreamViewer�� ���� ����Դϴ�.
	/// </summary>
	public ref class StreamViewer : public System::Windows::Forms::Form
	{
		DateTime last_fps_time;
		Bitmap^ screenshot_bitmap = nullptr;
		int media_width = 0;
		int media_height = 0;
		int vst_idx = -1;
		Thread^ read_thread;
		Thread^ draw_thread;
	private: System::Windows::Forms::SaveFileDialog^ sfd_screenshot;
	private: System::Windows::Forms::Label^ lbl_mouse;

		   Thread^ time_thread;
	public:
		StreamViewer(void)
		{
			InitializeComponent();
			//
			//TODO: ������ �ڵ带 ���⿡ �߰��մϴ�.
			//
			isStreaming = false;
			//format_ctx = avformat_alloc_context();
		}

	protected:
		/// <summary>
		/// ��� ���� ��� ���ҽ��� �����մϴ�.
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

		// ���� ����
		void open_file(Object^ sender, EventArgs^ e) {
			this->Invoke(gcnew EventHandler(this, &StreamViewer::Exit_media));
			OpenFile^ open_file = gcnew OpenFile();
			if (open_file->ShowDialog() == System::Windows::Forms::DialogResult::OK) {
				String^ input_str = open_file->input_url;
				IntPtr ptrToNative = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(input_str);
				url = static_cast<char*>(ptrToNative.ToPointer());
				Receive_stream();
			};
		}

		// ť ����
		void clear_q() {
			std::lock_guard<std::mutex> lock(q_mutex);
			while (!frame_q.empty()) {
				AVFrame* frame = frame_q.front();
				Console::WriteLine("Clear Queue count: " + frame_q.size());
				frame_q.pop();
				av_frame_free(&frame);
			}
		}


		// ��Ʈ�� ����
		void Stop_stream() {
			isStreaming = false;
			clear_q();
		}


		// ��Ʈ�� ���� ����
		void Receive_stream() {
			avformat_network_init();

			av_dict_set(&options, "timeout", "5000000", 0);

			//av_log_set_level(AV_LOG_DEBUG);
			int res = avformat_open_input(&fmt_ctx, url, NULL, &options);
			av_dict_free(&options);
			if (res < 0) {
				char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
				av_strerror(res, err_buf, sizeof(err_buf));
				std::cerr << "Error opening input: " << err_buf << std::endl;
				rtb_log->Text += gcnew String("url�� �� �� �����ϴ�") + Environment::NewLine;
				return;
			}

			if (res == AVERROR(EIO)) {
				std::cerr << "I/O Error" << std::endl;
				return;
			}
			else if (res == AVERROR(EINVAL)) {
				std::cerr << "Invalid URL or unsupported format." << std::endl;
				return;
			}
			else if (res == AVERROR(ENOMEM)) {
				std::cerr << "Memory allocation error." << std::endl;
				return;
			}

			res = avformat_find_stream_info(fmt_ctx, NULL);

			if (res < 0) {
				char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
				av_strerror(res, err_buf, sizeof(err_buf));
				std::cerr << "Error finding stream info: " << err_buf << std::endl;
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

			// �ȼ� ���� ����
			if (vcodec_ctx->pix_fmt == AV_PIX_FMT_YUVJ420P) {
				pix_fmt = AV_PIX_FMT_YUV420P;
				Console::WriteLine("[Receive_stream] �ȼ� ���� ���� yuvj420p->yuv420p");
			}
			avcodec_open2(vcodec_ctx, vcodec, 0);


			float res_fps = float(fmt_ctx->streams[vst_idx]->r_frame_rate.num) / (fmt_ctx->streams[vst_idx]->r_frame_rate.den);
			String^ codec_info = "��Ʈ��ũ �ּ�" + Environment::NewLine +
				gcnew String(fmt_ctx->url) + Environment::NewLine + Environment::NewLine +
				"���� ��Ʈ�� �ε���" + Environment::NewLine +
				gcnew int(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0)) + Environment::NewLine + Environment::NewLine +
				"�ڵ�" + Environment::NewLine +
				gcnew String(vcodec->name) + " - " + gcnew String(vcodec->long_name) + Environment::NewLine + Environment::NewLine +
				/*"����� ��Ʈ�� �ε���: " + gcnew int(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0));*/
				"�ػ�" + Environment::NewLine +
				gcnew int(vcodec_ctx->width) + " x " + gcnew int(vcodec_ctx->height) + Environment::NewLine + Environment::NewLine +
				"������ ����Ʈ" + Environment::NewLine +
				res_fps.ToString("F2") + " FPS" + Environment::NewLine + Environment::NewLine +
				"��Ʈ ����Ʈ" + Environment::NewLine +
				gcnew int64_t(fmt_ctx->streams[vst_idx]->codecpar->bit_rate / 1000) + " kb/s" + Environment::NewLine + Environment::NewLine;

			rtb_log->Invoke(gcnew Action<String^>(this, &StreamViewer::Update_log), codec_info);



			if (isStreaming) return;

			isStreaming = true;
			read_thread = gcnew Thread(gcnew ThreadStart(this, &StreamViewer::read_frame));
			draw_thread = gcnew Thread(gcnew ThreadStart(this, &StreamViewer::draw_frame));
			read_thread->IsBackground = true;
			draw_thread->IsBackground = true;
			read_thread->Start();
			draw_thread->Start();
		}

		// ������ �б� // ������ ����
		void read_frame() {
			if (!fmt_ctx) {
				Console::WriteLine("Error: fmt_ctx is nullptr");
			}
			pkt = av_packet_alloc();
			if (!pkt) {
				ShowError("Failed to allocate AVPacket");
				return;
			}
			while (isStreaming)
			{
				av_frame_free(&vframe);
				vframe = av_frame_alloc();
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
						// ���۸�
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
			//avcodec_free_context(&acodec_ctx);
			avformat_close_input(&fmt_ctx);

			this->Invoke(gcnew Action(this, &StreamViewer::Stop_stream));
			read_thread->Join();
		}

		// ť���� ������ ��������
		AVFrame* Dequeue_frame() {
			std::lock_guard<std::mutex> lock(q_mutex);
			if (frame_q.empty()) return nullptr;
			AVFrame* frame = frame_q.front();
			frame_q.pop();
			return frame;
		}

		// PictureBox�� ��Ʈ�� �̹��� ����
		void Update_pb(Bitmap^ dequeue_bitmap) {
			if (pb_media->Image != nullptr) {
				delete pb_media->Image;
				pb_media->Image = nullptr;
			}
			pb_media->Image = dequeue_bitmap;
			Console::WriteLine("[Update_pb] Frame dequeued and displayed. Current count: " + frame_q.size());
			frame_count++;
			this->Invoke(gcnew Action(this, &StreamViewer::FPS));
		}

		// �ʴ� ������ ���
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

		// ������ �����ϸ� �迭 ����
		void frame_scale() {
			Console::WriteLine("[frame_scale] �����ϸ� ����");
			if (vcodec_ctx == nullptr || rgb_frame == nullptr || screenshot_frame == nullptr) {
				std::cerr << "[frame_scale] vcodec_ctx or rgb_frame == nullptr" << std::endl;
				return;
			}

			// ���� sws_ctx ���� �� �缳��
			if (sws_ctx || screenshot_ctx) {
				sws_freeContext(sws_ctx);
				sws_freeContext(screenshot_ctx);
				sws_ctx = nullptr;
				screenshot_ctx = nullptr;
				Console::WriteLine("[frame_scale] ���� sws_ctx ���� �� �缳��");
			}

			sws_ctx = sws_getContext(
				vcodec_ctx->width, vcodec_ctx->height, pix_fmt,
				media_width, media_height, AV_PIX_FMT_BGR24,
				SWS_BILINEAR, NULL, NULL, NULL
			);
			screenshot_ctx = sws_getContext(
				vcodec_ctx->width, vcodec_ctx->height, pix_fmt,
				vcodec_ctx->width, vcodec_ctx->height, AV_PIX_FMT_BGR24,
				SWS_BILINEAR, NULL, NULL, NULL
			);
			Console::WriteLine("[frame_scale] sws_getContext ���� �Ϸ�");

			if (!sws_ctx) {
				ShowError("[frame_scale] Error reinitializing sws context.");
				return;
			}

			// RGB ���� �Ҵ�
			static uint8_t* rgb_buf = nullptr;
			static uint8_t* screenshot_buf = nullptr;
			static int buf_size = 0;
			int screenshot_buf_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, vcodec_ctx->width, vcodec_ctx->height, 1);
			int new_buf_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, media_width, media_height, 1);

			if (!rgb_buf || buf_size != new_buf_size)
			{
				rgb_buf = (uint8_t*)av_malloc(new_buf_size);
				screenshot_buf = (uint8_t*)av_malloc(screenshot_buf_size);
				buf_size = new_buf_size;
				if (!rgb_buf) {
					ShowError("Failed to allocate RGB buffer.");
					Console::WriteLine("[frame_scale] Failed to allocate RGB buffer.");
					return;
				}
			}

			// rgb_frame �Ҵ� �� ������ �迭 �ʱ�ȭ
			av_frame_free(&rgb_frame);
			av_frame_free(&screenshot_frame);
			rgb_frame = av_frame_alloc();
			screenshot_frame = av_frame_alloc();
			Console::WriteLine("[frame_scale] rgb_frame �Ҵ� �� ������ �迭 �ʱ�ȭ ���� �Ϸ�");

			av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buf, AV_PIX_FMT_BGR24,
				media_width, media_height, 1);
			av_image_fill_arrays(screenshot_frame->data, screenshot_frame->linesize, screenshot_buf, AV_PIX_FMT_BGR24,
				vcodec_ctx->width, vcodec_ctx->height, 1);

			//av_free(rgb_buf);
			//av_free(screenshot_buf);
			Console::WriteLine("[frame_scale] �����ӿ� ������ �迭 ���� �Ϸ�");
			//Console::WriteLine("Updated sws context and RGB buffer for new PictureBox size.");
			//av_frame_unref(rgb_frame);
			//av_frame_unref(screenshot_frame);

		}

		void mouse_hover(Object^ sender, EventArgs^ e) {
			lbl_mouse->Visible = true;
		}

		void mouse_leave(Object^ sender, EventArgs^ e) {
			lbl_mouse->Visible = false;
		}

		// MouseDown �̺�Ʈ
		void form_resize_start(Object^ sender, EventArgs^ e) {
			isResizing = true;
			Console::WriteLine("Resize Started");
			//form_resize(sender, e);
		}

		// MouseMove �̺�Ʈ
		void form_mouse_move(Object^ sender, MouseEventArgs^ e) {
			lbl_mouse->Text = "(" + e->X + ", " + e->Y + ")";
		}

		// MouseUp �̺�Ʈ
		void form_resize_end(Object^ sender, EventArgs^ e) {
			if (isResizing) {
				isResizing = false;
				Console::WriteLine("Resize Ended");
				form_resize(sender, e);
			}
		}

		void form_size_changed(Object^ sender, EventArgs^ e) {
			Console::WriteLine("Size Changed");
			form_resize(sender, e);
		}

		// �� ũ�� ���� �� PictureBox ũ�� ó�� �Լ�
		void form_resize(Object^ sender, EventArgs^ e) {
			try {

				if (isResizing) {
					Console::WriteLine("Skipping frame update during resize");
					return;
				}
				if (tlp_media->ClientSize.Width < 16 || tlp_media->ClientSize.Height < 9) {
					return;
				}
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

				if (pb_media->Width == target_width && pb_media->Height == target_height) {
					isResizing = false;
					return;
				}

				pb_media->Width = target_width;
				pb_media->Height = target_height;
				pb_media->Margin.Left = (form_width - target_width) / 2;
				pb_media->Margin.Top = (form_height - target_height) / 2;
				//pb_media->Location = Point(form_width - target_width / 2, form_height - target_width / 2);

				if (target_width < 1 || target_height < 1) {
					isResizing = false;
					return;
				}
				media_width = target_width;
				media_height = target_height;

				pb_media->SizeMode = PictureBoxSizeMode::Zoom;

				//this->Invoke(gcnew Action(this, &StreamViewer::frame_scale));
				Console::WriteLine("[form_resize] #############�� ������ ���� �����ϸ�");
				this->Text = L"�̵�� �÷��̾� (" + this->Width + ", " + this->Height + ")";
				//frame_scale();
				isResizing = false;
			}
			catch (Exception^ ex) {
				Console::WriteLine("[form_resize] Error during form resize: " + ex->Message);
			}
		}

		// ��Ʈ�� �̹��� �׸��� // ������ ����
		void draw_frame() {
			time_thread = gcnew Thread(gcnew ThreadStart(this, &StreamViewer::play_time));
			time_thread->IsBackground = true;
			time_thread->Start();

			const int target_fps = 33;
			const auto target_frame_duration = std::chrono::milliseconds(1000 / target_fps);
			this->Invoke(gcnew EventHandler(this, &StreamViewer::form_resize));
			Console::WriteLine("[draw_frame] ���� �� ������ �ҷ����� �Ϸ�");

			sws_ctx = sws_alloc_context();
			screenshot_ctx = sws_alloc_context();
			screenshot_frame = av_frame_alloc();
			rgb_frame = av_frame_alloc();
			Console::WriteLine("[draw_frame] ���ؽ�Ʈ, ������ alloc �Ϸ�");

			while (isStreaming) {
				auto frame_start_time = std::chrono::steady_clock::now();
				if (isResizing)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					Console::WriteLine("[draw_frame] ..............................Form Resizeing.............................");
					AVFrame* r_frame = frame_q.front();
					if (frame_q.size() > 1) {
						frame_q.pop();
						av_frame_free(&r_frame);
					}
					auto frame_end_time = std::chrono::steady_clock::now();
					auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end_time - frame_start_time);
					if (elapsed_time < target_frame_duration) {
						std::this_thread::sleep_for(target_frame_duration - elapsed_time);
					}
					continue;
				}

				if (frame_q.size() > 40) {
					while (frame_q.size() > 5) {
						AVFrame* d_frame = frame_q.front();
						frame_q.pop();
						av_frame_free(&d_frame);
					}
					Console::WriteLine("[draw_frame] ť Ŭ����: " + frame_q.size());
				}

				if (frame_q.size() > 5) {
					AVFrame* frame = Dequeue_frame();
					Console::WriteLine("[draw_frame 1] ������ Dequeue");
					if (!frame || !frame->data[0]) {
						av_frame_free(&frame);
						Console::WriteLine("[draw_frame] �����Ͱ� ����. ������ free");
						continue;
					}

					if (sws_ctx == nullptr) {
						Console::WriteLine("[draw_frame] sws_ctx is null, skipping frame.");
						av_frame_free(&frame);
						continue;
					}
					this->Invoke(gcnew Action(this, &StreamViewer::frame_scale));
					Console::WriteLine("[draw_frame 2] ��ο� �����忡�� �����ϸ� ����");
					sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, rgb_frame->data, rgb_frame->linesize);
					sws_scale(screenshot_ctx, frame->data, frame->linesize, 0, frame->height, screenshot_frame->data, screenshot_frame->linesize);
					Console::WriteLine("[draw_frame 3] �����ϸ� �Ϸ�");
					//Console::WriteLine("##########sws_scale executed successfully.#############");
					Bitmap^ bitmap = nullptr;

					try {

						bitmap = gcnew Bitmap(
							media_width, media_height, rgb_frame->linesize[0],
							Imaging::PixelFormat::Format24bppRgb, IntPtr(rgb_frame->data[0])
						);
						screenshot_bitmap = gcnew Bitmap(
							vcodec_ctx->width, vcodec_ctx->height, screenshot_frame->linesize[0],
							Imaging::PixelFormat::Format24bppRgb, IntPtr(screenshot_frame->data[0])
						);
						Console::WriteLine("[draw_frame 4] ��Ʈ�� �̹��� ���� �Ϸ�");

					}
					catch (System::Exception^ ex) {
						Console::WriteLine("[draw_frame] Bitmap update error: " + ex->Message);
						av_frame_free(&frame);
					}

					this->Invoke(gcnew Action<Bitmap^>(this, &StreamViewer::Update_pb), bitmap);
					Console::WriteLine("[draw_frame 5] ��Ʈ�� ��� �Լ� ����");
					//Update_pb(bitmap);
					av_frame_free(&frame);
					Console::WriteLine("[draw_frame 6] ���� ���� �� ������ free");
				}
				else {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					Console::WriteLine("[draw_frame] ť ����� 10���� �����Ƿ� dequeue���� ����");
				}

				auto frame_end_time = std::chrono::steady_clock::now();
				auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end_time - frame_start_time);
				if (elapsed_time < target_frame_duration) {
					std::this_thread::sleep_for(target_frame_duration - elapsed_time);
				}
			}
			sws_freeContext(sws_ctx);
			sws_freeContext(screenshot_ctx);
			sws_ctx = nullptr;
			screenshot_ctx = nullptr;
			Console::WriteLine("[draw_frame] ��ο� ������ ���ؽ�Ʈ ����");

			if (rgb_frame || screenshot_frame) {
				av_frame_free(&rgb_frame);
				av_frame_free(&screenshot_frame);
				delete pb_media->Image;
				Console::WriteLine("[draw_frame] ��ο� �����忡�� �迭�� ����� ������ ����");
			}

			this->Invoke(gcnew Action(this, &StreamViewer::Stop_stream));
			draw_thread->Join();
		}

		// ��� �ð� ó�� �Լ� // ������ ����
		void play_time() {
			auto play_start_time = std::chrono::steady_clock::now();
			while (isStreaming) {
				auto play_current_time = std::chrono::steady_clock::now();
				auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(play_current_time - play_start_time);
				if (elapsed_time.count() >= (second_counter.load() + 1)) {
					second_counter++;
					int total_seconds = second_counter.load();
					int minutes = total_seconds / 60;
					int seconds = total_seconds % 60;

					this->Invoke(gcnew update_time(this, &StreamViewer::set_timer), minutes, seconds);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			second_counter = 0;
			time_thread->Join();
		}

		// ��� �ð� ����
		void set_timer(int minutes, int seconds) {
			lbl_time->Text = String::Format("{0:D2} : {1:D2}", minutes, seconds);
		}

		// ��Ʈ�� ���� ���
		void Update_log(String^ codec_info) {
			rtb_log->Text += codec_info;
		}

		// ��Ŭ�� �̺�Ʈ
		void OnMouseDown(Object^ sender, MouseEventArgs^ e) {
			if (e->Button == System::Windows::Forms::MouseButtons::Right) {
				ctms_right->Show(pb_media, e->Location);
			}
		}

		// ��ũ�� ��
		void Screenshot(Object^ sender, EventArgs^ e) {
			if (pb_media->Image == nullptr)
			{
				MessageBox::Show("�̹����� �����ϴ�.");
				return;
			}
			DateTime^ date = DateTime::Now;
			String^ name = date->ToString("yyyy-MM-dd-HH.mm.ss");

			sfd_screenshot->Filter = "JPEG Image|*.jpg";
			sfd_screenshot->DefaultExt = "jpg";
			sfd_screenshot->AddExtension = true;
			sfd_screenshot->FileName = name + ".jpg";

			if (sfd_screenshot->ShowDialog() == System::Windows::Forms::DialogResult::OK) {
				String^ file_path = sfd_screenshot->FileName;
				screenshot_bitmap->Save(file_path, Imaging::ImageFormat::Jpeg);

				MessageBox::Show("��ũ���� �Ϸ�");
			}
		}

		// ��Ʈ���� ����
		void Exit_media(Object^ sender, EventArgs^ e) {
			Stop_stream();
			clearTimer = gcnew System::Windows::Forms::Timer();
			clearTimer->Interval = 300;

			clearTimer->Tick += gcnew EventHandler(this, &StreamViewer::remove_pb);

			clearTimer->Start();
		}

		// PictureBox �� �α� �����
		void remove_pb(Object^ sender, EventArgs^ e) {
			clearTimer->Stop();
			pb_media->Image = nullptr;
			delete clearTimer;
			pb_media->Invalidate();
			rtb_log->Clear();
			lbl_fps->Text = "FPS:";
			lbl_time->Text = "00 : 00";
		}

		void ShowError(String^ message) {
			MessageBox::Show(message);
		}














		//void Start_stream() {
		//	if (isStreaming) return;
		//
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
//
		//	timer_delay->Start();
		//}
//
		// ��Ʈ��
		//void StreamRTP() {
		//	// FFmpeg �ʱ�ȭ
		//	avformat_network_init();
//
		//	//FFmpeg^ call_format = gcnew FFmpeg();
		//	//call_format->Format();
//
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
//
		//	if (avformat_open_input(&fmt_ctx, url, NULL, NULL) != 0) {
		//		ShowError("Failed to open stream.");
		//		rtb_log->Text += gcnew String("url�� �� �� �����ϴ�") + Environment::NewLine;
		//		return;
		//	}
//
		//	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		//		ShowError("Failed to retrieve stream info.");
		//		avformat_close_input(&fmt_ctx);
		//		return;
		//	}
//
		//	int vst_idx = -1;
		//	for (unsigned i = 0; i < fmt_ctx->nb_streams; i++) {
		//		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
		//			vst_idx = i;
		//			codec = avcodec_find_decoder(fmt_ctx->streams[i]->codecpar->codec_id);
		//			vcodec = codec;
		//			break;
		//		}
		//	}
//
		//	if (!codec || vst_idx == -1) {
		//		ShowError("No video stream found.");
		//		avformat_close_input(&fmt_ctx);
		//		return;
		//	}
//
		//	codec_ctx = avcodec_alloc_context3(codec);
		//	vcodec_ctx = codec_ctx;
		//	avcodec_parameters_to_context(vcodec_ctx, fmt_ctx->streams[vst_idx]->codecpar);
//
		//	/*AVDictionary* options = nullptr;
		//	av_dict_set(&options, "framedrop", "1", 0);
		//	av_dict_set(&options, "max_delay", "500000", 0);*/
//
//
		//	int res = avcodec_open2(vcodec_ctx, codec, 0);
		//	if (res < 0) {
		//		MessageBox::Show("�ڵ� ���� ����");
		//	}
		//	float res_fps = float(fmt_ctx->streams[vst_idx]->r_frame_rate.num) / (fmt_ctx->streams[vst_idx]->r_frame_rate.den);
		//	String^ codec_info = "��Ʈ��ũ �ּ�" + Environment::NewLine +
		//		gcnew String(fmt_ctx->url) + Environment::NewLine + Environment::NewLine +
		//		"���� ��Ʈ�� �ε���" + Environment::NewLine +
		//		gcnew int(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0)) + Environment::NewLine + Environment::NewLine +
		//		"�ڵ�" + Environment::NewLine +
		//		gcnew String(codec->name) + " - " + gcnew String(codec->long_name) + Environment::NewLine + Environment::NewLine +
		//		/*"����� ��Ʈ�� �ε���: " + gcnew int(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0));*/
		//		"�ػ�" + Environment::NewLine +
		//		gcnew int(vcodec_ctx->width) + " x " + gcnew int(vcodec_ctx->height) + Environment::NewLine + Environment::NewLine +
		//		"������ ����Ʈ" + Environment::NewLine +
		//		res_fps.ToString("F2") + " FPS" + Environment::NewLine + Environment::NewLine +
		//		"��Ʈ ����Ʈ" + Environment::NewLine +
		//		gcnew int64_t(fmt_ctx->streams[vst_idx]->codecpar->bit_rate / 1000) + " kb/s" + Environment::NewLine + Environment::NewLine;
		//		
		//	rtb_log->Invoke(gcnew Action<String^>(this, &StreamViewer::Update_log), codec_info);
//
		//	//int width = frame->width;
		//	//int height = frame->height;
		//	//uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
		//	//uint8_t* screenshot_buffer = (uint8_t*)av_malloc(bufferSize);
		//	//av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, buffer, AV_PIX_FMT_BGR24, width, height, 1);
		//	//av_image_fill_arrays(screenshot_rgb_frame->data, screenshot_rgb_frame->linesize, screenshot_buffer, AV_PIX_FMT_BGR24, width, height, 1);
//
		//	static auto last_frame_time = std::chrono::high_resolution_clock::now();
		//	//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		//	auto current_time = std::chrono::high_resolution_clock::now();
		//	int ret;
//
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
//
		//				//{
		//					//queue_mutex->WaitOne();
		//					//IntPtr ptr = IntPtr(frame);
		//					//Console::WriteLine("Frame added to queue. Current count: " + frame_queue->Count);
		//					//frame_queue->Enqueue(ptr);
		//					//queue_mutex->ReleaseMutex();
		//				//}
		//				// �����ϸ� ���ؽ�Ʈ ����
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
		//					// ��ȯ ����� ������ ������ ���� �Ҵ�
		//					int buf_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
		//					uint8_t* rgb_buf = (uint8_t*)av_malloc(buf_size);
		//					uint8_t* screenshot_buffer = (uint8_t*)av_malloc(buf_size);
		//					av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buf, AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
		//					av_image_fill_arrays(screenshot_rgb_frame->data, screenshot_rgb_frame->linesize, screenshot_buffer, AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
		//				}
//
		//				// ��ȯ ����
		//				sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, rgb_frame->data, rgb_frame->linesize);
		//				sws_scale(screenshot_ctx, frame->data, frame->linesize, 0, frame->height, screenshot_rgb_frame->data, screenshot_rgb_frame->linesize);
//
		//				// ��Ʈ������ �Ѹ���
		//				Bitmap^ bitmap = gcnew Bitmap(frame->width, frame->height, rgb_frame->linesize[0],
		//					Imaging::PixelFormat::Format24bppRgb, IntPtr(rgb_frame->data[0]));
//
		//				screenshot_bitmap = gcnew Bitmap(frame->width, frame->height, screenshot_rgb_frame->linesize[0],
		//					Imaging::PixelFormat::Format24bppRgb, IntPtr(screenshot_rgb_frame->data[0]));
//
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
//
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
//
		//	this->Invoke(gcnew Action(this, &StreamViewer::Stop_stream));
		//}
//
	//	
//
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
		//
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
	private: System::Windows::Forms::TableLayoutPanel^ tlp_media;
	private:
		/// <summary>
		/// �ʼ� �����̳� �����Դϴ�.
		/// </summary>


#pragma region Windows Form Designer generated code
		/// <summary>
		/// �����̳� ������ �ʿ��� �޼����Դϴ�. 
		/// �� �޼����� ������ �ڵ� ������� �������� ������.
		/// </summary>
		void InitializeComponent(void)
		{
			this->components = (gcnew System::ComponentModel::Container());
			this->tlp_main = (gcnew System::Windows::Forms::TableLayoutPanel());
			this->tlp_left = (gcnew System::Windows::Forms::TableLayoutPanel());
			this->btn_open = (gcnew System::Windows::Forms::Button());
			this->lbl_fps = (gcnew System::Windows::Forms::Label());
			this->lbl_time = (gcnew System::Windows::Forms::Label());
			this->rtb_log = (gcnew System::Windows::Forms::RichTextBox());
			this->tlp_media = (gcnew System::Windows::Forms::TableLayoutPanel());
			this->pb_media = (gcnew System::Windows::Forms::PictureBox());
			this->lbl_mouse = (gcnew System::Windows::Forms::Label());
			this->ctms_right = (gcnew System::Windows::Forms::ContextMenuStrip(this->components));
			this->cms_screenshot = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->cms_exit = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->openFileDialog = (gcnew System::Windows::Forms::OpenFileDialog());
			this->frame_timer = (gcnew System::Windows::Forms::Timer(this->components));
			this->timer_delay = (gcnew System::Windows::Forms::Timer(this->components));
			this->sfd_screenshot = (gcnew System::Windows::Forms::SaveFileDialog());
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
			this->tlp_main->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 17.34435F)));
			this->tlp_main->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 82.65565F)));
			this->tlp_main->Controls->Add(this->tlp_left, 0, 0);
			this->tlp_main->Controls->Add(this->tlp_media, 1, 0);
			this->tlp_main->Controls->Add(this->lbl_mouse, 1, 1);
			this->tlp_main->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tlp_main->Location = System::Drawing::Point(0, 0);
			this->tlp_main->Margin = System::Windows::Forms::Padding(0);
			this->tlp_main->Name = L"tlp_main";
			this->tlp_main->RowCount = 2;
			this->tlp_main->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 88.29916F)));
			this->tlp_main->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 11.70084F)));
			this->tlp_main->Size = System::Drawing::Size(1600, 900);
			this->tlp_main->TabIndex = 4;
			// 
			// tlp_left
			// 
			this->tlp_left->ColumnCount = 1;
			this->tlp_left->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 100)));
			this->tlp_left->Controls->Add(this->btn_open, 0, 0);
			this->tlp_left->Controls->Add(this->lbl_fps, 0, 1);
			this->tlp_left->Controls->Add(this->lbl_time, 0, 3);
			this->tlp_left->Controls->Add(this->rtb_log, 0, 2);
			this->tlp_left->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tlp_left->Location = System::Drawing::Point(3, 3);
			this->tlp_left->MaximumSize = System::Drawing::Size(250, 0);
			this->tlp_left->Name = L"tlp_left";
			this->tlp_left->RowCount = 4;
			this->tlp_left->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 80)));
			this->tlp_left->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 40)));
			this->tlp_left->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
			this->tlp_left->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 80)));
			this->tlp_left->Size = System::Drawing::Size(250, 788);
			this->tlp_left->TabIndex = 1;
			// 
			// btn_open
			// 
			this->btn_open->Dock = System::Windows::Forms::DockStyle::Fill;
			this->btn_open->Font = (gcnew System::Drawing::Font(L"���� ���", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->btn_open->Location = System::Drawing::Point(20, 20);
			this->btn_open->Margin = System::Windows::Forms::Padding(20);
			this->btn_open->Name = L"btn_open";
			this->btn_open->Size = System::Drawing::Size(210, 40);
			this->btn_open->TabIndex = 0;
			this->btn_open->Text = L"Open";
			this->btn_open->UseVisualStyleBackColor = true;
			this->btn_open->Click += gcnew System::EventHandler(this, &StreamViewer::open_file);
			// 
			// lbl_fps
			// 
			this->lbl_fps->AutoSize = true;
			this->lbl_fps->Dock = System::Windows::Forms::DockStyle::Fill;
			this->lbl_fps->Font = (gcnew System::Drawing::Font(L"���� ���", 18, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->lbl_fps->ForeColor = System::Drawing::Color::Lime;
			this->lbl_fps->Location = System::Drawing::Point(20, 80);
			this->lbl_fps->Margin = System::Windows::Forms::Padding(20, 0, 20, 0);
			this->lbl_fps->Name = L"lbl_fps";
			this->lbl_fps->Size = System::Drawing::Size(210, 40);
			this->lbl_fps->TabIndex = 1;
			this->lbl_fps->Text = L"FPS:";
			// 
			// lbl_time
			// 
			this->lbl_time->AutoSize = true;
			this->lbl_time->Dock = System::Windows::Forms::DockStyle::Right;
			this->lbl_time->Font = (gcnew System::Drawing::Font(L"���� ���", 16.125F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->lbl_time->ForeColor = System::Drawing::Color::White;
			this->lbl_time->ImageAlign = System::Drawing::ContentAlignment::MiddleLeft;
			this->lbl_time->Location = System::Drawing::Point(81, 708);
			this->lbl_time->Name = L"lbl_time";
			this->lbl_time->Size = System::Drawing::Size(166, 80);
			this->lbl_time->TabIndex = 2;
			this->lbl_time->Text = L"00 : 00";
			// 
			// rtb_log
			// 
			this->rtb_log->BackColor = System::Drawing::Color::Silver;
			this->rtb_log->Dock = System::Windows::Forms::DockStyle::Fill;
			this->rtb_log->Font = (gcnew System::Drawing::Font(L"���� ���", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->rtb_log->Location = System::Drawing::Point(20, 140);
			this->rtb_log->Margin = System::Windows::Forms::Padding(20, 20, 0, 20);
			this->rtb_log->Name = L"rtb_log";
			this->rtb_log->Size = System::Drawing::Size(230, 548);
			this->rtb_log->TabIndex = 2;
			this->rtb_log->Text = L"";
			// 
			// tlp_media
			// 
			this->tlp_media->BackColor = System::Drawing::Color::Black;
			this->tlp_media->ColumnCount = 1;
			this->tlp_media->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle()));
			this->tlp_media->Controls->Add(this->pb_media, 0, 1);
			this->tlp_media->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tlp_media->Location = System::Drawing::Point(280, 3);
			this->tlp_media->Name = L"tlp_media";
			this->tlp_media->RowCount = 1;
			this->tlp_media->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
			this->tlp_media->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 20)));
			this->tlp_media->Size = System::Drawing::Size(1317, 788);
			this->tlp_media->TabIndex = 3;
			// 
			// pb_media
			// 
			this->pb_media->Anchor = System::Windows::Forms::AnchorStyles::None;
			this->pb_media->BackColor = System::Drawing::Color::Gray;
			this->pb_media->Location = System::Drawing::Point(17, 10);
			this->pb_media->Margin = System::Windows::Forms::Padding(0);
			this->pb_media->Name = L"pb_media";
			this->pb_media->Size = System::Drawing::Size(1282, 768);
			this->pb_media->TabIndex = 0;
			this->pb_media->TabStop = false;
			this->pb_media->MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &StreamViewer::OnMouseDown);
			this->pb_media->MouseLeave += gcnew System::EventHandler(this, &StreamViewer::mouse_leave);
			this->pb_media->MouseHover += gcnew System::EventHandler(this, &StreamViewer::mouse_hover);
			this->pb_media->MouseMove += gcnew System::Windows::Forms::MouseEventHandler(this, &StreamViewer::form_mouse_move);
			// 
			// lbl_mouse
			// 
			this->lbl_mouse->AutoSize = true;
			this->lbl_mouse->Dock = System::Windows::Forms::DockStyle::Right;
			this->lbl_mouse->Font = (gcnew System::Drawing::Font(L"���� ���", 9, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->lbl_mouse->ForeColor = System::Drawing::Color::White;
			this->lbl_mouse->Location = System::Drawing::Point(1597, 794);
			this->lbl_mouse->Name = L"lbl_mouse";
			this->lbl_mouse->Size = System::Drawing::Size(0, 106);
			this->lbl_mouse->TabIndex = 4;
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
			this->cms_screenshot->Text = L"��ũ����";
			this->cms_screenshot->Click += gcnew System::EventHandler(this, &StreamViewer::Screenshot);
			// 
			// cms_exit
			// 
			this->cms_exit->Name = L"cms_exit";
			this->cms_exit->Size = System::Drawing::Size(216, 38);
			this->cms_exit->Text = L"��Ʈ�� ����";
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
			this->ClientSize = System::Drawing::Size(1600, 900);
			this->Controls->Add(this->tlp_main);
			this->MinimumSize = System::Drawing::Size(800, 400);
			this->Name = L"StreamViewer";
			this->SizeGripStyle = System::Windows::Forms::SizeGripStyle::Show;
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->Text = L"�̵�� �÷��̾� (, )";
			this->Shown += gcnew System::EventHandler(this, &StreamViewer::form_resize);
			this->ResizeBegin += gcnew System::EventHandler(this, &StreamViewer::form_resize_start);
			this->ResizeEnd += gcnew System::EventHandler(this, &StreamViewer::form_resize_end);
			this->SizeChanged += gcnew System::EventHandler(this, &StreamViewer::form_size_changed);
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
