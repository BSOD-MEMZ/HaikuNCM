#ifndef NCM_TRANSPORT_BUTTON_H
#define NCM_TRANSPORT_BUTTON_H

#include <Control.h>
#include <String.h>

// MediaPlayer 的传输按钮是自绘的（它用编译进二进制的 HVIF 图标）。
// 这里同样自绘，只是直接画矢量形状：不依赖图标资源，也不受 BButton::SetIcon
// 在 beta6 上不显示的问题影响，颜色还能跟着系统主题走。
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
		BMessage* message);

	void SetKind(Kind kind);

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage) override;
	void GetPreferredSize(float* width, float* height) override;

private:
	void _DrawGlyph(BRect bounds, rgb_color color);

	Kind fKind;
	bool fPressed = false;
	bool fHover = false;
};

#endif
