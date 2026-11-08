#ifndef NCM_TRANSPORT_BUTTON_H
#define NCM_TRANSPORT_BUTTON_H

#include <ControlLook.h>
#include <Control.h>
#include <Shape.h>

// 对齐 Haiku MediaPlayer 的传输按钮（SymbolButton，MIT，Stephan Aßmus）：
// 图标是 BShape 矢量路径，按钮底用 be_control_look 画原生斜面，
// 字形用线性渐变（暗 -> 基色）填充，颜色跟随系统主题。
class TransportButton : public BControl {
public:
	enum Kind {
		kPrevious,
		kPlay,
		kPause,
		kNext,
		kHeart,
		kHeartFilled,
	};

	TransportButton(const char* name, Kind kind, const char* tooltip,
		BMessage* message, uint32 borders = 0);
	~TransportButton() override;

	void SetKind(Kind kind);

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void MouseUp(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit,
		const BMessage* dragMessage) override;

private:
	BShape* _SkipBackwardsShape(float height) const;
	BShape* _SkipForwardShape(float height) const;
	BShape* _PlayShape(float height) const;
	BShape* _PauseShape(float height) const;
	BShape* _HeartShape(float height) const;

	void _DrawGlyph(BRect bounds, const rgb_color& base);

	Kind fKind;
	BShape* fShape = nullptr;
	BShape* fAltShape = nullptr; // play/pause 双形态
	uint32 fBorders;
	bool fPressed = false;
	bool fHover = false;
};

#endif
