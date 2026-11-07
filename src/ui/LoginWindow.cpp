#include "LoginWindow.h"

#include "net/netease.h"

#include <Catalog.h>

#include <Button.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "LoginWindow"

namespace {

BString Prefixed(const char* prefix, const std::string& detail)
{
	BString text(prefix);
	text << detail.c_str();
	return text;
}

} // namespace

LoginWindow::LoginWindow(BMessenger target)
	: BWindow(BRect(0, 0, 360, 210), B_TRANSLATE("登录网易云音乐"), B_TITLED_WINDOW,
		  B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
	, fTarget(target)
{
	fPhone = new BTextControl("phone", B_TRANSLATE("手机号"), "", nullptr);
	fPhone->SetModificationMessage(nullptr);

	fPassword = new BTextView("password");
	fPassword->HideTyping(true);
	fPassword->SetExplicitMinSize(BSize(180, 20));
	fPassword->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 20));

	fLoginButton = new BButton("login", B_TRANSLATE("登录"), new BMessage('doit'));
	fLoginButton->MakeDefault(true);

	BButton* cancel = new BButton("cancel", B_TRANSLATE("取消"),
		new BMessage(B_QUIT_REQUESTED));

	fStatus = new BStringView("status", B_TRANSLATE("密码会直接发给网易云，不经过任何中间服务器"));

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fPhone)
		.AddGroup(B_VERTICAL, 2.0f)
			.Add(new BStringView("password-label", B_TRANSLATE("密码")))
			.Add(new BScrollView("password-scroll", fPassword, 0, false, false))
		.End()
		.Add(fStatus)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.AddGlue()
			.Add(cancel)
			.Add(fLoginButton)
		.End();

	CenterOnScreen();
	fPhone->MakeFocus(true);
}

void LoginWindow::MessageReceived(BMessage* message)
{
	if (message->what == 'doit') {
		_StartLogin();
		return;
	}

	if (message->what == kMessageLoginDone) {
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

	if (message->what == 'fail') {
		const char* text = nullptr;
		if (message->FindString("text", &text) == B_OK)
			fStatus->SetText(text);
		fBusy = false;
		fLoginButton->SetEnabled(true);
		return;
	}

	BWindow::MessageReceived(message);
}

bool LoginWindow::QuitRequested()
{
	return true;
}

void LoginWindow::_StartLogin()
{
	if (fBusy)
		return;

	const char* phone = fPhone->Text();
	const char* password = fPassword->Text();

	if (phone == nullptr || phone[0] == '\0' || password == nullptr
		|| password[0] == '\0') {
		fStatus->SetText(B_TRANSLATE("手机号和密码都要填"));
		return;
	}

	fBusy = true;
	fLoginButton->SetEnabled(false);
	fStatus->SetText(B_TRANSLATE("正在登录…"));

	auto* payload = new std::pair<LoginWindow*, BString>(this, BString(phone));
	payload->second << "\n" << password;

	thread_id thread = spawn_thread(_LoginThread, "ncm-login",
		B_NORMAL_PRIORITY, payload);
	if (thread < B_OK) {
		delete payload;
		fBusy = false;
		fLoginButton->SetEnabled(true);
		fStatus->SetText(B_TRANSLATE("无法创建登录线程"));
		return;
	}
	resume_thread(thread);
}

int32 LoginWindow::_LoginThread(void* data)
{
	auto* payload = static_cast<std::pair<LoginWindow*, BString>*>(data);
	LoginWindow* window = payload->first;
	BString credentials = payload->second;
	delete payload;

	const std::string combined(credentials.String());
	const size_t split = combined.find('\n');
	const std::string phone = combined.substr(0, split);
	const std::string password = split == std::string::npos
		? std::string()
		: combined.substr(split + 1);

	ncm::NeteaseApi api;
	std::string nickname;
	std::string cookie;
	std::string error;

	const bool ok = api.Login(phone, password, &nickname,
		&cookie, &error);

	BMessage result(ok ? kMessageLoginDone : 'fail');
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
