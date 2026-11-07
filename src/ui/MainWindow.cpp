#include "MainWindow.h"

#include "ListMetrics.h"
#include "LoginWindow.h"
#include "SongHeader.h"
#include "SongItem.h"
#include "audio/Player.h"

#include <Alert.h>

#include <Catalog.h>
#include <Bitmap.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <ScrollView.h>
#include <Slider.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <utility>

using namespace ncm::metrics;

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"

namespace {

const uint32 kSelectionChanged = 'slch';

BString Prefixed(const char* prefix, const std::string& detail)
{
	BString text(prefix);
	text << detail.c_str();
	return text;
}

std::string EnsureDir(const std::string& path)
{
	mkdir(path.c_str(), 0755);
	return path;
}

std::string SessionDir()
{
	const char* home = getenv("HOME");
	if (home == nullptr)
		home = "/boot/home";
	return EnsureDir(std::string(home) + "/HaikuNCM");
}

std::string CacheDir()
{
	return EnsureDir(SessionDir() + "/cache");
}

std::string CoverDir()
{
	return EnsureDir(CacheDir() + "/covers");
}

std::string CoverPath(long long id)
{
	return CoverDir() + "/" + std::to_string(id) + ".jpg";
}

std::string AudioPath(long long id, const std::string& type)
{
	return CacheDir() + "/" + std::to_string(id) + "."
		+ (type.empty() ? "mp3" : type);
}

std::string SessionPath()
{
	return SessionDir() + "/session.txt";
}

void SaveSession(const std::string& cookie, const std::string& nickname)
{
	FILE* file = fopen(SessionPath().c_str(), "w");
	if (file == nullptr)
		return;
	fprintf(file, "%s\n%s\n", nickname.c_str(), cookie.c_str());
	fclose(file);
}

void TrimLine(char* text)
{
	for (char* cursor = text; *cursor != '\0'; cursor++) {
		if (*cursor == '\n' || *cursor == '\r') {
			*cursor = '\0';
			return;
		}
	}
}

bool LoadSession(std::string* cookie, std::string* nickname)
{
	FILE* file = fopen(SessionPath().c_str(), "r");
	if (file == nullptr)
		return false;

	char nicknameBuffer[256] = { 0 };
	char cookieBuffer[2048] = { 0 };
	const bool ok = fgets(nicknameBuffer, sizeof(nicknameBuffer), file) != nullptr
		&& fgets(cookieBuffer, sizeof(cookieBuffer), file) != nullptr;
	fclose(file);
	if (!ok)
		return false;

	TrimLine(nicknameBuffer);
	TrimLine(cookieBuffer);

	*nickname = nicknameBuffer;
	*cookie = cookieBuffer;
	return !cookie->empty();
}

std::string FormatTime(double seconds)
{
	if (seconds < 0.0)
		seconds = 0.0;

	const int total = static_cast<int>(seconds);
	char buffer[16];
	snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
	return buffer;
}

} // namespace

