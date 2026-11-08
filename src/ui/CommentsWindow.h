#ifndef NCM_COMMENTS_WINDOW_H
#define NCM_COMMENTS_WINDOW_H

#include <Messenger.h>
#include <String.h>
#include <Window.h>

class BTextView;

enum {
	kMessageCommentsClosed = 'cmcl',
};

// 评论窗口：一个当前歌曲的所有评论（热评在前），点底板的评论按钮弹出
class CommentsWindow : public BWindow {
public:
	CommentsWindow(BMessenger target, long long songId, const BString& title);
	~CommentsWindow() override;

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void _SetText(const BString& text);

	BMessenger fTarget;
	long long fSongId;
	BTextView* fText = nullptr;
	volatile bool fLoading = false;
};

#endif
