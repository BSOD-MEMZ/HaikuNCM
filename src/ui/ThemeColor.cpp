#include "ThemeColor.h"

#include <View.h>

#include <algorithm>
#include <vector>

namespace {

float Clamp(float value)
{
	if (value < 0.0f)
		return 0.0f;
	if (value > 255.0f)
		return 255.0f;
	return value;
}

} // namespace

rgb_color BlendColor(rgb_color base, rgb_color accent, float accentRatio)
{
	if (accentRatio < 0.0f)
		accentRatio = 0.0f;
	if (accentRatio > 1.0f)
		accentRatio = 1.0f;
	const float baseRatio = 1.0f - accentRatio;

	rgb_color result;
	result.red = static_cast<uint8>(Clamp(base.red * baseRatio
		+ accent.red * accentRatio));
	result.green = static_cast<uint8>(Clamp(base.green * baseRatio
		+ accent.green * accentRatio));
	result.blue = static_cast<uint8>(Clamp(base.blue * baseRatio
		+ accent.blue * accentRatio));
	result.alpha = 255;
	return result;
}

rgb_color ThemeColorFromCover(const BBitmap* cover)
{
	const rgb_color panel = ui_color(B_PANEL_BACKGROUND_COLOR);
	if (cover == nullptr || !cover->IsValid())
		return panel;

	const color_space space = cover->ColorSpace();
	if (space != B_RGB32 && space != B_RGBA32 && space != B_RGB24)
		return panel;

	const BRect bounds = cover->Bounds();
	const int32 width = static_cast<int32>(bounds.Width()) + 1;
	const int32 height = static_cast<int32>(bounds.Height()) + 1;
	if (width <= 0 || height <= 0)
		return panel;

	const uchar* pixels = static_cast<const uchar*>(cover->Bits());
	const int32 rowBytes = cover->BytesPerRow();
	const int32 pixelSize = space == B_RGB24 ? 3 : 4;

	// 隔行采样就够了，反正只是要个平均色
	const int32 step = std::max(1, std::min(width, height) / 24);
	double red = 0.0;
	double green = 0.0;
	double blue = 0.0;
	int32 count = 0;

	for (int32 y = 0; y < height; y += step) {
		const uchar* row = pixels + y * rowBytes;
		for (int32 x = 0; x < width; x += step) {
			const uchar* pixel = row + x * pixelSize;
			blue += pixel[0];  // 小端下 B_RGB32 内存顺序是 B,G,R,A
			green += pixel[1];
			red += pixel[2];
			count++;
		}
	}

	if (count == 0)
		return panel;

	const rgb_color average = {
		static_cast<uint8>(red / count),
		static_cast<uint8>(green / count),
		static_cast<uint8>(blue / count),
		255
	};

	return BlendColor(panel, average, 0.3f);
}
