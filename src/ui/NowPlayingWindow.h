#ifndef NCM_NOWPLAYING_WINDOW_H
#define NCM_NOWPLAYING_WINDOW_H

#include <Bitmap.h>
#include <Control.h>
#include <Messenger.h>
#include <View.h>
#include <Window.h>

#include <String.h>
#include <vector>

class BStringView;
class BScrollView;

enum {
	kMessageNowPlayingReady = 'nprd', // title/artist/coverPath/lrcPath
	kMessageNowPlayingTick = 'nptk',  // position (double)
	kMessageNowPlayingClosed = 'npcl',
};

// LRC 歌词视图：解析 [mm:ss.xx] 行，当前位置行高亮并滚动
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
	float fScrollOffset = 0.0f;
};

// 底部左侧徽章：小封面 + 曲名/歌手，点击打开播放页
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

// 播放页窗口：大封面 + 曲目信息 + 滚动歌词（官方播放器同布局）
class NowPlayingWindow : public BWindow {
public:
	explicit NowPlayingWindow(BMessenger target);

	void SetNowPlaying(const BString& title, const BString& artist,
		const BString& coverPath, const BString& lrcPath);
	void SetPosition(double seconds);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	BMessenger fTarget;
	LrcView* fLrc = nullptr;
	BView* fCoverView = nullptr;
	BBitmap* fCover = nullptr;
	BStringView* fTitle = nullptr;
	BStringView* fArtist = nullptr;
	BString fCoverPath;
	BString fPendingLrc;
};

#endif
