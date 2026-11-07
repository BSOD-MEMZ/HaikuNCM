#include "LoginWindow.h"

#include "net/netease.h"

#include <Catalog.h>

#include <qrcodegen/qrcodegen.hpp>

#include <Button.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "LoginWindow"

#include <algorithm>
#include <string>

namespace {

const uint32 kMessageSendCode = 'send';
const uint32 kMessageSmsLogin = 'smlg';
const uint32 kMessagePwdLogin = 'pwdg';
const uint32 kMessageQrStart = 'qrs0';
const uint32 kMessageQrKey = 'qrky';
const uint32 kMessageQrState = 'qrst';
const uint32 kMessageOk = 'succ';
const uint32 kMessageFail = 'fail';

BString Prefixed(const char* prefix, const std::string& detail)
{
	BString text(prefix);
	text << detail.c_str();
	return text;
}

struct Job {
	LoginWindow* window;
	std::string first;
	std::string second;
};

} // namespace

// ---------------------------------------------------------------- QrView

QrView::QrView()
	: BView("qr", B_WILL_DRAW)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}

void QrView::GetPreferredSize(float* width, float* height)
{
	*width = 168.0f;
	*height = 168.0f;
}

void QrView::SetCode(const char* text)
{
	fModules.clear();
	fSize = 0;

	if (text != nullptr && text[0] != '\0') {
		try {
			const qrcodegen::QrCode code = qrcodegen::QrCode::encodeText(text,
				qrcodegen::QrCode::Ecc::MEDIUM);
			fSize = code.getSize();
			fModules.resize(static_cast<size_t>(fSize) * fSize);
			for (int y = 0; y < fSize; y++) {
				for (int x = 0; x < fSize; x++)
					fModules[y * fSize + x] = code.getModule(x, y);
			}
		} catch (...) {
			fSize = 0;
			fModules.clear();
		}
	}

	Invalidate();
}

void QrView::Clear()
{
	fModules.clear();
	fSize = 0;
	Invalidate();
}

void QrView::Draw(BRect)
{
	SetHighColor((rgb_color){ 255, 255, 255, 255 });
	FillRect(Bounds());

	if (fSize <= 0 || fModules.empty())
		return;

	const float side = std::min(Bounds().Width(), Bounds().Height());
	const float module = side / (fSize + 2);
	SetHighColor((rgb_color){ 0, 0, 0, 255 });

	for (int y = 0; y < fSize; y++) {
		for (int x = 0; x < fSize; x++) {
			if (!fModules[y * fSize + x])
				continue;
			const BRect rect(module * (x + 1), module * (y + 1),
				module * (x + 2) - 1, module * (y + 2) - 1);
			FillRect(rect);
		}
	}
}

// ------------------------------------------------------------ LoginWindow

