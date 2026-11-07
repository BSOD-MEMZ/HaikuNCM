// Decode any media file the system has a plugin for into a 16 bit PCM WAV.
// Used to verify the Media Kit decode path on machines without a usable
// sound card (for example a VMware guest with no Haiku driver).
#include <Entry.h>
#include <MediaDefs.h>
#include <MediaFile.h>
#include <MediaTrack.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

void WriteLE32(FILE* file, uint32_t value)
{
	unsigned char bytes[4] = {
		static_cast<unsigned char>(value & 0xff),
		static_cast<unsigned char>((value >> 8) & 0xff),
		static_cast<unsigned char>((value >> 16) & 0xff),
		static_cast<unsigned char>((value >> 24) & 0xff),
	};
	fwrite(bytes, 1, 4, file);
}

void WriteLE16(FILE* file, uint16_t value)
{
	unsigned char bytes[2] = {
		static_cast<unsigned char>(value & 0xff),
		static_cast<unsigned char>((value >> 8) & 0xff),
	};
	fwrite(bytes, 1, 2, file);
}

float Clamp(float value)
{
	if (value > 1.0f)
		return 1.0f;
	if (value < -1.0f)
		return -1.0f;
	return value;
}

} // namespace

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: wavdump <input media> <output.wav>\n");
		return 1;
	}

	BEntry entry(argv[1], true);
	entry_ref ref;
	if (entry.InitCheck() != B_OK || entry.GetRef(&ref) != B_OK) {
		fprintf(stderr, "cannot stat %s\n", argv[1]);
		return 2;
	}

	BMediaFile mediaFile(&ref);
	if (mediaFile.InitCheck() != B_OK) {
		fprintf(stderr, "BMediaFile init failed: %s\n",
			strerror(mediaFile.InitCheck()));
		return 3;
	}

	media_format format;
	memset(reinterpret_cast<void*>(&format), 0, sizeof(format));
	format.type = B_MEDIA_RAW_AUDIO;
	format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;
	format.u.raw_audio.channel_count = 2;
	format.u.raw_audio.frame_rate = 44100;
	format.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
	format.u.raw_audio.buffer_size = 4096;

	BMediaTrack* track = nullptr;
	for (int i = 0; i < mediaFile.CountTracks(); i++) {
		BMediaTrack* candidate = mediaFile.TrackAt(i);
		if (candidate == nullptr)
			continue;
		media_format negotiated = format;
		if (candidate->DecodedFormat(&negotiated) == B_OK
			&& negotiated.type == B_MEDIA_RAW_AUDIO) {
			track = candidate;
			format = negotiated;
			break;
		}
		mediaFile.ReleaseTrack(candidate);
	}

	if (track == nullptr) {
		fprintf(stderr, "no decodable audio track\n");
		return 4;
	}

	const int channels = static_cast<int>(format.u.raw_audio.channel_count);
	const uint32_t rate = static_cast<uint32_t>(format.u.raw_audio.frame_rate);
	printf("decoding: %d ch, %u Hz, duration %.2fs\n", channels, rate,
		track->Duration() / 1000000.0);

	FILE* out = fopen(argv[2], "wb");
	if (out == nullptr) {
		fprintf(stderr, "cannot open %s\n", argv[2]);
		return 5;
	}

	// header is patched once the real sample count is known
	fwrite("RIFF", 1, 4, out);
	WriteLE32(out, 0);
	fwrite("WAVEfmt ", 1, 8, out);
	WriteLE32(out, 16);
	WriteLE16(out, 1); // PCM
	WriteLE16(out, static_cast<uint16_t>(channels));
	WriteLE32(out, rate);
	WriteLE32(out, rate * channels * 2);
	WriteLE16(out, static_cast<uint16_t>(channels * 2));
	WriteLE16(out, 16);
	fwrite("data", 1, 4, out);
	WriteLE32(out, 0);

	const size_t framesPerRead = 4096;
	float* buffer = new float[framesPerRead * channels];
	int64_t totalFrames = 0;

	while (true) {
		int64 frames = static_cast<int64>(framesPerRead);
		const status_t status = track->ReadFrames(buffer, &frames, nullptr);
		if (status != B_OK || frames <= 0)
			break;

		for (int64 i = 0; i < frames * channels; i++) {
			const float clamped = Clamp(buffer[i]);
			const int16_t sample = static_cast<int16_t>(lrintf(clamped * 32767.0f));
			WriteLE16(out, static_cast<uint16_t>(sample));
		}
		totalFrames += frames;
	}

	delete[] buffer;

	const uint32_t dataBytes = static_cast<uint32_t>(totalFrames * channels * 2);
	fseek(out, 4, SEEK_SET);
	WriteLE32(out, dataBytes + 36);
	fseek(out, 40, SEEK_SET);
	WriteLE32(out, dataBytes);
	fclose(out);

	mediaFile.ReleaseTrack(track);
	printf("wrote %s: %lld frames, %.2fs\n", argv[2],
		static_cast<long long>(totalFrames),
		static_cast<double>(totalFrames) / rate);
	return totalFrames > 0 ? 0 : 6;
}
