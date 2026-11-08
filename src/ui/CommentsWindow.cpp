#include "CommentsWindow.h"

#include "net/netease.h"

#include <Alert.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <TextView.h>

#include <string>
#include <vector>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "CommentsWindow"

namespace {

const uint32 kMessageLoaded = 'cmld';
const uint32 kMessageFailed = 'cmfl';

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

} // namespace

CommentsWindow::CommentsWindow(BMessenger target, long long songId,
	const BString& title)
	: BWindow(BRect(0, 0, 460, 520),
		  BString(B_TRANSLATE("评论 — ")).Append(title.String()).String(),
		  B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS)
	, fTarget(target)
	, fSongId(songId)
{
	fText = new BTextView("comments");
	fText->MakeEditable(false);
	fText->SetWordWrap(true);
	fText->SetInsets(8.0f, 8.0f, 8.0f, 8.0f);
	fText->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	fText->SetText(B_TRANSLATE("正在加载评论…"));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0.0f)
		.Add(new BScrollView("comments-scroll", fText, 0, false, true), 1.0f)
		.End();

	ResizeTo(460, 520);
	CenterOnScreen();

	fLoading = true;
	thread_id thread = spawn_thread([](void* data) -> int32 {
		auto* job = static_cast<Job*>(data);
		CommentsWindow* window = job->window;
		const long long songId = job->songId;
		delete job;

		ncm::NeteaseApi api;
		std::string error;
		std::vector<ncm::Comment> comments;
		BString text;

		try {
			if (!api.FetchComments(songId, 100, 0, &comments, &error)) {
				BMessage failure(kMessageFailed);
				failure.AddString("text",
					Prefixed(B_TRANSLATE("取评论失败: "), error).String());
				if (window->Lock()) {
					window->PostMessage(&failure);
					window->Unlock();
				}
				return 0;
			}
			for (const ncm::Comment& comment : comments) {
				text << comment.nickname.c_str();
				if (comment.likedCount > 0)
					text << "  ·  " << comment.likedCount
							<< B_TRANSLATE(" 赞");
				text << "\n" << comment.content.c_str() << "\n\n";
			}
		} catch (const std::exception& exception) {
			BMessage failure(kMessageFailed);
			failure.AddString("text",
				Prefixed(B_TRANSLATE("取评论失败: "), exception.what()).String());
			if (window->Lock()) {
				window->PostMessage(&failure);
				window->Unlock();
			}
			return 0;
		}

		BMessage loaded(kMessageLoaded);
		loaded.AddString("text", text.String());
		if (window->Lock()) {
			window->PostMessage(&loaded);
			window->Unlock();
		}
		return 0;
	}, "ncm-comments", B_NORMAL_PRIORITY, new Job{this, songId});
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
	fText->SetText(text.IsEmpty() ? B_TRANSLATE("（没有评论）") : text.String());
	fText->ScrollTo(0.0f, 0.0f);
}

void CommentsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessageLoaded:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				_SetText(text);
			fLoading = false;
			return;
		}

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
	BMessage closed(kMessageCommentsClosed);
	fTarget.SendMessage(&closed);
	return true;
}