LoginWindow::LoginWindow(BMessenger target)
	: BWindow(BRect(0, 0, 380, 400), B_TRANSLATE("登录网易云音乐"),
		  B_TITLED_WINDOW,
		  B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
	, fTarget(target)
{
	auto* tabs = new BTabView("tabs", B_WIDTH_FROM_LABEL);

	BTab* smsTab = new BTab();
	tabs->AddTab(_BuildSmsTab(), smsTab);
	smsTab->SetLabel(B_TRANSLATE("手机验证码"));

	BTab* qrTab = new BTab();
	tabs->AddTab(_BuildQrTab(), qrTab);
	qrTab->SetLabel(B_TRANSLATE("扫码"));

	BTab* passwordTab = new BTab();
	tabs->AddTab(_BuildPasswordTab(), passwordTab);
	passwordTab->SetLabel(B_TRANSLATE("密码"));

	fStatus = new BStringView("status",
		B_TRANSLATE("验证码登录最稳，账密容易被风控"));

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(tabs)
		.Add(fStatus)
		.End();

	CenterOnScreen();
}

LoginWindow::~LoginWindow()
{
	fQrRunning = false;
}

BView* LoginWindow::_BuildSmsTab()
{
	auto* view = new BView("sms", B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fSmsPhone = new BTextControl("sms-phone", B_TRANSLATE("手机号"), "",
		nullptr);
	fSmsPhone->SetModificationMessage(nullptr);

	fCode = new BTextControl("code", B_TRANSLATE("验证码"), "", nullptr);
	fCode->SetModificationMessage(nullptr);

	fCodeButton = new BButton("send-code", B_TRANSLATE("获取验证码"),
		new BMessage(kMessageSendCode));

	BButton* login = new BButton("sms-login", B_TRANSLATE("登录"),
		new BMessage(kMessageSmsLogin));
	login->MakeDefault(true);

	BLayoutBuilder::Group<>(view, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fSmsPhone)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fCode)
			.Add(fCodeButton)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.AddGlue()
			.Add(login)
		.End()
		.AddGlue()
		.End();

	return view;
}

BView* LoginWindow::_BuildQrTab()
{
	auto* view = new BView("qr-tab", B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fQr = new QrView();
	fQrStatus = new BStringView("qr-status", B_TRANSLATE("尚未取到二维码"));

	BButton* refresh = new BButton("refresh", B_TRANSLATE("刷新二维码"),
		new BMessage(kMessageQrStart));

	BLayoutBuilder::Group<>(view, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.AddGlue()
			.Add(fQr)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fQrStatus, 1.0f)
			.Add(refresh)
		.End()
		.AddGlue()
		.End();

	return view;
}

BView* LoginWindow::_BuildPasswordTab()
{
	auto* view = new BView("password-tab", B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fPhone = new BTextControl("phone", B_TRANSLATE("手机号"), "", nullptr);
	fPhone->SetModificationMessage(nullptr);

	fPassword = new BTextView("password");
	fPassword->HideTyping(true);
	fPassword->SetExplicitMinSize(BSize(160, 20));
	fPassword->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 20));

	BButton* login = new BButton("pwd-login", B_TRANSLATE("登录"),
		new BMessage(kMessagePwdLogin));

	BLayoutBuilder::Group<>(view, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fPhone)
		.AddGroup(B_VERTICAL, 2.0f)
			.Add(new BStringView("password-label", B_TRANSLATE("密码")))
			.Add(new BScrollView("password-scroll", fPassword, 0, false, false))
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.AddGlue()
			.Add(login)
		.End()
		.AddGlue()
		.End();

	return view;
}

void LoginWindow::_SetStatus(const char* text)
{
	if (fStatus != nullptr && text != nullptr)
		fStatus->SetText(text);
}

void LoginWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessageSendCode:
			_SendCaptcha();
			return;

		case kMessageSmsLogin:
			_StartSmsLogin();
			return;

		case kMessagePwdLogin:
			_StartPasswordLogin();
			return;

		case kMessageQrStart:
			_StartQrLogin();
			return;

		case kMessageQrKey:
		{
			const char* key = nullptr;
			if (message->FindString("key", &key) == B_OK
				&& key != nullptr) {
				fQr->SetCode(key);
				_SetStatus(B_TRANSLATE("用网易云音乐 App 扫码"));
			}
			return;
		}

		case kMessageQrState:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				_SetStatus(text);
			return;
		}

		case kMessageOk:
		{
			const char* nickname = nullptr;
			const char* cookie = nullptr;
			if (message->FindString("nickname", &nickname) == B_OK
				&& message->FindString("cookie", &cookie) == B_OK) {
				BMessage forward(kMessageLoginDone);
				forward.AddString("nickname", nickname);
				forward.AddString("cookie", cookie);
				fTarget.SendMessage(&forward);
				PostMessage(B_QUIT_REQUESTED);
			}
			return;
		}

		case kMessageFail:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				_SetStatus(text);
			fBusy = false;
			fQrRunning = false;
			return;
		}

		default:
			break;
	}

	BWindow::MessageReceived(message);
}

bool LoginWindow::QuitRequested()
{
	fQrRunning = false;
	return true;
}

void LoginWindow::_SendCaptcha()
{
	const char* phone = fSmsPhone->Text();
	if (phone == nullptr || phone[0] == '\0') {
		_SetStatus(B_TRANSLATE("先填手机号"));
		return;
	}

	auto* job = new Job();
	job->window = this;
	job->first = phone;

	thread_id thread = spawn_thread(_CaptchaThread, "ncm-captcha",
		B_NORMAL_PRIORITY, job);
	if (thread < B_OK) {
		delete job;
		_SetStatus(B_TRANSLATE("无法创建线程"));
		return;
	}
	resume_thread(thread);
}

int32 LoginWindow::_CaptchaThread(void* data)
{
	auto* job = static_cast<Job*>(data);
	LoginWindow* window = job->window;
	const std::string phone = job->first;
	delete job;

	ncm::NeteaseApi api;
	std::string error;
	const bool ok = api.SendSmsCaptcha(phone, &error);

	BMessage result(ok ? kMessageQrState : kMessageFail);
	if (ok)
		result.AddString("text", B_TRANSLATE("验证码已发送，请查收短信"));
	else
		result.AddString("text", Prefixed(B_TRANSLATE("发送失败: "), error));

	if (window->Lock()) {
		window->PostMessage(&result);
		window->Unlock();
	}
	return 0;
}

void LoginWindow::_StartSmsLogin()
{
	if (fBusy)
		return;

	const char* phone = fSmsPhone->Text();
	const char* code = fCode->Text();
	if (phone == nullptr || phone[0] == '\0' || code == nullptr
		|| code[0] == '\0') {
		_SetStatus(B_TRANSLATE("手机号和验证码都要填"));
		return;
	}

	fBusy = true;
	_SetStatus(B_TRANSLATE("正在登录..."));

	auto* job = new Job();
	job->window = this;
	job->first = phone;
	job->second = code;

	thread_id thread = spawn_thread(_SmsThread, "ncm-sms", B_NORMAL_PRIORITY,
		job);
	if (thread < B_OK) {
		delete job;
		fBusy = false;
		_SetStatus(B_TRANSLATE("无法创建线程"));
		return;
	}
	resume_thread(thread);
}

