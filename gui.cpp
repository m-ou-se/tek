#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Winconsistent-missing-override"
#pragma GCC diagnostic ignored "-Wpotentially-evaluated-expression"
#include <wx/wx.h>
#include <wx/aboutdlg.h>
#include <wx/gauge.h>
#include <wx/custombgwin.h>
#pragma GCC diagnostic pop

#include "ihex.hpp"
#include "usb.hpp"

#ifdef __linux
unsigned char background_png[] = {
#include "background.inc"
};
#endif

enum {
	ID_UPLOAD_THREAD_FINISHED,
	ID_PROGRESS_TIMER,
};

class MainFrame;

class UploadThread : public wxThread {
public:
	UploadThread(MainFrame *handler) : wxThread(wxTHREAD_DETACHED) { handler_ = handler; }
	~UploadThread();
protected:
	ExitCode Entry() override;
	MainFrame *handler_;
};

class MainFrame : public wxFrame {
public:
	MainFrame() : wxFrame(
		nullptr, wxID_ANY, "Truly Ergonomic Keyboard - Firmware Upgrade",
		wxPoint(50, 50), wxSize(537, 227),
		wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX)
	) {
		auto file = new wxMenu;
		auto help = new wxMenu;
		auto menu = new wxMenuBar;
		load_menu_ = file->Append(wxID_OPEN, "&Load...\tCtrl-O", "Load a firmware file.");
		upload_menu_ = file->Append(wxID_UP, "&Upload\tCtrl-U", "Upload the loaded file into the keyboard.");
		file->AppendSeparator();
		file->Append(wxID_EXIT);
		help->Append(wxID_ABOUT);
		menu->Append(file, "&File");
		menu->Append(help, "&Help");
		SetMenuBar(menu);
		CreateStatusBar();

		auto panel = new wxCustomBackgroundWindow<wxPanel>();
		panel->Create(this, -1, wxPoint(0, 0), wxSize(537, 227));

		panel->SetBackgroundBitmap(wxBITMAP_PNG(background));

		load_ = new wxButton(panel, wxID_OPEN, "1. Load file", wxPoint(), wxSize(100, 28));
		load_->SetPosition(wxPoint(30, 100 - load_->GetSize().GetHeight() / 2));

		filename_ = new wxTextCtrl(panel, -1, wxEmptyString, wxPoint(), wxSize(357, 22), wxTE_READONLY);
		filename_->SetPosition(wxPoint(150, 100 - filename_->GetSize().GetHeight() / 2));

		upload_ = new wxButton(panel, wxID_UP, "2. Upload", wxPoint(), wxSize(100, 28));
		upload_->SetPosition(wxPoint(30, 165 - upload_->GetSize().GetHeight() / 2));

		progress_ = new wxGauge(panel, -1, 100, wxPoint(), wxSize(357, 22));
		progress_->SetPosition(wxPoint(150, 165 - progress_->GetSize().GetHeight() / 2));

		timer_ = new wxTimer(this, ID_PROGRESS_TIMER);

		Fit();

		enable(true, false);

		SetStatusText("Ready.");
	}

	void enable(bool load, bool upload) {
		load_->Enable(load);
		load_menu_->Enable(load);
		upload_->Enable(upload);
		upload_menu_->Enable(upload);
		if (!upload) progress_->SetValue(0);
		if (load && !upload) load_->SetFocus();
		else if (upload) upload_->SetFocus();
	}

