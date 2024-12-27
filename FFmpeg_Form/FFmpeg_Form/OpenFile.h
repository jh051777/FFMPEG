#pragma once


namespace FFmpegForm {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// OpenFile에 대한 요약입니다.
	/// </summary>
	public ref class OpenFile : public System::Windows::Forms::Form
	{
	public:
		OpenFile(void)
		{
			InitializeComponent();
			//
			//TODO: 생성자 코드를 여기에 추가합니다.
			//
		}
		String^ input_url;
	protected:
		/// <summary>
		/// 사용 중인 모든 리소스를 정리합니다.
		/// </summary>
		~OpenFile()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::TableLayoutPanel^ tableLayoutPanel1;
	private: System::Windows::Forms::TextBox^ tb_url;
	private: System::Windows::Forms::Label^ lbl_url;
	private: System::Windows::Forms::Button^ btn_done;

	protected:

	private:
		/// <summary>
		/// 필수 디자이너 변수입니다.
		/// </summary>
		System::ComponentModel::Container ^components;

		void url(Object^ sender, EventArgs^ e) {
			input_url = tb_url->Text;
			tb_url->Text = input_url;
			this->DialogResult = System::Windows::Forms::DialogResult::OK;
			this->Close();
		}
#pragma region Windows Form Designer generated code
		/// <summary>
		/// 디자이너 지원에 필요한 메서드입니다. 
		/// 이 메서드의 내용을 코드 편집기로 수정하지 마세요.
		/// </summary>
		void InitializeComponent(void)
		{
			this->tableLayoutPanel1 = (gcnew System::Windows::Forms::TableLayoutPanel());
			this->tb_url = (gcnew System::Windows::Forms::TextBox());
			this->lbl_url = (gcnew System::Windows::Forms::Label());
			this->btn_done = (gcnew System::Windows::Forms::Button());
			this->tableLayoutPanel1->SuspendLayout();
			this->SuspendLayout();
			// 
			// tableLayoutPanel1
			// 
			this->tableLayoutPanel1->ColumnCount = 2;
			this->tableLayoutPanel1->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
				100)));
			this->tableLayoutPanel1->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Absolute,
				664)));
			this->tableLayoutPanel1->Controls->Add(this->tb_url, 1, 1);
			this->tableLayoutPanel1->Controls->Add(this->lbl_url, 0, 1);
			this->tableLayoutPanel1->Controls->Add(this->btn_done, 1, 2);
			this->tableLayoutPanel1->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tableLayoutPanel1->Location = System::Drawing::Point(0, 0);
			this->tableLayoutPanel1->Name = L"tableLayoutPanel1";
			this->tableLayoutPanel1->RowCount = 3;
			this->tableLayoutPanel1->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 100)));
			this->tableLayoutPanel1->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
			this->tableLayoutPanel1->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 100)));
			this->tableLayoutPanel1->Size = System::Drawing::Size(774, 329);
			this->tableLayoutPanel1->TabIndex = 0;
			// 
			// tb_url
			// 
			this->tb_url->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tb_url->Font = (gcnew System::Drawing::Font(L"맑은 고딕", 9));
			this->tb_url->Location = System::Drawing::Point(113, 110);
			this->tb_url->Margin = System::Windows::Forms::Padding(3, 10, 20, 3);
			this->tb_url->Name = L"tb_url";
			this->tb_url->Size = System::Drawing::Size(641, 39);
			this->tb_url->TabIndex = 0;
			// 
			// lbl_url
			// 
			this->lbl_url->AutoSize = true;
			this->lbl_url->Dock = System::Windows::Forms::DockStyle::Top;
			this->lbl_url->Font = (gcnew System::Drawing::Font(L"맑은 고딕", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->lbl_url->Location = System::Drawing::Point(3, 100);
			this->lbl_url->Name = L"lbl_url";
			this->lbl_url->Size = System::Drawing::Size(104, 45);
			this->lbl_url->TabIndex = 1;
			this->lbl_url->Text = L"url:";
			this->lbl_url->TextAlign = System::Drawing::ContentAlignment::MiddleRight;
			// 
			// btn_done
			// 
			this->btn_done->Dock = System::Windows::Forms::DockStyle::Fill;
			this->btn_done->Font = (gcnew System::Drawing::Font(L"맑은 고딕", 9, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(129)));
			this->btn_done->Location = System::Drawing::Point(130, 249);
			this->btn_done->Margin = System::Windows::Forms::Padding(20);
			this->btn_done->Name = L"btn_done";
			this->btn_done->Size = System::Drawing::Size(624, 60);
			this->btn_done->TabIndex = 2;
			this->btn_done->Text = L"확인";
			this->btn_done->UseVisualStyleBackColor = true;
			this->btn_done->Click += gcnew System::EventHandler(this, &OpenFile::url);
			// 
			// OpenFile
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(13, 24);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(774, 329);
			this->Controls->Add(this->tableLayoutPanel1);
			this->MaximumSize = System::Drawing::Size(800, 400);
			this->MinimumSize = System::Drawing::Size(800, 400);
			this->Name = L"OpenFile";
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->Text = L"파일 열기";
			this->tableLayoutPanel1->ResumeLayout(false);
			this->tableLayoutPanel1->PerformLayout();
			this->ResumeLayout(false);

		}
#pragma endregion
	};
}
