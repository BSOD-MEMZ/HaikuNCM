#include "UserMenuItem.h"

#include <Font.h>
#include <InterfaceDefs.h>
#include <OS.h>
#include <Window.h>

#include <algorithm>

UserMenuItem::UserMenuItem(BMessage* message)
	: BControl("user-item", nullptr, message, B_WILL_DRAW)
{
	SetViewUIColor(B_MENU_BACKGROUND_COLOR);
	SetLowUIColor(B_MENU_BACKGROUND_COLOR);
	SetFont(be_plain_font);
	SetLabel("");
}

void UserMenuItem::SetLabel(const char* text)
{
	fText = text != nullptr ? text : "";
	InvalidateLayout();
	Invalidate();
}

const char* UserMenuItem::Label() const
{
	return fText.String();
}

void UserMenuItem::GetPreferredSize(float* width, float* height)
{
	*width = StringWidth(fText.String()) + 18.0f;
	*height = be_plain_font->Size() + 10.0f;
}

void UserMenuItem::Draw(BRect)
{
	SetHighColor(ui_color(B_MENU_BACKGROUND_COLOR));
	FillRect(Bounds());

	if (fHover) {
		SetHighColor(tint_color(ui_color(B_MENU_BACKGROUND_COLOR),
			B_DARKEN_1_TINT));
		BRect frame(Bounds());
		frame.InsetBy(1.0f, 2.0f);
		FillRoundRect(frame, 3.0f, 3.0f);
	}

	SetHighColor(IsEnabled() ? ui_color(B_MENU_ITEM_TEXT_COLOR)
							 : tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
								   B_LIGHTEN_2_TINT));
	const float textWidth = StringWidth(fText.String());
	const float baseline = (Bounds().top + Bounds().bottom) / 2.0f
		+ be_plain_font->Size() * 0.36f;
	MovePenTo(BPoint(Bounds().right - textWidth - 8.0f, baseline));
	DrawString(fText.String());
}

void UserMenuItem::MouseDown(BPoint where)
{
	(void)where;

	if (!IsEnabled())
		return;

	BPoint cursor;
	uint32 buttons = 0;
	GetMouse(&cursor, &buttons);
	snooze(10000);
	Invoke();
}

void UserMenuItem::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	(void)transit;
	(void)dragMessage;

	const bool hover = Bounds().Contains(where);
	if (hover != fHover) {
		fHover = hover;
		Invalidate();
	}
}
