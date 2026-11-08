#include "SeekSlider.h"

#include <OS.h>

SeekSlider::SeekSlider(const char* name, BMessage* message, int32 minValue,
	int32 maxValue)
	: BSlider(name, nullptr, message, minValue, maxValue, B_HORIZONTAL,
		  B_TRIANGLE_THUMB)
{
	// 官方同款：填充色比轨道深三档
	const rgb_color fill = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_DARKEN_3_TINT);
	UseFillColor(true, &fill);
}

void SeekSlider::MouseDown(BPoint where)
{
	fTracking = true;
	BSlider::MouseDown(where);
}

void SeekSlider::MouseUp(BPoint where)
{
	fTracking = false;
	fLastTrackTime = system_time();
	BSlider::MouseUp(where);
}

bool SeekSlider::IsTracking() const
{
	return fTracking || system_time() - fLastTrackTime < 250000;
}
