// HaikuNCM protocol spike CLI. Used to validate the weapi/eapi pipeline on Haiku
// before any UI work starts.
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "net/netease.h"

namespace {

void PrintUsage()
{
	fprintf(stderr,
		"usage:\n"
		"  ncmcli search <keyword>\n"
		"  ncmcli url <song id> [output path]\n"
		"  ncmcli fetch <keyword> [output path]\n");
}

std::string DefaultCachePath(long long id, const std::string& type)
{
	const char* home = getenv("HOME");
	if (home == nullptr)
		home = "/boot/home";

	const std::string homeDirectory = std::string(home) + "/HaikuNCM";
	const std::string directory = homeDirectory + "/cache";
	mkdir(homeDirectory.c_str(), 0755);
	mkdir(directory.c_str(), 0755);

	return directory + "/" + std::to_string(id) + "."
		+ (type.empty() ? "mp3" : type);
}

void PrintSong(const ncm::Song& song, size_t index)
{
	printf("%2zu. %-12lld  %-32s  %-18s  fee=%d  %.0fs\n", index, song.id,
		song.name.c_str(), song.artist.c_str(), song.fee,
		song.durationMs / 1000.0);
}

int DoSearch(ncm::NeteaseApi& api, const std::string& keyword, int limit,
	std::vector<ncm::Song>* songs)
{
	std::string error;
	if (!api.Search(keyword, limit, songs, &error)) {
		fprintf(stderr, "search failed: %s\n", error.c_str());
		return 2;
	}

	printf("search \"%s\" -> %zu result(s)\n", keyword.c_str(), songs->size());
	for (size_t i = 0; i < songs->size(); i++)
		PrintSong((*songs)[i], i + 1);
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	if (argc < 2) {
		PrintUsage();
		return 1;
	}

	const std::string command = argv[1];
	ncm::NeteaseApi api;

	if (command == "search") {
		if (argc < 3) {
			PrintUsage();
			return 1;
		}
		std::vector<ncm::Song> songs;
		return DoSearch(api, argv[2], 10, &songs);
	}

	if (command == "url" || command == "fetch") {
		if (argc < 3) {
			PrintUsage();
			return 1;
		}

		ncm::Song target;
		if (command == "url") {
			target.id = atoll(argv[2]);
		} else {
			std::vector<ncm::Song> songs;
			if (int status = DoSearch(api, argv[2], 10, &songs); status != 0)
				return status;
			if (songs.empty()) {
				fprintf(stderr, "no result for \"%s\"\n", argv[2]);
				return 3;
			}
			// prefer a fully playable track so the download is not a 30s trial
			target = songs[0];
			for (const ncm::Song& song : songs) {
				if (song.fee == 0) {
					target = song;
					break;
				}
			}
			printf("picked: %s - %s (fee=%d)\n", target.name.c_str(),
				target.artist.c_str(), target.fee);
		}

		ncm::AudioUrl audio;
		std::string error;
		if (!api.SongUrl(target.id, "standard", &audio, &error)) {
			fprintf(stderr, "song url failed: %s\n", error.c_str());
			return 4;
		}
		printf("url: %s\n", audio.url.c_str());
		printf("type=%s  br=%d  size=%lld\n", audio.type.c_str(), audio.bitrate,
			audio.size);

		if (command == "url")
			return 0;

		const std::string path = argc >= 4 ? argv[3]
										   : DefaultCachePath(target.id, audio.type);
		if (!api.DownloadAudio(audio, path, &error)) {
			fprintf(stderr, "download failed: %s\n", error.c_str());
			return 5;
		}

		struct stat info;
		if (stat(path.c_str(), &info) == 0)
			printf("saved: %s (%lld bytes)\n", path.c_str(),
				static_cast<long long>(info.st_size));
		else
			printf("saved: %s\n", path.c_str());
		return 0;
	}

	PrintUsage();
	return 1;
}
