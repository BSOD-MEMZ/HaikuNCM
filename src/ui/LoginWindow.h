#ifndef NCM_LOGINWINDOW_H
#define NCM_LOGINWINDOW_H

#include <Messenger.h>
#include <View.h>
#include <Window.h>

#include <string>
#include <vector>

class BButton;
class BStringView;
class BTextControl;
class BTextView;

enum {
	kMessageLoginDone = 'lgdn',
};

// 二维码显示区：把 qrcodegen 的模块矩阵画成方块
class QrView : public BView {
public:
	QrView();

	void SetCode(const char* text);
	void Clear();

	void Draw(BRect updateRect) override;
	void GetPreferredSize(float* width, float* height) override;

private:
	int fSize = 0;
	std::vector<bool> fModules;
};

// 三种登录方式：手机验证码 / 扫码 / 密码。
// 网易云风控很严，账密经常被直接拒，验证码和扫码才是主力。
class LoginWindow : public BWindow {
public:
	explicit LoginWindow(BMessenger target);
	~LoginWindow() override;

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	BView* _BuildSmsTab();
	BView* _BuildQrTab();
	BView* _BuildPasswordTab();

	void _SendCaptcha();
	void _StartSmsLogin();
	void _StartQrLogin();
	void _StartPasswordLogin();
	void _SetStatus(const char* text);

	static int32 _CaptchaThread(void* data);
	static int32 _SmsThread(void* data);
	static int32 _PasswordThread(void* data);
	static int32 _QrThread(void* data);

	BMessenger fTarget;
	BStringView* fStatus = nullptr;

	BTextControl* fSmsPhone = nullptr;
	BTextControl* fCode = nullptr;
	BButton* fCodeButton = nullptr;

	QrView* fQr = nullptr;
	BStringView* fQrStatus = nullptr;

	BTextControl* fPhone = nullptr;
	BTextView* fPassword = nullptr;

	volatile bool fBusy = false;
	volatile bool fQrRunning = false;
};

#endif
