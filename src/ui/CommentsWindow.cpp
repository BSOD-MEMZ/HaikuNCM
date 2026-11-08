#include "CommentsWindow.h"

#include "net/netease.h"

#include <Alert.h>
#include <Catalog.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <ScrollBar.h>
#include <ScrollView.h>

#include <string>
#include <vector>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "CommentsWindow"

namespace {

const uint32 kMessageLoaded = 'cmld';
const uint32 kMessageFailed = 'cmfl';
const float kMargin = 10.0f;

struct Job {
	CommentsWindow* window;
	long long songId;
};

BString Prefixed(const char* prefix, const std::string& detail)
{
	BString text(prefix);
	text << detail.c_str();
	return text;
}

// 感知宽度变化：折行要跟着窗口宽走
class CommentListView : public BListView {
public:
	CommentListView()
		: BListView("comments", B_SINGLE_SELECTION_LIST,
			B_WILL_DRAW | B_FRAME_EVENTS)
		, fOwner(nullptr)
	{
	}

	void SetOwner(CommentsWindow* owner) { fOwner = owner; }

	void FrameResized(float width, float height) override
	{
		BListView::FrameResized(width, height);
		if (fOwner != nullptr)
			fOwner->RelayoutItems();
	}

private:
	CommentsWindow* fOwner;
};

// 一条评论：昵称（粗体）+ 赞数（次要色）+ 内容按宽度折行 + 底部分隔线
class CommentItem : public BListItem {
public:
	CommentItem(const BString& nickname, const BString& content, int32 likes)
		: fNickname(nickname)
		, fContent(content)
		, fLikes(likes)
		, fWidth(0)
	{
	}

	void SetWidth(float width)
	{
		if (width == fWidth)
			return;
		fWidth = width;
		Relayout(nullptr); // 先按单行占位，挂进窗口后会用真实宽度重排
	}

	void Relayout(BView* measure)
	{
		fLines.clear();
		float height = 6.0f; // 顶部留白

		BFont plain(be_plain_font);
		const float lineHeight = plain.Size() * 1.5f;
		height += lineHeight + 2.0f; // 昵称行

		if (measure == nullptr || fWidth <= 0) {
			if (!fContent.IsEmpty())
				fLines.push_back(fContent);
			height += lineHeight + 10.0f;
			SetHeight(height);
			return;
		}

		const float textWidth = fWidth - kMargin * 2.0f;
		BString remaining(fContent);
		while (remaining.Length() > 0) {
			// 逐字贪心折行（中文没有空格可分）
			int32 take = 0;
			int32 offset = 0;
			while (offset < remaining.Length()) {
				int32 charLen = 1;
				const uchar c = static_cast<uchar>(remaining.String()[offset]);
				if ((c & 0x80) != 0) {
					charLen = 2;
					if ((c & 0xE0) == 0xE0)
						charLen = 3;
					if ((c & 0xF0) == 0xF0)
						charLen = 4;
				}
				const int32 candidate = offset + charLen;
				BString part;
				remaining.CopyInto(part, 0, candidate);
				if (measure->StringWidth(part.String()) > textWidth
					&& offset > 0)
					break;
				take = candidate;
				offset = candidate;
			}

			if (take <= 0)
				take = remaining.Length();

			BString line;
			remaining.CopyInto(line, 0, take);
			fLines.push_back(line);
			BString rest;
			remaining.CopyInto(rest, take, remaining.Length() - take);
			remaining = rest;
			height += lineHeight;
		}

		height += 10.0f;
		SetHeight(height);
	}

	void DrawItem(BView* owner, BRect frame, bool complete = false) override
	{
		if (fWidth <= 0)
			SetWidth(frame.Width());

		if (complete) {
			owner->SetHighColor(ui_color(B_LIST_BACKGROUND_COLOR));
			owner->FillRect(frame);
		}

		const float textWidth = fWidth - kMargin * 2.0f;
		font_height fh;
		owner->GetFontHeight(&fh);
		const float lineHeight = be_plain_font->Size() * 1.5f;
		float y = frame.top + 6.0f;

		owner->SetFont(be_bold_font);
		owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
		BString name(fNickname);
		owner->TruncateString(&name, B_TRUNCATE_END, textWidth * 0.7f);
		owner->MovePenTo(kMargin, y + fh.ascent);
		owner->DrawString(name);

		if (fLikes > 0) {
			owner->SetFont(be_plain_font);
			owner->SetHighColor(tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
				B_LIGHTEN_1_TINT));
			BString likes;
			likes << fLikes << B_TRANSLATE(" 赞");
			const float likesWidth = owner->StringWidth(likes.String());
			owner->MovePenTo(frame.right - kMargin - likesWidth,
				y + fh.ascent);
			owner->DrawString(likes);
		}

		y += lineHeight;

		owner->SetFont(be_plain_font);
		owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
		for (const BString& line : fLines) {
			owner->MovePenTo(kMargin, y + fh.ascent);
			owner->DrawString(line);
			y += lineHeight;
		}

