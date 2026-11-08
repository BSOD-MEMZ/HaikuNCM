#ifndef NCM_NOWPLAYING_PAGE_H
#define NCM_NOWPLAYING_PAGE_H

#include <Bitmap.h>
#include <Control.h>
#include <String.h>
#include <View.h>

#include <vector>

class BStringView;
class CoverView;

enum {
	kMessageNowPlayingReady = 'nprd', // title / artist / cover / lrc
	kMessageNowPlayingClose = 'npcx', // 播放页的返回按钮
};

// LRC 歌词视图：解析 [mm:ss.xx] 行，当前行居中高亮
class LrcView : public BView {
public:
	LrcView();

	void SetLyric(const BString& lrc);
	void SetPosition(double seconds);

	void Draw(BRect updateRect) override;
	void FrameResized(float width, float height) override;

private:
	struct Line {
		double time;
		BString text;
	};
	std::vector<Line> fLines;
	int32 fCurrent = -1;
};

// 底部徽章：小封面 + 曲名/歌手，点击在列表/播放页之间切换
class NowPlayingBadge : public BControl {
public:
	NowPlayingBadge();

	void SetInfo(const BString& title, const BString& artist,
		BBitmap* cover);
	void SetCover(BBitmap* cover); // 接管所有权

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void GetPreferredSize(float* width, float* height) override;

private:
	BString fTitle;
	BString fArtist;
	BBitmap* fCover = nullptr;
};

// 播放页：嵌在主窗口内容区（把歌曲列表换掉），底部播放控件始终保留
class NowPlayingView : public BView {
public:
	NowPlayingView();

	void SetNowPlaying(const BString& title, const BString& artist,
		const BString& coverPath, const BString& lrcPath);
	void SetPosition(double seconds);

private:
	CoverView* fCover = nullptr;
	BStringView* fTitle = nullptr;
	BStringView* fArtist = nullptr;
	LrcView* fLrc = nullptr;
};

#endif
