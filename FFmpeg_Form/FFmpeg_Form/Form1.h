#pragma once

namespace MyCLRApp {
	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();
		}

	protected:
		~Form1()
		{
			if (components)
			{
				delete components;
			}
		}

	private:
		System::ComponentModel::Container^ components;

		void InitializeComponent(void)
		{
			this->Text = L"CLR 빈 프로젝트 윈폼";
			this->Width = 1600;
			this->Height = 900;

			Button^ btn1 = gcnew Button();
			btn1->Text = L"미디어 열기";
			btn1->Location = Point(20, 20);
			btn1->Click += gcnew EventHandler(this, &Form1::Button1_Click);

			this->Controls->Add(btn1);
		}

		void Button1_Click(System::Object^ sender, System::EventArgs^ e)
		{
			MessageBox::Show(L"안녕하세유");
		}
	};
}