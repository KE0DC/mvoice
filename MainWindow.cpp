/*
 *   Copyright (c) 2019-2020 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <chrono>

#include "MainWindow.h"
#include "WaitCursor.h"
#include "Utilities.h"
#include "TemplateClasses.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

static Glib::RefPtr<Gtk::Application> theApp;

CMainWindow::CMainWindow() :
	pWin(nullptr),
	pQuitButton(nullptr),
	pSettingsButton(nullptr),
	bDestCS(false),
	bDestIP(false),
	bTransOK(true)
{
	cfg.CopyTo(cfgdata);
	// allowed M17 " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."
	CallRegEx = std::regex("^(([1-9][A-Z])|([A-PR-Z][0-9])|([A-PR-Z][A-Z][0-9]))[0-9A-Z]*[A-Z][ ]*[ A-RT-Z]$", std::regex::extended);
	IPv4RegEx = std::regex("^((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])$", std::regex::extended);
	IPv6RegEx = std::regex("([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)$", std::regex::extended);
	M17RefRegEx = std::regex("^M17([A-Z0-9-]){4,4}[ ][A-Z]$", std::regex::extended);
	//M17CallRegEx = std::regex("^[A-Z0-9/\\.-]([ A-Z0-9/\\.-]){0,8}$", std::regex::extended);
	M17CallRegEx = std::regex("^(([1-9][A-Z])|([A-PR-Z][0-9])|([A-PR-Z][A-Z][0-9]))[0-9A-Z]*[A-Z][- /\\.]*[ A-Z]$", std::regex::extended);
}

CMainWindow::~CMainWindow()
{
	StopM17();
	if (pWin)
		delete pWin;
}

void CMainWindow::RunM17()
{
	std::cout << "Starting M17 Gateway..." << std::endl;
	if (! gateM17.Init(cfgdata, &routeMap))
		gateM17.Process();
	std::cout << "M17 Gateway has stopped." << std::endl;
}

void CMainWindow::SetState(const CFGDATA &data)
{
	pM17DestCallsignEntry->set_text(data.sM17DestCallsign);
	pM17DestIPEntry->set_text(data.sM17DestIp);
	if (cfg.IsOkay() && false == gateM17.keep_running)
		futM17 = std::async(std::launch::async, &CMainWindow::RunM17, this);
}

void CMainWindow::CloseAll()
{
	M172AM.Close();
	LogInput.Close();
}

bool CMainWindow::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name)
{
	std::string dbname(CFG_DIR);
	dbname.append("qn.db");
	if (qnDB.Open(dbname.c_str()))
		return true;
	qnDB.ClearLH();
	qnDB.ClearLS();

	if (M172AM.Open("m172am")) {
		CloseAll();
		return true;
	}

	if (LogInput.Open("log_input")) {
		CloseAll();
		return true;
	}

	if (AudioManager.Init(this)) {
		CloseAll();
		return true;
	}

 	builder->get_widget(name, pWin);
	if (nullptr == pWin) {
		CloseAll();
		std::cerr << "Failed to Initialize MainWindow!" << std::endl;
		return true;
	}

	//setup our css context and provider
	Glib::RefPtr<Gtk::CssProvider> css = Gtk::CssProvider::create();
	Glib::RefPtr<Gtk::StyleContext> style = Gtk::StyleContext::create();

	//load our red clicked style (applies to Gtk::ToggleButton)
	if (css->load_from_data("button:checked { background: red; }")) {
		style->add_provider_for_screen(pWin->get_screen(), css, GTK_STYLE_PROVIDER_PRIORITY_USER);
	}

	if (SettingsDlg.Init(builder, "SettingsDialog", pWin, this)) {
		CloseAll();
		return true;
	}

	if (AboutDlg.Init(builder, pWin)) {
		CloseAll();
		return true;
	}

	builder->get_widget("QuitButton", pQuitButton);
	builder->get_widget("SettingsButton", pSettingsButton);
	builder->get_widget("EchoTestButton", pEchoTestButton);
	builder->get_widget("PTTButton", pPTTButton);
	builder->get_widget("QuickKeyButton", pQuickKeyButton);
	builder->get_widget("ScrolledWindow", pScrolledWindow);
	builder->get_widget("LogTextView", pLogTextView);
	builder->get_widget("AboutMenuItem", pAboutMenuItem);
	builder->get_widget("M17DestActionButton", pM17DestActionButton);
	builder->get_widget("M17DestCallsignEntry", pM17DestCallsignEntry);
	builder->get_widget("M17DestIPEntry", pM17DestIPEntry);
	builder->get_widget("M17DestCallsignComboBox", pM17DestCallsignComboBox);
	builder->get_widget("M17LinkButton", pM17LinkButton);
	builder->get_widget("M17UnlinkButton", pM17UnlinkButton);

	pLogTextBuffer = pLogTextView->get_buffer();

	// events
	pM17DestCallsignEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_M17DestCallsignEntry_changed));
	pM17DestIPEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_M17DestIPEntry_changed));
	pM17DestCallsignComboBox->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_M17DestCallsignComboBox_changed));
	pM17DestActionButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_M17DestActionButton_clicked));
	pM17LinkButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_M17LinkButton_clicked));
	pM17UnlinkButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_M17UnlinkButton_clicked));
	pSettingsButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_SettingsButton_clicked));
	pQuitButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_QuitButton_clicked));
	pEchoTestButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_EchoTestButton_toggled));
	pPTTButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_PTTButton_toggled));
	pQuickKeyButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_QuickKeyButton_clicked));
	pAboutMenuItem->signal_activate().connect(sigc::mem_fun(*this, &CMainWindow::on_AboutMenuItem_activate));

	routeMap.Open();
	for (const auto &item : routeMap.GetKeys()) {
		// std::cout << "Addding " << item << " to M17 ComboBox" << std::endl;
		pM17DestCallsignComboBox->append(item);
	}
	if (routeMap.Size())
		pM17DestCallsignComboBox->set_active(0);
	Receive(false);
	SetState(cfgdata);
	on_M17DestCallsignComboBox_changed();

	// i/o events
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::RelayM172AM),  M172AM.GetFD(), Glib::IO_IN);
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::GetLogInput), LogInput.GetFD(), Glib::IO_IN);
	// idle processing
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &CMainWindow::TimeoutProcess), 1000);

	return false;
}

void CMainWindow::Run()
{
	theApp->run(*pWin);
}

void CMainWindow::on_QuitButton_clicked()
{
	CWaitCursor wait;
	AudioManager.KeyOff();
	StopM17();

	if (pWin)
		pWin->hide();
}

void CMainWindow::on_AboutMenuItem_activate()
{
	AboutDlg.Show();
}

void CMainWindow::on_SettingsButton_clicked()
{
	auto newdata = SettingsDlg.Show();
	if (newdata) {	// the user clicked okay so if anything changed. We'll shut things down and let SetState start things up again
		CWaitCursor wait;
		if (newdata->sM17SourceCallsign.compare(cfgdata.sM17SourceCallsign) || newdata->eNetType!=cfgdata.eNetType) {
			StopM17();
		}
		SetState(*newdata);
		cfg.CopyTo(cfgdata);
	}
}

void CMainWindow::on_M17DestCallsignComboBox_changed()
{
	auto cs = pM17DestCallsignComboBox->get_active_text();
	pM17DestCallsignEntry->set_text(cs);
	auto address = routeMap.Find(cs.c_str());
	if (address)
		pM17DestIPEntry->set_text(address->GetAddress());
}

void CMainWindow::on_M17DestActionButton_clicked()
{
	auto label = pM17DestActionButton->get_label();
	auto cs = pM17DestCallsignEntry->get_text();
	if (0 == label.compare("Save")) {
		routeMap.Update(cs, pM17DestIPEntry->get_text());
		pM17DestCallsignComboBox->remove_all();
		for (const auto &member : routeMap.GetKeys())
			pM17DestCallsignComboBox->append(member);
		pM17DestCallsignComboBox->set_active_text(cs);
	} else if (0 == label.compare("Delete")) {
		int index = pM17DestCallsignComboBox->get_active_row_number();
		pM17DestCallsignComboBox->remove_text(index);
		routeMap.Erase(cs);
		if (index >= int(routeMap.Size()))
			index--;
		if (index < 0) {
			pM17DestCallsignComboBox->unset_active();
			pM17DestIPEntry->set_text("");
		} else
			pM17DestCallsignComboBox->set_active(index);
	} else if (0 == label.compare("Update")) {
		routeMap.Update(cs, pM17DestIPEntry->get_text());
	}
	FixM17DestActionButton();
	routeMap.Save();
}

void CMainWindow::AudioSummary(const char *title)
{
		char line[640];
		double t = AudioManager.volStats.count * 0.000125;	// 0.000125 = 1 / 8000
		// we only do the sums of squares on every other point, so 0.5 mult in denominator
		// 25 db subtration for "ambient quiet", an arbitrary reference point
		double d = 20.0 * log10(sqrt(AudioManager.volStats.ss/(0.5 * AudioManager.volStats.count))) - 45.0;
		double c = 100.0 * AudioManager.volStats.clip / AudioManager.volStats.count;
		snprintf(line, 64, "%s Time=%.1fs Vol=%.0fdB Clip=%.0f%%\n", title, t, d, c);
		insertLogText(line);
}

void CMainWindow::on_EchoTestButton_toggled()
{
	if (pEchoTestButton->get_active()) {
		// record the mic to a queue
		AudioManager.RecordMicThread(E_PTT_Type::echo, "ECHOTEST");
	} else {
		AudioSummary("Echo");
		// play back the queue
		AudioManager.PlayEchoDataThread();
	}
}

void CMainWindow::Receive(bool is_rx)
{
	bTransOK = ! is_rx;
	bool ppt_okay = bTransOK && bDestCS && bDestIP;
	pPTTButton->set_sensitive(ppt_okay);
	pEchoTestButton->set_sensitive(bTransOK);
	pQuickKeyButton->set_sensitive(ppt_okay);
	if (bTransOK && AudioManager.volStats.count)
		AudioSummary("RX Audio");
}

void CMainWindow::on_PTTButton_toggled()
{
	const std::string dest(pM17DestCallsignEntry->get_text().c_str());
	if (pPTTButton->get_active()) {
		AudioManager.RecordMicThread(E_PTT_Type::m17, dest);
	} else {
		AudioManager.KeyOff();
		AudioSummary("PTT");
	}
}

void CMainWindow::on_QuickKeyButton_clicked()
{
	AudioManager.QuickKey(pM17DestCallsignEntry->get_text().c_str(), cfgdata.sM17SourceCallsign);
}

bool CMainWindow::RelayM172AM(Glib::IOCondition condition)
{
	if (condition & Glib::IO_IN) {
		SM17Frame m17;
		M172AM.Read(m17.magic, sizeof(SM17Frame));
		if (0 == memcmp(m17.magic, "M17 ", 4))
			AudioManager.M17_2AudioMgr(m17);
		else if (0 == memcmp(m17.magic, "PLAY", 4))
			AudioManager.PlayFile((char *)&m17.streamid);
	} else {
		std::cerr << "RelayM17_2AM not a read event!" << std::endl;
	}
	return true;
}

void CMainWindow::insertLogText(const char *line)
{
	static auto it = pLogTextBuffer->begin();
	if (strlen(line)) {
		it = pLogTextBuffer->insert(it, line);
		pLogTextView->scroll_to(it, 0.0, 0.0, 0.0);
	}
}

bool CMainWindow::GetLogInput(Glib::IOCondition condition)
{
	if (condition & Glib::IO_IN) {
		char line[256] = { 0 };
		LogInput.Read(line, 256);
		insertLogText(line);
	} else {
		std::cerr << "GetLogInput is not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::TimeoutProcess()
{
	std::list<CLink> linkstatus;
	if (qnDB.FindLS(cfgdata.cModule, linkstatus))	// get the link status list of our module (there should only be one, or none if it's not linked)
		return true;

	std::string call;
	if (linkstatus.size()) {	// extract the linked module from the returned list, if the list is empty, it means our module is not linked!
		CLink ls(linkstatus.front());
		call.assign(ls.callsign);
	}

	if (call.empty()) {
		pM17UnlinkButton->set_sensitive(false);
		std::string s(pM17DestCallsignEntry->get_text().c_str());
		pM17LinkButton->set_sensitive(std::regex_match(s, M17RefRegEx));
	} else {
		pM17LinkButton->set_sensitive(false);
		pM17UnlinkButton->set_sensitive(true);
	}
	return true;
}

void CMainWindow::on_M17DestCallsignEntry_changed()
{
	int pos = pM17DestCallsignEntry->get_position();
	Glib::ustring s = pM17DestCallsignEntry->get_text().uppercase();
	pM17DestCallsignEntry->set_text(s);
	pM17DestCallsignEntry->set_position(pos);
	bDestCS = std::regex_match(s.c_str(), M17CallRegEx) || std::regex_match(s.c_str(), M17RefRegEx);
	const auto addr = routeMap.FindBase(s);
	if (addr)
		pM17DestIPEntry->set_text(addr->GetAddress());
	pM17DestCallsignEntry->set_icon_from_icon_name(bDestCS ? "gtk-ok" : "gtk-cancel");
	FixM17DestActionButton();
}

void CMainWindow::on_M17DestIPEntry_changed()
{
	auto bIP4 = std::regex_match(pM17DestIPEntry->get_text().c_str(), IPv4RegEx);
	auto bIP6 = std::regex_match(pM17DestIPEntry->get_text().c_str(), IPv6RegEx);
	switch (cfgdata.eNetType) {
		case EInternetType::ipv4only:
			bDestIP = bIP4;
			break;
		case EInternetType::ipv6only:
			bDestIP = bIP6;
			break;
		default:
			bDestIP = (bIP4 || bIP6);
	}
	pM17DestIPEntry->set_icon_from_icon_name(bDestIP ? "gtk-ok" : "gtk-cancel");
	FixM17DestActionButton();
}

void CMainWindow::SetDestActionButton(const bool sensitive, const char *label)
{
	pM17DestActionButton->set_sensitive(sensitive);
	pM17DestActionButton->set_label(label);
}

void CMainWindow::on_M17LinkButton_clicked()
{
	std::string cmd("M17L");
	cmd.append(pM17DestCallsignEntry->get_text().c_str());
	AudioManager.Link(cmd);
}

void CMainWindow::on_M17UnlinkButton_clicked()
{
	std::string cmd("M17U");
	AudioManager.Link(cmd);
}

void CMainWindow::FixM17DestActionButton()
{
	const std::string cs(pM17DestCallsignEntry->get_text().c_str());
	const std::string ip(pM17DestIPEntry->get_text().c_str());
	if (bDestCS) {
		auto addr = routeMap.Find(cs);
		if (addr) {
			// cs is found in map
			if (bDestIP) { // is the IP okay?
				if (ip.compare(addr->GetAddress())) {
					// the ip in the IPEntry is different
					SetDestActionButton(true, "Update");
				} else {
					// perfect match
					SetDestActionButton(true, "Delete");
					pM17DestCallsignComboBox->set_active_text(cs);
				}
			} else {
				SetDestActionButton(false, "");
			}
		} else {
			// cs is not found in map
			if (bDestIP) { // is the IP okay?
				SetDestActionButton(true, "Save");
			} else {
				SetDestActionButton(false, "");
			}
		}
	} else {
		SetDestActionButton(false, "");
	}
	bool all = (bTransOK && bDestCS && bDestIP);
	pPTTButton->set_sensitive(all);
	pQuickKeyButton->set_sensitive(all);
}

int main (int argc, char **argv)
{
	theApp = Gtk::Application::create(argc, argv, "net.openquad.DVoice");

	//Load the GtkBuilder file and instantiate its widgets:
	Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();
	try
	{
		std::string path(CFG_DIR);
		builder->add_from_file(path + "MVoice.glade");
	}
	catch (const Glib::FileError& ex)
	{
		std::cerr << "FileError: " << ex.what() << std::endl;
		return 1;
	}
	catch (const Glib::MarkupError& ex)
	{
		std::cerr << "MarkupError: " << ex.what() << std::endl;
		return 1;
	}
	catch (const Gtk::BuilderError& ex)
	{
		std::cerr << "BuilderError: " << ex.what() << std::endl;
		return 1;
	}

	CMainWindow MainWindow;
	if (MainWindow.Init(builder, "AppWindow"))
		return 1;

	MainWindow.Run();

	return 0;
}

void CMainWindow::StopM17()
{
	if (gateM17.keep_running) {
		gateM17.keep_running = false;
		futM17.get();
	}
}
