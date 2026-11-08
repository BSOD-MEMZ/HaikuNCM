#include "TransportButton.h"

#include <ControlLook.h>
#include <GradientLinear.h>
#include <LayoutUtils.h>
#include <Font.h>
#include <Window.h>

#include <cmath>

namespace {

float SymbolHeight()
{
	// 官方 TransportControlGroup 的取法：字号 / 1.33，且压成奇数
	// 让尖角形状的中点落在像素上
	return float(int(be_plain_font->Size() / 1.33) | 1);
}

} // namespace

TransportButton::TransportButton(const char* name, Kind kind,
	const char* tooltip, BMessage* message, uint32 borders)
	: BControl(name, nullptr, message, B_WILL_DRAW)
	, fKind(kind)
	, fBorders(borders)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(B_PANEL_BACKGROUND_COLOR);

	const float height = SymbolHeight();
	switch (fKind) {
		case kPrevious:
			fShape = _SkipBackwardsShape(height);
			break;
		case kNext:
			fShape = _SkipForwardShape(height);
			break;
		case kPlay:
			fShape = _PlayShape(height);
			break;
		case kPause:
			fShape = _PauseShape(height);
			break;
		default:
			fShape = _HeartShape(height);
			break;
	}

	// kPlay 需要带一个 pause 备用形态
	if (fKind == kPlay)
		fAltShape = _PauseShape(height);

	// BControl 没有字形就没尺寸，布局会把按钮压成 0 宽，必须显式给
	SetExplicitMinSize(BSize(34, 30));
	SetExplicitMaxSize(BSize(34, 30));

	if (tooltip != nullptr)
		SetToolTip(tooltip);
}

TransportButton::~TransportButton()
{
	delete fShape;
	delete fAltShape;
}

void TransportButton::SetKind(Kind kind)
{
	if (fKind == kind)
		return;

	// play <-> pause 切换：交换两个字形指针（移动会把备用形清空，
	// 第二次切换时 fShape 被赋成 null，字形就没了）
	if ((fKind == kPlay && kind == kPause)
		|| (fKind == kPause && kind == kPlay)) {
		BShape* swap = fShape;
		fShape = fAltShape;
		fAltShape = swap;
		fKind = kind;
		fActive = kind == kPause;
		Invalidate();
		return;
	}

	fKind = kind;
	Invalidate();
}

void TransportButton::Draw(BRect updateRect)
{
	BRect bounds(Bounds());

	// 官方 SymbolButton：底面交给 ControlLook，颜色跟随主题
	uint32 flags = be_control_look->Flags(this);
	rgb_color base = LowColor();
	if (fBorders != 0) {
		// hover 时底面提亮一档，鼠标扫过能看出来
		rgb_color face = fHover ? tint_color(base, B_LIGHTEN_1_TINT) : base;
		be_control_look->DrawButtonFrame(this, bounds, updateRect, face, face,
			flags, fBorders);
		be_control_look->DrawButtonBackground(this, bounds, updateRect, face,
			flags);
	}

	_DrawGlyph(bounds, base);
}

void TransportButton::_DrawGlyph(BRect bounds, const rgb_color& base)
{
	if (fShape == nullptr)
		return;

	// 官方同款配色逻辑：可用时深四档，按下更深；
	// 悬停或正在播放时字形改成绿色渐变（MediaPlayer 的那个绿）
	rgb_color symbol = IsEnabled()
		? tint_color(base, B_DARKEN_4_TINT)
		: tint_color(base, B_DARKEN_2_TINT);
	if (fPressed)
		symbol = tint_color(symbol, B_DARKEN_1_TINT);

	const bool green = fHover || fActive;

	// 字形居中
	BPoint offset;
	offset.x = (bounds.left + bounds.right) / 2.0f;
	offset.y = (bounds.top + bounds.bottom) / 2.0f;
	offset.x -= fShape->Bounds().Width() / 2.0f;
	offset.y -= fShape->Bounds().Height() / 2.0f;
	offset.x = floorf(offset.x - fShape->Bounds().left);
	offset.y = ceilf(offset.y - fShape->Bounds().top);

	MovePenTo(offset);

	// 官方同款：字形用垂直线性渐变（暗 -> 基色），立体感来自这里
	BGradientLinear gradient;
	if (green) {
		gradient.AddColor((rgb_color){ 46, 122, 26, 255 }, 0);
		gradient.AddColor((rgb_color){ 146, 214, 88, 255 }, 255);
	} else {
		gradient.AddColor(tint_color(symbol, B_DARKEN_1_TINT), 0);
		gradient.AddColor(symbol, 255);
	}
	gradient.SetStart(offset);
	BPoint end(offset);
	end.y += fShape->Bounds().Height();
	gradient.SetEnd(end);

	FillShape(fShape, gradient);
}