MainWindow::MainWindow()
	: BWindow(BRect(80, 80, 880, 660), "HaikuNCM", B_TITLED_WINDOW,
		  B_QUIT_ON_WINDOW_CLOSE)
{
	fApi = new ncm::NeteaseApi();
	fPlayer = new ncm::Player();

	fMenuBar = new BMenuBar("menubar");
	_BuildMenu();

	fLoginButton = new BButton("login-button", B_TRANSLATE("登录…"),
		new BMessage(kMessageLogin));

	fSearchField = new BTextControl("search", B_TRANSLATE("搜索"), "",
		new BMessage(kMessageSearch));
	fSearchField->SetModificationMessage(nullptr);
	fSearchField->SetExplicitMinSize(BSize(280, B_SIZE_UNSET));
	fSearchButton = new BButton("search-button", B_TRANSLATE("搜索"),
		new BMessage(kMessageSearch));

	fHeader = new SongHeader();
	fResults = new BListView("results", B_SINGLE_SELECTION_LIST);
	fResults->SetViewUIColor(B_LIST_BACKGROUND_COLOR);
	fResults->SetSelectionMessage(new BMessage(kSelectionChanged));
	fResults->SetInvocationMessage(new BMessage(kMessagePlay));
	fResults->SetExplicitMinSize(BSize(560, 260));

	fPrevButton = new BButton("prev", B_TRANSLATE("上一首"), new BMessage(kMessagePrev));
	fPlayButton = new BButton("play", B_TRANSLATE("播放"), new BMessage(kMessagePlay));
	fNextButton = new BButton("next", B_TRANSLATE("下一首"), new BMessage(kMessageNext));

	fSeek = new BSlider("seek", nullptr, new BMessage(kMessageSeek), 0, 1000,
		B_HORIZONTAL);
	fSeek->SetExplicitMinSize(BSize(160, B_SIZE_UNSET));
	fSeek->SetEnabled(false);

	fTimeLabel = new BStringView("time", "0:00 / 0:00");
	fStatus = new BStringView("status", B_TRANSLATE("输入关键词开始搜索"));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.AddGroup(B_HORIZONTAL, 0)
			.Add(fMenuBar)
			.Add(fLoginButton)
			.SetInsets(0, 0, B_USE_SMALL_SPACING, 0)
		.End()
		.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS)
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.Add(fSearchField)
				.Add(fSearchButton)
			.End()
			.Add(fHeader)
			.Add(new BScrollView("scroll", fResults, 0, false, true))
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.Add(fPrevButton)
				.Add(fPlayButton)
				.Add(fNextButton)
				.Add(fSeek, 1.0f)
				.Add(fTimeLabel)
			.End()
			.Add(fStatus)
		.End();

	SetSizeLimits(720, 4000, 400, 4000);
	ResizeTo(800, 580);
	CenterOnScreen();

	fTicker = new BMessageRunner(BMessenger(this), new BMessage(kMessageTick),
		500000);

	std::string cookie;
	std::string nickname;
	if (LoadSession(&cookie, &nickname) && !cookie.empty()) {
		fApi->SetCookie(cookie);
		BString label("使用 ");
		label << nickname.c_str() << " 登录";
		fLoginButton->SetLabel(label.String());
		fLoginButton->SetEnabled(false);
		if (fLogoutItem != nullptr)
			fLogoutItem->SetEnabled(true);
	}
}

MainWindow::~MainWindow()
{
	delete fPlayer;
	delete fApi;
}

