#ifndef NCM_LIST_METRICS_H
#define NCM_LIST_METRICS_H

#include <Font.h>
#include <InterfaceDefs.h>

namespace ncm {
namespace metrics {

// 一切尺寸和颜色都从系统取值：字号变了行高跟着变，配色跟着主题走。
inline float RowHeight()
{
	return be_plain_font->Size() * 2.0f + 30.0f;
}

inline float HeaderHeight()
{
	return be_plain_font->Size() + 14.0f;
}

inline float CoverSize()
{
	return RowHeight() - 16.0f;
}

inline float IndexRight()
{
	return be_plain_font->Size() * 2.6f;
}

inline float CoverX()
{
	return IndexRight() + 14.0f;
}

inline float TextX()
{
	return CoverX() + CoverSize() + 10.0f;
}

inline float AlbumX(float width)
{
	return width * 0.56f;
}

inline float HeartX(float width)
{
	return width - 132.0f;
}

inline float DurationRight(float width)
{
	return width - 20.0f;
}

// 原生控件用的系统颜色，不自己发明配色
inline rgb_color Background()
{
	return ui_color(B_LIST_BACKGROUND_COLOR);
}

inline rgb_color SelectionBackground()
{
	return ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
}

inline rgb_color Text()
{
	return ui_color(B_LIST_ITEM_TEXT_COLOR);
}

inline rgb_color SelectedText()
{
	return ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);
}

// 次要信息：用同一个文字色往背景方向淡化，暗色主题下也成立
inline rgb_color SecondaryText()
{
	return tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR), B_LIGHTEN_1_TINT);
}

inline rgb_color Accent()
{
	return ui_color(B_FAILURE_COLOR);
}

} // namespace metrics
} // namespace ncm

#endif