private:
	wxTextCtrl *filename_ = nullptr;
	wxButton *load_ = nullptr;
	wxMenuItem *load_menu_ = nullptr;
	wxButton *upload_ = nullptr;
	wxMenuItem *upload_menu_ = nullptr;
	wxGauge *progress_ = nullptr;
	UploadThread *upload_thread_ = nullptr;
	wxCriticalSection upload_thread_cs_;
	wxTimer *timer_ = nullptr;
	std::vector<std::uint8_t> firmware_;

	friend class UploadThread;

	void OnLoad(wxCommandEvent &e) {
		auto fn = wxFileSelector(
			"Select the firmware you want to upload.",
			wxEmptyString,
			wxEmptyString,
			wxEmptyString,
			"HEX files (*.hex)|*.hex|All files|*",
			0,
			this
		);
		if (!fn) return;
		enable(true, false);
		progress_->SetValue(0);
		SetStatusText("Loading file.");
		try {
			std::ifstream f(fn.ToStdString());
			if (!f) throw std::runtime_error{"Unable to open file."};
			firmware_ = load_ihex(f);
		} catch (std::exception &e) {
			SetStatusText("Error while loading file.");
			wxMessageBox(e.what(), "Error while loading file", wxICON_ERROR | wxOK | wxCENTRE, this);
			return;
		}
		filename_->SetValue(fn);
		SetStatusText("Ready to upload.");
		enable(true, true);
	}

	void OnUpload(wxCommandEvent &e) {
		wxCriticalSectionLocker enter(upload_thread_cs_);
		enable(false, false);
		SetStatusText("Uploading...");
		timer_->Start(50);
		upload_thread_ = new UploadThread(this);
		upload_thread_->Create(1024*1024);
		upload_thread_->Run();
	}

	void OnAbout(wxCommandEvent &e) {
		wxAboutDialogInfo info;
		info.SetName("Truly Ergonomic Keyboard Firmware Upgrade");
		info.SetVersion("1.0.0");
		info.SetDescription("This tool can upload new firmware to your Truly Ergonomic Keyboard over USB.");
		info.SetCopyright("2015 - Maurice Bos");
		wxAboutBox(info);
	}

	void OnExit(wxCommandEvent &e) {
		wxCriticalSectionLocker enter(upload_thread_cs_);
		if (upload_thread_) return;
		Close(true);
	}

	void OnUpdateThreadFinished(wxThreadEvent &e) {
		timer_->Stop();
		auto err = e.GetPayload<std::string>();
		if (err.empty()) {
			progress_->SetValue(100);
			SetStatusText("Keyboard firmware upgraded.");
			wxMessageBox("Succesfully upgraded keyboard firmware.", "Done");
		} else {
			progress_->SetValue(0);
			SetStatusText("Error while upgrading.");
			wxMessageBox("Error while upgrading keyboard firmware:\n\n" + err, "Error", wxICON_ERROR | wxOK | wxCENTRE);
		}
		enable(true, true);
	}

	void OnProgressTick(wxTimerEvent &e) {
		progress_->Pulse();
	}

	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
	EVT_MENU(wxID_OPEN,   MainFrame::OnLoad)
	EVT_MENU(wxID_UP,     MainFrame::OnUpload)
	EVT_MENU(wxID_EXIT,   MainFrame::OnExit)
	EVT_MENU(wxID_ABOUT,  MainFrame::OnAbout)
	EVT_BUTTON(wxID_OPEN, MainFrame::OnLoad)
	EVT_BUTTON(wxID_UP,   MainFrame::OnUpload)
	EVT_THREAD(ID_UPLOAD_THREAD_FINISHED, MainFrame::OnUpdateThreadFinished)
	EVT_TIMER(ID_PROGRESS_TIMER, MainFrame::OnProgressTick)
wxEND_EVENT_TABLE()

UploadThread::~UploadThread() {
	wxCriticalSectionLocker enter(handler_->upload_thread_cs_);
	handler_->upload_thread_ = nullptr;
}

wxThread::ExitCode UploadThread::Entry() {
	std::string err;
	try {
		auto c = usb::connect();
		auto &dev = c.first;
		auto &need_switch = c.second;
		if (!dev) throw std::runtime_error{"No Truly Ergonomic Keyboard found."};
		std::clog << "Found keyboard." << std::endl;
		if (need_switch) {
			std::clog << "It is in normal mode." << std::endl;
			int retries = 0;
			while (true) {
				if (dev) {
					std::clog << "Switching it to upgrade mode." << std::endl;
					usb::switch_mode(dev);
					usb::close(dev);
				}
				wxSleep(1);
				std::clog << "Reconnecting." << std::endl;
				c = usb::connect();
				if (!dev) {
					std::clog << "No keyboard found." << std::endl;
				} else if (need_switch) {
					std::clog << "Reconnected, but keyboard is in normal mode." << std::endl;
				} else {
					std::clog << "Reconnected to keyboard in upgrade mode." << std::endl;
					break;
				}
				if (++retries < 5) {
					std::clog << "Retrying." << std::endl;
				} else {
					std::clog << "Giving up." << std::endl;
					break;
				}
			}
			if (!dev) {
				throw std::runtime_error{"Keyboard did not reconnect after switching it to firmware upgrade mode.\nPlease reconnect it and try again."};
			}
			if (need_switch) {
				usb::close(dev);
				throw std::runtime_error{"Unable to switch keyboard to firmware upgrade mode. Is DIP #5 set to OFF?"};
			}
		} else {
			std::clog << "It is already in upgrade mode." << std::endl;
		}
		std::clog << "Uploading firmware." << std::endl;
		usb::program(dev, handler_->firmware_.data(), handler_->firmware_.size());
		std::clog << "Switching back to normal mode." << std::endl;
		usb::switch_mode(dev);
		usb::close(dev);
		std::clog << "Done." << std::endl;
	} catch (std::exception &e) {
		std::clog << "Error: " << e.what() << std::endl;
		err = e.what();
	}
	auto ev = new wxThreadEvent(wxEVT_THREAD, ID_UPLOAD_THREAD_FINISHED);
	ev->SetPayload<std::string>(std::move(err));
	wxQueueEvent(handler_, ev);
	return 0;
}

class TekApp : public wxApp {
public:
	bool OnInit() override {
		wxInitAllImageHandlers();
		auto frame = new MainFrame();
		frame->Show(true);
		return true;
	}
};

wxIMPLEMENT_APP(TekApp);