void MainWindow::_BuildMenu()
{
	BMenu* fileMenu = new BMenu(B_TRANSLATE("文件"));
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("搜索…"),
		new BMessage(kMessageFocusSearch), 'F'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("退出"), new BMessage(B_QUIT_REQUESTED), 'Q'));

	BMenu* musicMenu = new BMenu(B_TRANSLATE("音乐"));
	BMenuItem* myMusic = new BMenuItem(B_TRANSLATE("我的音乐"), new BMessage(kMessageStatus));
	myMusic->SetEnabled(false);
	musicMenu->AddItem(myMusic);
	BMenuItem* recent = new BMenuItem(B_TRANSLATE("最近播放"), new BMessage(kMessageStatus));
	recent->SetEnabled(false);
	musicMenu->AddItem(recent);

	fAccountMenu = new BMenu(B_TRANSLATE("账号"));
	fLoginItem = new BMenuItem(B_TRANSLATE("登录…"), new BMessage(kMessageLogin));
	fLogoutItem = new BMenuItem(B_TRANSLATE("退出登录"), new BMessage(kMessageLogout));
	fLogoutItem->SetEnabled(false);
	fAccountMenu->AddItem(fLoginItem);
	fAccountMenu->AddItem(fLogoutItem);

	BMenu* helpMenu = new BMenu(B_TRANSLATE("帮助"));
	helpMenu->AddItem(new BMenuItem(B_TRANSLATE("关于 HaikuNCM"), new BMessage(kMessageAbout)));

	fMenuBar->AddItem(fileMenu);
	fMenuBar->AddItem(musicMenu);
	fMenuBar->AddItem(fAccountMenu);
	fMenuBar->AddItem(helpMenu);
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
		case kMessageFocusSearch:
			fSearchField->MakeFocus(true);
			return;

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

		case kMessageCoversDone:
			_ApplyCovers();
			return;

		case kMessagePlay:
			if (fBusy)
				return;
			if (fPlayer->IsLoaded()
				&& fResults->CurrentSelection() == fCurrentIndex)
				_TogglePlayback();
			else
				_StartPlay(fResults->CurrentSelection());
			return;

		case kMessagePrev:
		{
			int32 target = fCurrentIndex > 0 ? fCurrentIndex - 1
											: fResults->CountItems() - 1;
			if (target >= 0)
				_StartPlay(target);
			return;
		}

		case kMessageNext:
		{
			const int32 count = fResults->CountItems();
			int32 target = 0;
			if (fCurrentIndex >= 0 && fCurrentIndex + 1 < count)
				target = fCurrentIndex + 1;
			if (count > 0)
				_StartPlay(target);
			return;
		}

		case kMessagePlayDone:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				fStatus->SetText(text);

			const char* path = nullptr;
			if (message->FindString("path", &path) == B_OK) {
				fCurrentIndex = fResults->CurrentSelection();

				std::string error;
				if (!fPlayer->Load(path, &error)) {
					fStatus->SetText(Prefixed(B_TRANSLATE("解码失败: "), error));
				} else {
					fSeek->SetEnabled(true);
					if (fPlayer->Play(&error)) {
						fPlayButton->SetLabel(B_TRANSLATE("暂停"));
					} else {
						fPlayButton->SetLabel(B_TRANSLATE("播放"));
						fStatus->SetText(Prefixed(B_TRANSLATE("没有音频输出设备: "), error));
					}
					_UpdateTransport();
				}
			}
			fBusy = false;
			return;
		}

		case kMessageStatus:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				fStatus->SetText(text);
			return;
		}

		case kMessageSeek:
		{
			if (!fPlayer->IsLoaded())
				return;

			const double target = fSeek->Value() / 1000.0
				* fPlayer->DurationSeconds();
			std::string error;
			if (!fPlayer->Seek(target, &error))
				fStatus->SetText(Prefixed(B_TRANSLATE("定位失败: "), error));
			_UpdateTransport();
			return;
		}

		case kMessageTick:
			if (fPlayer->IsPlaying())
				_UpdateTransport();
			return;

		case kMessageLogin:
			(new LoginWindow(BMessenger(this)))->Show();
			return;

		case kMessageLoginDone:
		{
			const char* nickname = nullptr;
			const char* cookie = nullptr;
			if (message->FindString("nickname", &nickname) == B_OK
				&& message->FindString("cookie", &cookie) == B_OK) {
				fApi->SetCookie(cookie);
				SaveSession(cookie, nickname);

				BString label("使用 ");
				label << nickname << " 登录";
				fLoginButton->SetLabel(label.String());
				fLoginButton->SetEnabled(false);
				if (fLogoutItem != nullptr)
					fLogoutItem->SetEnabled(true);

				BString status("已登录: ");
				status << nickname;
				fStatus->SetText(status.String());
			}
			return;
		}

		case kMessageLogout:
			fApi->SetCookie(std::string());
			remove(SessionPath().c_str());
			fLoginButton->SetLabel(B_TRANSLATE("登录…"));
			fLoginButton->SetEnabled(true);
			if (fLogoutItem != nullptr)
				fLogoutItem->SetEnabled(false);
			fStatus->SetText(B_TRANSLATE("已退出登录"));
			return;

		case kMessageAbout:
		{
			BAlert* alert = new BAlert("关于 HaikuNCM",
				"HaikuNCM 1.0\n\nHaiku 原生第三方网易云音乐客户端。\n"
				"界面与播放全部使用 Haiku 的 Interface Kit / Media Kit。\n\n"
				"协议参考 NeteaseCloudMusicApi，代码为独立实现。",
				"好");
			alert->Go();
			return;
		}

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
		fStatus->SetText(B_TRANSLATE("先输入要搜的歌名"));
		return;
	}

	fKeyword = text;
	fBusy = true;
	fStatus->SetText(B_TRANSLATE("搜索中…"));
	fSearchButton->SetEnabled(false);

	thread_id thread = spawn_thread(_SearchThread, "ncm-search",
		B_NORMAL_PRIORITY, this);
	if (thread < B_OK) {
		fBusy = false;
		fStatus->SetText(B_TRANSLATE("无法创建搜索线程"));
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
		BMessage failure(kMessageStatus);
		failure.AddString("text", Prefixed(B_TRANSLATE("搜索失败: "), error));
		self->_PostIfAlive(&failure);
	}
	return 0;
}

void MainWindow::_FillResults()
{
	fPlayer->Stop();
	fResults->MakeEmpty();
	fCurrentIndex = -1;
	fPlayButton->SetLabel(B_TRANSLATE("播放"));
	fSeek->SetValue(0);
	fSeek->SetEnabled(false);
	fTimeLabel->SetText("0:00 / 0:00");

	fLock.Lock();
	const size_t count = fSongs.size();
	for (size_t i = 0; i < count; i++)
		fResults->AddItem(new SongItem(fSongs[i], static_cast<int32>(i)));
	fLock.Unlock();

	fBusy = false;
	fSearchButton->SetEnabled(true);

	BString status;
	status << "找到 " << count << " 首（双击播放）";
	fStatus->SetText(status.String());

	_StartCoverDownload();
}

