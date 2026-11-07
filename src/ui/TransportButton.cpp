#include "TransportButton.h"

#include "ListMetrics.h"

#include <Window.h>

namespace {

const float kWidth = 36.0f;
const float kHeight = 28.0f;

} // namespace

TransportButton::TransportButton(const char* name, Kind kind,
	const char* tooltip, BMessage* message)
	: BControl(name, nullptr, message, B_WILL_DRAW)
	, fKind(kind)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
	SetExplicitMinSize(BSize(kWidth, kHeight));
	SetExplicitMaxSize(BSize(kWidth, kHeight));
	if (tooltip != nullptr)
		SetToolTip(tooltip);
}

void TransportButton::SetKind(Kind kind)
{
	if (fKind == kind)
		return;
	fKind = kind;
	Invalidate();
}

void TransportButton::GetPreferredSize(float* width, float* height)
{
	*width = kWidth;
	*height = kHeight;
}

void TransportButton::Draw(BRect)
{
	const BRect bounds = Bounds();
	const rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);

	rgb_color surface = base;
	if (fPressed)
		surface = tint_color(base, B_DARKEN_2_TINT);
	else if (fHover)
		surface = tint_color(base, B_DARKEN_1_TINT);

	SetHighColor(surface);
	FillRoundRect(bounds, 3.0f, 3.0f);

	BRect frame(bounds);
	frame.InsetBy(0.5f, 0.5f);
	SetHighColor(tint_color(base, B_DARKEN_2_TINT));
	StrokeRoundRect(frame, 3.0f, 3.0f);

	rgb_color glyph = IsEnabled() ? ui_color(B_CONTROL_TEXT_COLOR)
								  : tint_color(ui_color(B_CONTROL_TEXT_COLOR),
										B_LIGHTEN_2_TINT);
	if (fPressed)
		glyph = tint_color(glyph, B_DARKEN_1_TINT);

	_DrawGlyph(bounds, glyph);
}

void TransportButton::_DrawGlyph(BRect bounds, rgb_color color)
{
	const float centerX = (bounds.left + bounds.right) / 2.0f;
	const float centerY = (bounds.top + bounds.bottom) / 2.0f;
	const float halfHeight = 6.0f;
	const float halfWidth = 5.0f;

	SetHighColor(color);

	switch (fKind) {
		case kPlay:
		{
			BPoint points[3] = {
				BPoint(centerX - halfWidth + 1.0f, centerY - halfHeight),
				BPoint(centerX - halfWidth + 1.0f, centerY + halfHeight),
				BPoint(centerX + halfWidth + 1.0f, centerY),
			};
			FillPolygon(points, 3);
			break;
		}

		case kPause:
			FillRect(BRect(centerX - 4.0f, centerY - halfHeight,
				centerX - 1.5f, centerY + halfHeight));
			FillRect(BRect(centerX + 1.5f, centerY - halfHeight,
				centerX + 4.0f, centerY + halfHeight));
			break;

		case kPrevious:
		{
			FillRect(BRect(centerX - 6.0f, centerY - halfHeight,
				centerX - 4.0f, centerY + halfHeight));
			BPoint points[3] = {
				BPoint(centerX + 5.0f, centerY - halfHeight),
				BPoint(centerX + 5.0f, centerY + halfHeight),
				BPoint(centerX - 3.5f, centerY),
			};
			FillPolygon(points, 3);
			break;
		}

		case kNext:
		{
			BPoint points[3] = {
				BPoint(centerX - 5.0f, centerY - halfHeight),
				BPoint(centerX - 5.0f, centerY + halfHeight),
				BPoint(centerX + 3.5f, centerY),
			};
			FillPolygon(points, 3);
			FillRect(BRect(centerX + 4.0f, centerY - halfHeight,
				centerX + 6.0f, centerY + halfHeight));
			break;
		}
	}
}

void TransportButton::MouseDown(BPoint where)
{
	if (!IsEnabled())
		return;

	fPressed = true;
	Invalidate();

	BPoint cursor = where;
	uint32 buttons = 0;
	do {
		snooze(15000);
		if (Window() == nullptr)
			return;
		GetMouse(&cursor, &buttons);
	} while (buttons != 0);

	fPressed = false;
	Invalidate();

	if (Bounds().Contains(cursor))
		Invoke();
}

void TransportButton::MouseMoved(BPoint where, uint32 transit,
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