		owner->SetHighColor(tint_color(ui_color(B_LIST_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		owner->StrokeLine(BPoint(frame.left + kMargin, frame.bottom - 1.0f),
			BPoint(frame.right - kMargin, frame.bottom - 1.0f));
		owner->SetFont(be_plain_font);
	}

private:
	BString fNickname;
	BString fContent;
	int32 fLikes;
	std::vector<BString> fLines;
	float fWidth;
};

} // namespace

CommentsWindow::CommentsWindow(BMessenger target, long long songId,
	const BString& title)
	: BWindow(BRect(0, 0, 460, 520),
		  BString(B_TRANSLATE("评论 — ")).Append(title.String()).String(),
		  B_TITLED_WINDOW, 0)
	, fTarget(target)
	, fSongId(songId)
{
	auto* list = new CommentListView();
	list->SetOwner(this);
	list->SetViewUIColor(B_LIST_BACKGROUND_COLOR);
	fList = list;

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0.0f)
		.Add(new BScrollView("comments-scroll", fList, 0, false, true), 1.0f)
		.End();

	ResizeTo(460, 520);
	CenterOnScreen();

	fLoading = true;
	auto* job = new Job{this, songId};
	thread_id thread = spawn_thread([](void* data) -> int32 {
		auto* payload = static_cast<Job*>(data);
		CommentsWindow* window = payload->window;
		const long long songId = payload->songId;
		delete payload;

		ncm::NeteaseApi api;
		std::string error;
		std::vector<ncm::Comment> comments;
		BMessage loaded(kMessageLoaded);

		try {
			if (api.FetchComments(songId, 100, 0, &comments, &error)) {
				for (const ncm::Comment& comment : comments) {
					loaded.AddString("nickname", comment.nickname.c_str());
					loaded.AddString("content", comment.content.c_str());
					loaded.AddInt32("likes", comment.likedCount);
				}
			} else {
				BMessage failure(kMessageFailed);
				failure.AddString("text",
					Prefixed(B_TRANSLATE("取评论失败: "), error).String());
				if (window->Lock()) {
					window->PostMessage(&failure);
					window->Unlock();
				}
				return 0;
			}
		} catch (const std::exception& exception) {
			BMessage failure(kMessageFailed);
			failure.AddString("text",
				Prefixed(B_TRANSLATE("取评论失败: "),
					exception.what()).String());
			if (window->Lock()) {
				window->PostMessage(&failure);
				window->Unlock();
			}
			return 0;
		}

		if (window->Lock()) {
			window->PostMessage(&loaded);
			window->Unlock();
		}
		return 0;
	}, "ncm-comments", B_NORMAL_PRIORITY, job);
	if (thread >= B_OK)
		resume_thread(thread);
	else
		fLoading = false;
}

CommentsWindow::~CommentsWindow()
{
	fLoading = false;
}

void CommentsWindow::_SetText(const BString& text)
{
	fList->MakeEmpty();
	BStringItem* item = new BStringItem(text);
	item->SetHeight(60.0f);
	fList->AddItem(item);
	fList->Invalidate();
}

void CommentsWindow::_FillComments(BMessage* message)
{
	fList->MakeEmpty();

	const char* nickname = nullptr;
	const char* content = nullptr;
	int32 likes = 0;
	int32 index = 0;
	while (message->FindString("nickname", index, &nickname) == B_OK
		&& message->FindString("content", index, &content) == B_OK) {
		message->FindInt32("likes", index, &likes);
		fList->AddItem(new CommentItem(nickname, content, likes));
		index++;
	}

	if (index == 0) {
		BStringItem* empty = new BStringItem(B_TRANSLATE("（没有评论）"));
		empty->SetHeight(60.0f);
		fList->AddItem(empty);
	}

	// 内容折行要按真实宽度算，先按当前宽度重排一遍
	const float itemWidth = fList->Bounds().Width() - B_V_SCROLL_BAR_WIDTH;
	for (int32 i = 0; i < fList->CountItems(); i++) {
		auto* item = dynamic_cast<CommentItem*>(fList->ItemAt(i));
		if (item != nullptr)
			item->SetWidth(itemWidth);
	}
	RelayoutItems();
	fList->Invalidate();
}

void CommentsWindow::RelayoutItems()
{
	const float itemWidth = fList->Bounds().Width() - B_V_SCROLL_BAR_WIDTH;
	for (int32 i = 0; i < fList->CountItems(); i++) {
		auto* item = dynamic_cast<CommentItem*>(fList->ItemAt(i));
		if (item != nullptr) {
			item->SetWidth(itemWidth);
			item->Relayout(fList);
		}
	}
	fList->Invalidate();
}

void CommentsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessageLoaded:
			_FillComments(message);
			fLoading = false;
			return;

		case kMessageFailed:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				_SetText(text);
			fLoading = false;
			return;
		}

		default:
			break;
	}
	BWindow::MessageReceived(message);
}

bool CommentsWindow::QuitRequested()
{
	// 同歌单/歌手页：隐藏 + 通知主窗口回收，不能连坐整个程序
	if (Lock()) {
		Hide();
		Unlock();
	}

	BMessage closed(kMessageCommentsClosed);
	closed.AddPointer("window", this);
	fTarget.SendMessage(&closed);
	return false;
}

void CommentsWindow::Quit()
{
	BWindow::Quit();
}