int32 LoginWindow::_SmsThread(void* data)
{
	auto* job = static_cast<Job*>(data);
	LoginWindow* window = job->window;
	const std::string phone = job->first;
	const std::string code = job->second;
	delete job;

	ncm::NeteaseApi api;
	std::string nickname;
	std::string cookie;
	std::string error;

	const bool ok = api.LoginWithCaptcha(phone, code, &nickname, &cookie,
		&error);

	BMessage result(ok ? kMessageOk : kMessageFail);
	if (ok) {
		result.AddString("nickname", nickname.c_str());
		result.AddString("cookie", cookie.c_str());
	} else {
		result.AddString("text", Prefixed(B_TRANSLATE("登录失败: "), error));
	}

	if (window->Lock()) {
		window->PostMessage(&result);
		window->Unlock();
	}
	return 0;
}

void LoginWindow::_StartPasswordLogin()
{
	if (fBusy)
		return;

	const char* phone = fPhone->Text();
	const char* password = fPassword->Text();
	if (phone == nullptr || phone[0] == '\0' || password == nullptr
		|| password[0] == '\0') {
		_SetStatus(B_TRANSLATE("手机号和密码都要填"));
		return;
	}

	fBusy = true;
	_SetStatus(B_TRANSLATE("正在登录..."));

	auto* job = new Job();
	job->window = this;
	job->first = phone;
	job->second = password;

	thread_id thread = spawn_thread(_PasswordThread, "ncm-pwd",
		B_NORMAL_PRIORITY, job);
	if (thread < B_OK) {
		delete job;
		fBusy = false;
		_SetStatus(B_TRANSLATE("无法创建线程"));
		return;
	}
	resume_thread(thread);
}

int32 LoginWindow::_PasswordThread(void* data)
{
	auto* job = static_cast<Job*>(data);
	LoginWindow* window = job->window;
	const std::string phone = job->first;
	const std::string password = job->second;
	delete job;

	ncm::NeteaseApi api;
	std::string nickname;
	std::string cookie;
	std::string error;

	const bool ok = api.Login(phone, password, &nickname, &cookie, &error);

	BMessage result(ok ? kMessageOk : kMessageFail);
	if (ok) {
		result.AddString("nickname", nickname.c_str());
		result.AddString("cookie", cookie.c_str());
	} else {
		result.AddString("text", Prefixed(B_TRANSLATE("登录失败: "), error));
	}

	if (window->Lock()) {
		window->PostMessage(&result);
		window->Unlock();
	}
	return 0;
}

void LoginWindow::_StartQrLogin()
{
	if (fQrRunning)
		return;

	fQrRunning = true;
	fQr->Clear();
	_SetStatus(B_TRANSLATE("正在获取二维码..."));

	thread_id thread = spawn_thread(_QrThread, "ncm-qr", B_NORMAL_PRIORITY,
		this);
	if (thread < B_OK) {
		fQrRunning = false;
		_SetStatus(B_TRANSLATE("无法创建线程"));
		return;
	}
	resume_thread(thread);
}

int32 LoginWindow::_QrThread(void* data)
{
	auto* window = static_cast<LoginWindow*>(data);

	ncm::NeteaseApi api;
	std::string key;
	std::string error;
	if (!api.QrKey(&key, &error)) {
		BMessage failure(kMessageFail);
		failure.AddString("text", Prefixed(B_TRANSLATE("取二维码失败: "), error));
		if (window->Lock()) {
			window->PostMessage(&failure);
			window->Unlock();
		}
		return 0;
	}

	{
		BMessage ready(kMessageQrKey);
		BString payload("https://music.163.com/login?codekey=");
		payload << key.c_str();
		ready.AddString("key", payload.String());
		if (window->Lock()) {
			window->PostMessage(&ready);
			window->Unlock();
		}
	}

	while (window->fQrRunning) {
		int code = 0;
		std::string nickname;
		std::string cookie;
		if (!api.QrPoll(key, &code, &nickname, &cookie, &error)) {
			BMessage failure(kMessageFail);
			failure.AddString("text", Prefixed(B_TRANSLATE("轮询失败: "), error));
			if (window->Lock()) {
				window->PostMessage(&failure);
				window->Unlock();
			}
			return 0;
		}

		if (code == 803) {
			BMessage done(kMessageOk);
			done.AddString("nickname", nickname.c_str());
			done.AddString("cookie", cookie.c_str());
			if (window->Lock()) {
				window->PostMessage(&done);
				window->Unlock();
			}
			return 0;
		}

		if (code == 800) {
			BMessage expired(kMessageFail);
			expired.AddString("text", B_TRANSLATE("二维码已过期，请刷新"));
			if (window->Lock()) {
				window->PostMessage(&expired);
				window->Unlock();
			}
			return 0;
		}

		BMessage state(kMessageQrState);
		state.AddString("text", code == 802
				? B_TRANSLATE("已扫描，请在手机上确认")
				: B_TRANSLATE("等待扫码..."));
		if (window->Lock()) {
			window->PostMessage(&state);
			window->Unlock();
		}

		snooze(2000000);
	}
	return 0;
}
