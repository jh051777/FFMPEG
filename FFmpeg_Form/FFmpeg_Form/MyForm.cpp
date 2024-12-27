#include "MyForm.h"
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
}

using namespace System;
using namespace System::Windows::Forms;

[STAThread]
int Main(array<String^>^ args)
{
	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false);

	FFmpegForm::MyForm form;
	Application::Run(% form);
	return 0;
}