void MainWindow::_StartCoverDownload()
{
	thread_id thread = spawn_thread(_CoverThread, "ncm-covers", B_LOW_PRIORITY,
		this);
	if (thread >= B_OK)
		resume_thread(thread);
}

int32 MainWindow::_CoverThread(void* data)
{
	auto* self = static_cast<MainWindow*>(data);

	std::vector<ncm::Song> songs;
	self->fLock.Lock();
	songs = self->fSongs;
	self->fLock.Unlock();

	for (const ncm::Song& song : songs) {
		if (song.coverUrl.empty())
			continue;

		const std::string path = CoverPath(song.id);
		struct stat info;
		if (stat(path.c_str(), &info) == 0 && info.st_size > 1024)
			continue;

		std::string error;
		self->fApi->DownloadCover(song.coverUrl, path, &error);
	}

	BMessage done(kMessageCoversDone);
	self->_PostIfAlive(&done);
	return 0;
}

void MainWindow::_ApplyCovers()
{
	const int32 count = fResults->CountItems();
	for (int32 i = 0; i < count; i++) {
		auto* item = dynamic_cast<SongItem*>(fResults->ItemAt(i));
		if (item == nullptr)
			continue;

		const std::string path = CoverPath(item->Song().id);
		BBitmap* bitmap = BTranslationUtils::GetBitmapFile(path.c_str());
		if (bitmap != nullptr)
			item->SetCover(bitmap);
	}

	fResults->Invalidate();
}

void MainWindow::_UpdateSelectionLabel()
{
	// 列表选择变化时只需重绘；播放条由播放动作驱动
	fResults->Invalidate();
}

void MainWindow::_StartPlay(int32 index)
{
	if (fBusy)
		return;

	fLock.Lock();
	const bool valid = index >= 0 && static_cast<size_t>(index) < fSongs.size();
	fLock.Unlock();

	if (!valid) {
		fStatus->SetText(B_TRANSLATE("先在列表里选一首歌"));
		return;
	}

	fBusy = true;
	fStatus->SetText(B_TRANSLATE("正在获取播放地址…"));

	auto* payload = new std::pair<MainWindow*, int32>(this, index);
	thread_id thread = spawn_thread(_PlayThread, "ncm-play", B_NORMAL_PRIORITY,
		payload);
	if (thread < B_OK) {
		delete payload;
		fBusy = false;
		fStatus->SetText(B_TRANSLATE("无法创建播放线程"));
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
		result.AddString("text", Prefixed(B_TRANSLATE("取流失败: "), error));
		self->_PostIfAlive(&result);
		return 0;
	}

	const std::string path = AudioPath(song.id, audio.type);
	struct stat info;
	const bool cached = stat(path.c_str(), &info) == 0
		&& info.st_size == audio.size;
	if (!cached) {
		if (!self->fApi->DownloadAudio(audio, path, &error)) {
			result.AddString("text", Prefixed(B_TRANSLATE("下载失败: "), error));
			self->_PostIfAlive(&result);
			return 0;
		}
	}

	result.AddString("path", path.c_str());
	self->_PostIfAlive(&result);
	return 0;
}

void MainWindow::_TogglePlayback()
{
	if (!fPlayer->IsLoaded())
		return;

	std::string error;
	if (fPlayer->IsPlaying()) {
		fPlayer->Stop();
		fPlayButton->SetLabel(B_TRANSLATE("播放"));
		return;
	}

	if (fPlayer->Play(&error))
		fPlayButton->SetLabel(B_TRANSLATE("暂停"));
	else
		fStatus->SetText(Prefixed(B_TRANSLATE("没有音频输出设备: "), error));
}

void MainWindow::_UpdateTransport()
{
	if (!fPlayer->IsLoaded()) {
		fTimeLabel->SetText("0:00 / 0:00");
		return;
	}

	const double duration = fPlayer->DurationSeconds();
	const double position = fPlayer->PositionSeconds();

	if (!fSeeking) {
		const int32 value = duration > 0.0
			? static_cast<int32>(position / duration * 1000.0)
			: 0;
		fSeek->SetValue(value);
	}

	BString label(FormatTime(position).c_str());
	label << " / " << FormatTime(duration).c_str();
	fTimeLabel->SetText(label.String());
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
