#include "Player.h"

#include <Entry.h>
#include <MediaDefs.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <SoundPlayer.h>

#include <cstdio>
#include <cstring>

namespace ncm {

struct Player::Impl {
	BMediaFile* file = nullptr;
	BMediaTrack* track = nullptr;
	BSoundPlayer* output = nullptr;
	media_raw_audio_format format;
	int channels = 0;
	float peak = 0.0f;
	volatile int64 framesRendered = 0;
	float gain = 1.0f;
	volatile bool finished = false;

	static void Render(void* cookie, void* buffer, size_t size,
		const media_raw_audio_format& format);
};

void Player::Impl::Render(void* cookie, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	auto* self = static_cast<Impl*>(cookie);

	// 播放结束/未加载：必须持续输出静音，否则旧缓冲会被反复播出（卡音 bug）
	if (self == nullptr || self->track == nullptr || self->finished) {
		memset(buffer, 0, size);
		return;
	}

	int64 frames = static_cast<int64>(size
		/ (sizeof(float) * format.channel_count));
	if (self->track->ReadFrames(buffer, &frames, nullptr) != B_OK
		|| frames <= 0) {
		memset(buffer, 0, size);
		self->finished = true;
		return;
	}

	self->framesRendered += frames;

	float* samples = static_cast<float*>(buffer);
	const size_t count = frames * format.channel_count;
	for (size_t i = 0; i < count; i++) {
		if (self->gain != 1.0f)
			samples[i] *= self->gain;
		const float value = samples[i] < 0 ? -samples[i] : samples[i];
		if (value > self->peak)
			self->peak = value;
	}
}

Player::Player()
	: fImpl(new Impl())
{
}

Player::~Player()
{
	Stop();

	if (fImpl->track != nullptr && fImpl->file != nullptr)
		fImpl->file->ReleaseTrack(fImpl->track);
	delete fImpl->file;
	delete fImpl;
}

bool Player::Load(const std::string& path, std::string* error)
{
	Stop();

	if (fImpl->track != nullptr && fImpl->file != nullptr)
		fImpl->file->ReleaseTrack(fImpl->track);
	fImpl->track = nullptr;
	delete fImpl->file;
	fImpl->file = nullptr;

	BEntry entry(path.c_str(), true);
	entry_ref ref;
	if (entry.InitCheck() != B_OK || entry.GetRef(&ref) != B_OK) {
		if (error != nullptr)
			*error = "文件不存在: " + path;
		return false;
	}

	fImpl->file = new BMediaFile(&ref);
	if (fImpl->file->InitCheck() != B_OK) {
		if (error != nullptr)
			*error = std::string("无法识别该文件: ")
				+ strerror(fImpl->file->InitCheck());
		return false;
	}

	media_format format;
	memset(reinterpret_cast<void*>(&format), 0, sizeof(format));
	format.type = B_MEDIA_RAW_AUDIO;
	format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;
	format.u.raw_audio.channel_count = 2;
	format.u.raw_audio.frame_rate = 44100;
	format.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
	format.u.raw_audio.buffer_size = 4096;

	for (int32 i = 0; i < fImpl->file->CountTracks(); i++) {
		BMediaTrack* candidate = fImpl->file->TrackAt(i);
		if (candidate == nullptr)
			continue;

		media_format negotiated = format;
		if (candidate->DecodedFormat(&negotiated) == B_OK
			&& negotiated.type == B_MEDIA_RAW_AUDIO) {
			fImpl->track = candidate;
			fImpl->format = negotiated.u.raw_audio;
			fImpl->channels = static_cast<int>(negotiated.u.raw_audio.channel_count);
			return true;
		}
		fImpl->file->ReleaseTrack(candidate);
	}

	if (error != nullptr)
		*error = "没有可解码的音频轨";
	return false;
}

bool Player::_StartOutput(std::string* error)
{
	auto* output = new BSoundPlayer(&fImpl->format, "HaikuNCM",
		Impl::Render, nullptr, fImpl);
	const status_t status = output->InitCheck();
	if (status != B_OK) {
		delete output;
		if (error != nullptr) {
			*error = std::string("没有可用的音频输出设备（")
				+ strerror(status) + "）";
		}
		return false;
	}

	fImpl->output = output;
	output->Start();
	output->SetHasData(true);
	return true;
}

bool Player::Play(std::string* error)
{
	if (!IsLoaded()) {
		if (error != nullptr)
			*error = "尚未载入音频";
		return false;
	}

	Stop();

	fImpl->finished = false;
	fImpl->peak = 0.0f;
	return _StartOutput(error);
}

void Player::Stop()
{
	if (fImpl->output == nullptr)
		return;

	fImpl->output->Stop();
	delete fImpl->output;
	fImpl->output = nullptr;
}

bool Player::IsLoaded() const
{
	return fImpl->track != nullptr;
}

void Player::SetGain(float gain)
{
	if (gain < 0.0f)
		gain = 0.0f;
	if (gain > 1.0f)
		gain = 1.0f;
	fImpl->gain = gain;
}

float Player::Gain() const
{
	return fImpl->gain;
}

bool Player::IsPlaying() const
{
	return fImpl->output != nullptr;
}

bool Player::IsFinished() const
{
	return fImpl->finished;
}

double Player::DurationSeconds() const
{
	if (fImpl->track == nullptr)
		return 0.0;
	return fImpl->track->Duration() / 1000000.0;
}

double Player::PositionSeconds() const
{
	if (fImpl->format.frame_rate <= 0)
		return 0.0;
	return fImpl->framesRendered / static_cast<double>(fImpl->format.frame_rate);
}

bool Player::Seek(double seconds, std::string* error)
{
	if (fImpl->track == nullptr) {
		if (error != nullptr)
			*error = "尚未载入音频";
		return false;
	}

	const double duration = DurationSeconds();
	if (duration > 0.0 && seconds > duration)
		seconds = duration;
	if (seconds < 0.0)
		seconds = 0.0;

	// BMediaTrack 不是线程安全的：必须先停输出，再动解码轨
	const bool wasPlaying = fImpl->output != nullptr;
	Stop();

	bigtime_t time = static_cast<bigtime_t>(seconds * 1000000.0);
	const status_t status = fImpl->track->SeekToTime(&time);
	if (status != B_OK) {
		if (error != nullptr)
			*error = std::string("定位失败: ") + strerror(status);
		return false;
	}

	// 进度计数对齐到定位点，而不是归零
	if (fImpl->format.frame_rate > 0)
		fImpl->framesRendered = static_cast<int64>(seconds * fImpl->format.frame_rate);
	fImpl->finished = false;

	if (wasPlaying)
		return _StartOutput(error);
	return true;
}

std::string Player::Description() const
{
	if (fImpl->track == nullptr)
		return std::string();

	char buffer[128];
	snprintf(buffer, sizeof(buffer), "%.0f Hz · %d 声道 · %.1fs",
		fImpl->format.frame_rate, fImpl->channels,
		fImpl->track->Duration() / 1000000.0);
	return buffer;
}

} // namespace ncm
