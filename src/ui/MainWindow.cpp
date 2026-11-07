#include "MainWindow.h"

#include "audio/Player.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <utility>

namespace {

const uint32 kSelectionChanged = 'slch';

BString Prefixed(const char* prefix, const std::string& detail)
{
	BString text(prefix);
	text << detail.c_str();
	return text;
}

std::string FormatDuration(long long milliseconds)
{
	const long long seconds = milliseconds / 1000;
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%lld:%02lld", seconds / 60,
		seconds % 60);
	return buffer;
}

std::string CachePath(long long id, const std::string& type)
{
	const char* home = getenv("HOME");
	if (home == nullptr)
		home = "/boot/home";

	const std::string base = std::string(home) + "/HaikuNCM";
	const std::string cache = base + "/cache";
	mkdir(base.c_str(), 0755);
	mkdir(cache.c_str(), 0755);

	return cache + "/" + std::to_string(id) + "."
		+ (type.empty() ? "mp3" : type);
}

} // namespace

MainWindow::MainWindow()
	: BWindow(BRect(80, 80, 820, 600), "HaikuNCM", B_TITLED_WINDOW,
		  B_QUIT_ON_WINDOW_CLOSE)
{
	fApi = new ncm::NeteaseApi();

	fMenuBar = new BMenuBar("menubar");
	BMenu* fileMenu = new BMenu("文件");
	fileMenu->AddItem(new BMenuItem("退出", new BMessage(B_QUIT_REQUESTED), 'Q'));
	fMenuBar->AddItem(fileMenu);

	fSearchField = new BTextControl("search", nullptr, "",
		new BMessage(kMessageSearch));
	fSearchField->SetModificationMessage(nullptr);
	fSearchButton = new BButton("search-button", "搜索",
		new BMessage(kMessageSearch));

	fResults = new BListView("results");
	fResults->SetExplicitMinSize(BSize(540, 280));
	fResults->SetSelectionMessage(new BMessage(kSelectionChanged));
	fResults->SetInvocationMessage(new BMessage(kMessagePlay));

	fNowPlaying = new BStringView("now-playing", "未播放");
	fPlayButton = new BButton("play-button", "播放", new BMessage(kMessagePlay));

	fStatus = new BStringView("status", "输入关键词开始搜索");

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS)
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.Add(fSearchField)
				.Add(fSearchButton)
			.End()
			.Add(fResults)
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.Add(fNowPlaying, 1.0f)
				.Add(fPlayButton)
			.End()
			.Add(fStatus)
		.End();

	SetSizeLimits(520, 4000, 320, 4000);
	ResizeTo(760, 540);
	CenterOnScreen();
}

MainWindow::~MainWindow()
{
	delete fApi;
}

void MainWindow::SearchFor(const char* keyword)
{
	if (keyword == nullptr)
		return;

	// 可能来自其他线程，控件只能在自己的窗口线程里碰，走消息
	BMessage message(kMessageSearch);
	message.AddString("keyword", keyword);
	PostMessage(&message);
}

void MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessageSearch:
		{
			const char* keyword = nullptr;
			if (message->FindString("keyword", &keyword) == B_OK
				&& keyword != nullptr)
				fSearchField->SetText(keyword);
			_StartSearch();
			return;
		}

		case kMessageSearchDone:
			_FillResults();
			return;

		case kMessagePlay:
			_StartPlay(fResults->CurrentSelection());
			return;

		case kMessagePlayDone:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				fStatus->SetText(text);
			fBusy = false;
			break;
		}

		case kMessageStatus:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				fStatus->SetText(text);
			return;
		}

		case kSelectionChanged:
			_UpdateSelectionLabel();
			break;

		default:
			BWindow::MessageReceived(message);
			return;
	}
}

bool MainWindow::QuitRequested()
{
	fQuitting = true;
	return true;
}

void MainWindow::_StartSearch()
{
	if (fBusy)
		return;

	const char* text = fSearchField->Text();
	if (text == nullptr || text[0] == '\0') {
		fStatus->SetText("先输入要搜的歌名");
		return;
	}

	fKeyword = text;
	fBusy = true;
	fStatus->SetText("搜索中…");
	fSearchButton->SetEnabled(false);

	thread_id thread = spawn_thread(_SearchThread, "ncm-search",
		B_NORMAL_PRIORITY, this);
	if (thread < B_OK) {
		fBusy = false;
		fStatus->SetText("无法创建搜索线程");
		return;
	}
	resume_thread(thread);
}

