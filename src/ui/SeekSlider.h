#ifndef NCM_SEEK_SLIDER_H
#define NCM_SEEK_SLIDER_H

#include <Slider.h>

// 语义照抄 Haiku MediaPlayer 的 SeekSlider（MIT，Stephan Aßmus）：
// 拖动期间 IsTracking() 为真，外部刷新进度时要让位；松手后还有 250ms
// 宽限期，避免刚松手就被 ticker 抢回去。
class SeekSlider : public BSlider {
public:
	SeekSlider(const char* name, BMessage* message, int32 minValue,
		int32 maxValue);

	void MouseDown(BPoint where) override;
	void MouseUp(BPoint where) override;
	bool IsTracking() const;

private:
	bool fTracking = false;
	bigtime_t fLastTrackTime = 0;
};

#endif
