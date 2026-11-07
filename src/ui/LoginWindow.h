#ifndef NCM_LOGINWINDOW_H
#define NCM_LOGINWINDOW_H

#include <Messenger.h>
#include <Window.h>

class BButton;
class BStringView;
class BTextControl;
class BTextView;

enum {
	kMessageLoginDone = 'lgdn',
};

// 手机号 + 密码登录。成功后把 Cookie 和昵称发回主窗口。
class LoginWindow : public BWindow {
public:
	explicit LoginWindow(BMessenger target);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void _StartLogin();
	static int32 _LoginThread(void* data);

	BMessenger fTarget;
	BTextControl* fPhone = nullptr;
	BTextView* fPassword = nullptr;
	BButton* fLoginButton = nullptr;
	BStringView* fStatus = nullptr;
	volatile bool fBusy = false;
};

#endif
