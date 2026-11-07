#ifndef NCM_LIST_METRICS_H
#define NCM_LIST_METRICS_H

#include <InterfaceDefs.h>

namespace ncm {
namespace metrics {

const float kRowHeight = 62.0f;
const float kHeaderHeight = 28.0f;
const float kCoverSize = 44.0f;

const float kIndexRight = 42.0f;
const float kCoverX = 56.0f;
const float kTextX = kCoverX + kCoverSize + 12.0f;

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

inline rgb_color SelectionColor()
{
	return (rgb_color){ 232, 240, 254, 255 };
}

inline rgb_color PrimaryText()
{
	return (rgb_color){ 34, 34, 34, 255 };
}

inline rgb_color SecondaryText()
{
	return (rgb_color){ 138, 138, 138, 255 };
}

inline rgb_color Accent()
{
	return (rgb_color){ 214, 51, 60, 255 };
}

inline rgb_color RowBackground()
{
	return (rgb_color){ 255, 255, 255, 255 };
}

} // namespace metrics
} // namespace ncm

#endif
