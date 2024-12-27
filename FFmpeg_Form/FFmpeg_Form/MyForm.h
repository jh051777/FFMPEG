#pragma once
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}
namespace FFmpegForm {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// MyForm�� ���� ����Դϴ�.
	/// </summary>
	public ref class MyForm : public System::Windows::Forms::Form
	{
	public:
		MyForm(void)
		{
			InitializeComponent();
			//
			//TODO: ������ �ڵ带 ���⿡ �߰��մϴ�.
			//
		}

	protected:
		/// <summary>
		/// ��� ���� ��� ���ҽ��� �����մϴ�.
		/// </summary>
		~MyForm()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::TableLayoutPanel^ tableLayoutPanel1;
	private: System::Windows::Forms::Button^ btn_openMedia;
	private: System::Windows::Forms::Panel^ video;
	private: System::Windows::Forms::RichTextBox^ tb_log;
	private: System::Windows::Forms::PictureBox^ pictureBox1;
	private: bool running;


	protected:

	protected:

	private:
		/// <summary>
		/// �ʼ� �����̳� �����Դϴ�.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// �����̳� ������ �ʿ��� �޼����Դϴ�. 
		/// �� �޼����� ������ �ڵ� ������� �������� ������.
		/// </summary>
		void InitializeComponent(void)
		{
            this->tableLayoutPanel1 = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->btn_openMedia = (gcnew System::Windows::Forms::Button());
            this->video = (gcnew System::Windows::Forms::Panel());
            this->pictureBox1 = (gcnew System::Windows::Forms::PictureBox());
            this->tb_log = (gcnew System::Windows::Forms::RichTextBox());
            this->tableLayoutPanel1->SuspendLayout();
            this->video->SuspendLayout();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox1))->BeginInit();
            this->SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            this->tableLayoutPanel1->ColumnCount = 2;
            this->tableLayoutPanel1->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                100)));
            this->tableLayoutPanel1->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Absolute,
                1280)));
            this->tableLayoutPanel1->Controls->Add(this->btn_openMedia, 0, 0);
            this->tableLayoutPanel1->Controls->Add(this->video, 1, 0);
            this->tableLayoutPanel1->Dock = System::Windows::Forms::DockStyle::Fill;
            this->tableLayoutPanel1->Location = System::Drawing::Point(0, 0);
            this->tableLayoutPanel1->Name = L"tableLayoutPanel1";
            this->tableLayoutPanel1->RowCount = 2;
            this->tableLayoutPanel1->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 720)));
            this->tableLayoutPanel1->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->tableLayoutPanel1->Size = System::Drawing::Size(1584, 861);
            this->tableLayoutPanel1->TabIndex = 0;
            // 
            // btn_openMedia
            // 
            this->btn_openMedia->Anchor = System::Windows::Forms::AnchorStyles::Top;
            this->btn_openMedia->Font = (gcnew System::Drawing::Font(L"���� ���", 9, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
                static_cast<System::Byte>(129)));
            this->btn_openMedia->Location = System::Drawing::Point(53, 3);
            this->btn_openMedia->Name = L"btn_openMedia";
            this->btn_openMedia->Size = System::Drawing::Size(197, 31);
            this->btn_openMedia->TabIndex = 0;
            this->btn_openMedia->Text = L"�̵�� ����";
            this->btn_openMedia->UseVisualStyleBackColor = true;
            this->btn_openMedia->Click += gcnew System::EventHandler(this, &MyForm::Button1_Click);
            // 
            // video
            // 
            this->video->BackColor = System::Drawing::SystemColors::HotTrack;
            this->video->Controls->Add(this->pictureBox1);
            this->video->Controls->Add(this->tb_log);
            this->video->Dock = System::Windows::Forms::DockStyle::Fill;
            this->video->Location = System::Drawing::Point(334, 30);
            this->video->Margin = System::Windows::Forms::Padding(30);
            this->video->Name = L"video";
            this->video->Size = System::Drawing::Size(1220, 660);
            this->video->TabIndex = 1;
            // 
            // pictureBox1
            // 
            this->pictureBox1->BackColor = System::Drawing::SystemColors::Window;
            this->pictureBox1->Dock = System::Windows::Forms::DockStyle::Fill;
            this->pictureBox1->Location = System::Drawing::Point(0, 0);
            this->pictureBox1->Name = L"pictureBox1";
            this->pictureBox1->Size = System::Drawing::Size(1220, 660);
            this->pictureBox1->TabIndex = 1;
            this->pictureBox1->TabStop = false;
            // 
            // tb_log
            // 
            this->tb_log->Location = System::Drawing::Point(3, 3);
            this->tb_log->Name = L"tb_log";
            this->tb_log->Size = System::Drawing::Size(706, 259);
            this->tb_log->TabIndex = 0;
            this->tb_log->Text = L"";
            // 
            // MyForm
            // 
            this->AutoScaleDimensions = System::Drawing::SizeF(7, 12);
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
            this->ClientSize = System::Drawing::Size(1584, 861);
            this->Controls->Add(this->tableLayoutPanel1);
            this->Name = L"MyForm";
            this->Text = L"MyForm";
            this->tableLayoutPanel1->ResumeLayout(false);
            this->video->ResumeLayout(false);
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox1))->EndInit();
            this->ResumeLayout(false);

        }
		void Button1_Click(System::Object^ sender, System::EventArgs^ e)
		{
			running = true;
			MessageBox::Show(L"�̵�� ���� ����");
			ProcessVideo();
		}
		void ProcessVideo() {
        // RTP ��Ʈ�� URL
        std::string rtpStreamUrl = "rtsp://211.189.132.124:554/AIXTEAM03?mt=2&res=004";

        // FFmpeg �ʱ�ȭ
        AVFormatContext* formatContext = avformat_alloc_context();
        if (!formatContext) {
            Console::WriteLine("Failed to allocate format context");
            return;
        }

        // ��Ʈ�� ����
        if (avformat_open_input(&formatContext, rtpStreamUrl.c_str(), nullptr, nullptr) < 0) {
            Console::WriteLine("Failed to open input stream");
            avformat_free_context(formatContext);
            return;
        }

        // ��Ʈ�� ���� ã��
        if (avformat_find_stream_info(formatContext, nullptr) < 0) {
            Console::WriteLine("Failed to find stream info");
            avformat_close_input(&formatContext);
            return;
        }

        // ���� ��Ʈ�� ã��
        int videoStreamIndex = -1;
        for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
            if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = i;
                break;
            }
        }

        if (videoStreamIndex == -1) {
            Console::WriteLine("Failed to find video stream");
            avformat_close_input(&formatContext);
            return;
        }

        // �ڵ� ����
        AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
        if (!codec) {
            Console::WriteLine("Failed to find decoder");
            avformat_close_input(&formatContext);
            return;
        }

        AVCodecContext* codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            Console::WriteLine("Failed to allocate codec context");
            avformat_close_input(&formatContext);
            return;
        }

        if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
            Console::WriteLine("Failed to copy codec parameters");
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        if (avcodec_open2(codecContext, codec, nullptr) < 0) {
            Console::WriteLine("Failed to open codec");
            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        // ���ڵ� ����
        AVFrame* frame = av_frame_alloc();
        AVFrame* rgbFrame = av_frame_alloc();
        AVPacket* packet = av_packet_alloc();

        int width = codecContext->width;
        int height = codecContext->height;

        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
        uint8_t* buffer = (uint8_t*)av_malloc(numBytes);

        struct SwsContext* swsContext = sws_getContext(
            width, height, codecContext->pix_fmt,
            width, height, AV_PIX_FMT_BGR24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer, AV_PIX_FMT_BGR24, width, height, 1);

        while (running && av_read_frame(formatContext, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex) {
                if (avcodec_send_packet(codecContext, packet) >= 0) {
                    while (avcodec_receive_frame(codecContext, frame) >= 0) {
                        // RGB�� ��ȯ
                        sws_scale(swsContext, frame->data, frame->linesize, 0, height, rgbFrame->data, rgbFrame->linesize);

                        // Bitmap ����
                        Bitmap^ bmp = gcnew Bitmap(width, height, rgbFrame->linesize[0], Imaging::PixelFormat::Format24bppRgb, IntPtr(rgbFrame->data[0]));

                        // PictureBox�� �̹��� ǥ��
						pictureBox1->Invoke(gcnew Action<Bitmap^>(this, &MyForm::UpdatePictureBox), bmp);
                    }
                }
            }
            av_packet_unref(packet);
        }

        // ���ҽ� ����
        av_free(buffer);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        av_packet_free(&packet);
        sws_freeContext(swsContext);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
    }

	void UpdatePictureBox(Bitmap^ bmp) {
		if (pictureBox1->Image) delete pictureBox1->Image;
		pictureBox1->Image = bmp;
	}
	};
}
