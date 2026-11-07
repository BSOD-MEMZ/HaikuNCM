#ifndef NCM_USER_MENU_ITEM_H
#define NCM_USER_MENU_ITEM_H

#include <Control.h>
#include <String.h>

// 菜单栏右侧的登录状态：像 HaikuDepot 那样是一段可点的文字，不是按钮。
// 点击后弹出菜单（登录… / 退出登录）。
class UserMenuItem : public BControl {
public:
	explicit UserMenuItem(BMessage* message);

	void SetLabel(const char* text);
	const char* Label() const;

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage) override;
	void GetPreferredSize(float* width, float* height) override;

private:
	BString fText;
	bool fHover = false;
};

#endif