void TransportButton::MouseDown(BPoint where)
{
	(void)where;

	if (!IsEnabled())
		return;

	fPressed = true;
	Invalidate();
	SetMouseEventMask(B_POINTER_EVENTS);
}

void TransportButton::MouseUp(BPoint where)
{
	const bool wasPressed = fPressed;
	fPressed = false;
	Invalidate();

	if (wasPressed && Bounds().Contains(where))
		Invoke();
}

void TransportButton::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	(void)transit;
	(void)dragMessage;

	if (!fPressed) {
		const bool hover = Bounds().Contains(where);
		if (hover != fHover) {
			fHover = hover;
			Invalidate();
		}
		return;
	}

	// 按住时移出范围则取消按下态（原生按钮语义）
	const bool inside = Bounds().Contains(where);
	if (inside != fPressed) {
		fPressed = inside;
		Invalidate();
	}
}

// #pragma mark - 官方 TransportControlGroup 的字形路径（MIT）

BShape* TransportButton::_SkipBackwardsShape(float height) const
{
	BShape* shape = new BShape();

	const float stopWidth = ceilf(height / 6);

	shape->MoveTo(BPoint(-stopWidth, height));
	shape->LineTo(BPoint(0, height));
	shape->LineTo(BPoint(0, 0));
	shape->LineTo(BPoint(-stopWidth, 0));
	shape->Close();

	shape->MoveTo(BPoint(0, height / 2));
	shape->LineTo(BPoint(height, height));
	shape->LineTo(BPoint(height, 0));
	shape->Close();

	shape->MoveTo(BPoint(height, height / 2));
	shape->LineTo(BPoint(height * 2, height));
	shape->LineTo(BPoint(height * 2, 0));
	shape->Close();

	return shape;
}

BShape* TransportButton::_SkipForwardShape(float height) const
{
	BShape* shape = new BShape();

	shape->MoveTo(BPoint(height, height / 2));
	shape->LineTo(BPoint(0, height));
	shape->LineTo(BPoint(0, 0));
	shape->Close();

	shape->MoveTo(BPoint(height * 2, height / 2));
	shape->LineTo(BPoint(height, height));
	shape->LineTo(BPoint(height, 0));
	shape->Close();

	const float stopWidth = ceilf(height / 6);

	shape->MoveTo(BPoint(height * 2, height));
	shape->LineTo(BPoint(height * 2 + stopWidth, height));
	shape->LineTo(BPoint(height * 2 + stopWidth, 0));
	shape->LineTo(BPoint(height * 2, 0));
	shape->Close();

	return shape;
}

BShape* TransportButton::_PlayShape(float height) const
{
	BShape* shape = new BShape();

	const float step = floorf(height / 8);

	shape->MoveTo(BPoint(height + step, height / 2));
	shape->LineTo(BPoint(-step, height + step));
	shape->LineTo(BPoint(-step, 0 - step));
	shape->Close();

	return shape;
}

BShape* TransportButton::_PauseShape(float height) const
{
	BShape* shape = new BShape();

	const float stemWidth = floorf(height / 3);

	shape->MoveTo(BPoint(0, height));
	shape->LineTo(BPoint(stemWidth, height));
	shape->LineTo(BPoint(stemWidth, 0));
	shape->LineTo(BPoint(0, 0));
	shape->Close();

	shape->MoveTo(BPoint(height - stemWidth, height));
	shape->LineTo(BPoint(height, height));
	shape->LineTo(BPoint(height, 0));
	shape->LineTo(BPoint(height - stemWidth, 0));
	shape->Close();

	return shape;
}

// 爱心没有官方字形，用同风格贝塞尔自绘（实心/空心同一路径）
BShape* TransportButton::_HeartShape(float height) const
{
	BShape* shape = new BShape();

	const float cx = height;      // 字形宽约 2*height，中点
	const float cy = height / 2.0f;
	const float w = height * 0.85f;
	const float h = height * 0.75f;

	shape->MoveTo(BPoint(cx, cy + h));
	shape->BezierTo(BPoint(cx - w, cy + h * 0.15f),
		BPoint(cx - w, cy - h), BPoint(cx - w * 0.45f, cy - h));
	shape->BezierTo(BPoint(cx - w * 0.15f, cy - h),
		BPoint(cx, cy - h * 0.55f), BPoint(cx, cy - h * 0.3f));
	shape->BezierTo(BPoint(cx, cy - h * 0.55f),
		BPoint(cx + w * 0.15f, cy - h), BPoint(cx + w * 0.45f, cy - h));
	shape->BezierTo(BPoint(cx + w, cy - h),
		BPoint(cx + w, cy + h * 0.15f), BPoint(cx, cy + h));
	shape->Close();

	return shape;
}