int32 MainWindow::_SearchThread(void* data)
{
	auto* self = static_cast<MainWindow*>(data);

	std::vector<ncm::Song> songs;
	std::string error;
	const BString keyword = self->fKeyword;

	const bool ok = self->fApi->Search(keyword.String(), 30, &songs, &error);

	if (ok) {
		self->fLock.Lock();
		self->fSongs.swap(songs);
		self->fLock.Unlock();

		BMessage done(kMessageSearchDone);
		self->_PostIfAlive(&done);
	} else {
		BMessage status(kMessageStatus);
		status.AddString("text", Prefixed("搜索失败: ", error));
		self->_PostIfAlive(&status);

		BMessage reset(kMessagePlayDone);
		reset.AddString("text", Prefixed("搜索失败: ", error));
		self->_PostIfAlive(&reset);
	}
	return 0;
}

void MainWindow::_FillResults()
{
	fResults->MakeEmpty();

	fLock.Lock();
	const size_t count = fSongs.size();
	for (size_t i = 0; i < count; i++) {
		const ncm::Song& song = fSongs[i];
		BString label;
		label << song.name.c_str() << "  ·  " << song.artist.c_str()
			  << "  ·  " << FormatDuration(song.durationMs).c_str();
		fResults->AddItem(new BStringItem(label.String()));
	}
	fLock.Unlock();

	fBusy = false;
	fSearchButton->SetEnabled(true);

	BString status;
	status << "找到 " << count << " 首（双击试听）";
	fStatus->SetText(status.String());
}

void MainWindow::_UpdateSelectionLabel()
{
	const int32 index = fResults->CurrentSelection();
	if (index < 0) {
		fNowPlaying->SetText("未播放");
		return;
	}

	fLock.Lock();
	if (static_cast<size_t>(index) < fSongs.size()) {
		const ncm::Song& song = fSongs[index];
		BString text;
		text << song.name.c_str() << "  ·  " << song.artist.c_str();
		fNowPlaying->SetText(text.String());
	}
	fLock.Unlock();
}

void MainWindow::_StartPlay(int32 index)
{
	if (fBusy)
		return;

	fLock.Lock();
	const bool valid = index >= 0 && static_cast<size_t>(index) < fSongs.size();
	fLock.Unlock();

	if (!valid) {
		fStatus->SetText("先在列表里选一首歌");
		return;
	}

	fBusy = true;
	fStatus->SetText("正在获取播放地址…");

	auto* payload = new std::pair<MainWindow*, int32>(this, index);
	thread_id thread = spawn_thread(_PlayThread, "ncm-play", B_NORMAL_PRIORITY,
		payload);
	if (thread < B_OK) {
		delete payload;
		fBusy = false;
		fStatus->SetText("无法创建播放线程");
		return;
	}
	resume_thread(thread);
}

int32 MainWindow::_PlayThread(void* data)
{
	auto* payload = static_cast<std::pair<MainWindow*, int32>*>(data);
	MainWindow* self = payload->first;
	const int32 index = payload->second;
	delete payload;

	self->fLock.Lock();
	const ncm::Song song = self->fSongs[index];
	self->fLock.Unlock();

	BMessage result(kMessagePlayDone);

	ncm::AudioUrl audio;
	std::string error;
	if (!self->fApi->SongUrl(song.id, "standard", &audio, &error)) {
		result.AddString("text", Prefixed("取流失败: ", error));
		self->_PostIfAlive(&result);
		return 0;
	}

	const std::string path = CachePath(song.id, audio.type);
	struct stat info;
	const bool cached = stat(path.c_str(), &info) == 0
		&& info.st_size == audio.size;
	if (!cached) {
		if (!self->fApi->DownloadAudio(audio, path, &error)) {
			result.AddString("text", Prefixed("下载失败: ", error));
			self->_PostIfAlive(&result);
			return 0;
		}
	}

	ncm::Player player;
	if (!player.Load(path, &error)) {
		result.AddString("text", Prefixed("解码失败: ", error));
		self->_PostIfAlive(&result);
		return 0;
	}

	const std::string description = player.Description();
	if (!player.Play(&error)) {
		BString text;
		text << song.name.c_str() << " 已就绪（" << description.c_str()
			 << "），但" << error.c_str();
		result.AddString("text", text.String());
		self->_PostIfAlive(&result);
		return 0;
	}

	BString text;
	text << "正在播放: " << song.name.c_str() << "（" << description.c_str()
		 << "）";
	result.AddString("text", text.String());
	self->_PostIfAlive(&result);

	// keep the player alive while the sound is playing
	snooze(static_cast<bigtime_t>(300 * 1000 * 1000));
	return 0;
}

void MainWindow::_PostIfAlive(BMessage* message)
{
	if (fQuitting)
		return;

	if (Lock()) {
		PostMessage(message);
		Unlock();
	}
}
