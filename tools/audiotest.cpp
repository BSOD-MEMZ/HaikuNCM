// Media Kit playback spike: decode an mp3 through BMediaTrack and push the
// samples to BSoundPlayer, the same way WebKit/MediaPlayer do it.
#include <Entry.h>
#include <MediaDefs.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <SoundPlayer.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

BMediaTrack* gTrack = nullptr;
float gPeak = 0.0f;
int64 gFrames = 0;
bool gFinished = false;

void PlayBuffer(void*, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	if (gTrack == nullptr || gFinished)
		return;

	int64 frames = static_cast<int64>(size
		/ (sizeof(float) * format.channel_count));
	media_header header;
	const status_t status = gTrack->ReadFrames(buffer, &frames, &header);
	if (status != B_OK || frames <= 0) {
		memset(buffer, 0, size);
		gFinished = true;
		return;
	}

	const float* samples = static_cast<float*>(buffer);
	const size_t count = frames * format.channel_count;
	for (size_t i = 0; i < count; i++) {
		const float value = fabsf(samples[i]);
		if (value > gPeak)
			gPeak = value;
	}
	gFrames += frames;
}

} // namespace

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: audiotest <audio file> [seconds]\n");
		return 1;
	}

	const int seconds = argc >= 3 ? atoi(argv[2]) : 5;

	BEntry entry(argv[1], true);
	entry_ref ref;
	if (entry.InitCheck() != B_OK || entry.GetRef(&ref) != B_OK) {
		fprintf(stderr, "cannot stat %s\n", argv[1]);
		return 2;
	}

	BMediaFile file(&ref);
	if (file.InitCheck() != B_OK) {
		fprintf(stderr, "BMediaFile init failed: %s (no decoder plugin?)\n",
			strerror(file.InitCheck()));
		return 3;
	}

	media_format format;
	memset(&format, 0, sizeof(format));
	format.type = B_MEDIA_RAW_AUDIO;
	format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;
	format.u.raw_audio.channel_count = 2;
	format.u.raw_audio.frame_rate = 44100;
	format.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
	format.u.raw_audio.buffer_size = 4096;

	printf("tracks: %d\n", file.CountTracks());
	for (int i = 0; i < file.CountTracks(); i++) {
		BMediaTrack* track = file.TrackAt(i);
		if (track == nullptr)
			continue;
		media_format negotiated = format;
		const status_t status = track->DecodedFormat(&negotiated);
		printf("  track %d: DecodedFormat -> %s\n", i, strerror(status));
		if (status == B_OK && negotiated.type == B_MEDIA_RAW_AUDIO) {
			gTrack = track;
			format = negotiated;
			printf("    %.0f Hz, %d ch, format=%d, duration=%.2fs\n",
				negotiated.u.raw_audio.frame_rate,
				(int)negotiated.u.raw_audio.channel_count,
				(int)negotiated.u.raw_audio.format,
				track->Duration() / 1000000.0);
			break;
		}
		file.ReleaseTrack(track);
	}

	if (gTrack == nullptr) {
		fprintf(stderr, "no decodable audio track\n");
		return 4;
	}

	BSoundPlayer player(&format.u.raw_audio, "ncm-audiotest", PlayBuffer,
		nullptr, nullptr);
	if (player.InitCheck() != B_OK) {
		fprintf(stderr, "BSoundPlayer init failed: %s\n",
			strerror(player.InitCheck()));
		return 5;
	}

	player.Start();
	player.SetHasData(true);

	for (int i = 0; i < seconds * 10 && !gFinished; i++)
		usleep(100000);

	player.Stop();
	printf("played %lld frames, peak amplitude %.3f, finished=%d\n", gFrames,
		gPeak, gFinished ? 1 : 0);
	return gFrames > 0 && gPeak > 0.01f ? 0 : 6;
}
