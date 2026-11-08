#ifndef NCM_THEMECOLOR_H
#define NCM_THEMECOLOR_H

#include <Bitmap.h>
#include <GraphicsDefs.h>

// 从封面取一个主题色：整张图求平均（按块采样），再和面板背景色按 30% 混，
// 这样整块背景既是"这张专辑的颜色"，又不会花到看不清字。
rgb_color BlendColor(rgb_color base, rgb_color accent, float accentRatio);
rgb_color ThemeColorFromCover(const BBitmap* cover);

#endif
