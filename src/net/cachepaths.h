#ifndef NCM_CACHEPATHS_H
#define NCM_CACHEPATHS_H

// 全局落盘位置：~/HaikuNCM 存登录态，cache/ 存封面、歌词、音频。
// 主窗口和歌手/专辑页都要用，放头文件里省得复制。

#include <cstdlib>
#include <string>
#include <sys/stat.h>

namespace ncm {

inline std::string EnsureDir(const std::string& path)
{
	mkdir(path.c_str(), 0755);
	return path;
}

inline std::string SessionDir()
{
	const char* home = getenv("HOME");
	if (home == nullptr)
		home = "/boot/home";
	return EnsureDir(std::string(home) + "/HaikuNCM");
}

inline std::string CacheDir()
{
	return EnsureDir(SessionDir() + "/cache");
}

inline std::string CoverDir()
{
	return EnsureDir(CacheDir() + "/covers");
}

inline std::string CoverPath(long long id)
{
	return CoverDir() + "/" + std::to_string(id) + ".jpg";
}

inline std::string BigCoverPath(long long id)
{
	return CacheDir() + "/" + std::to_string(id) + "_big.jpg";
}

inline std::string LyricPath(long long id)
{
	return CacheDir() + "/" + std::to_string(id) + ".lrc";
}

inline std::string TranslationPath(long long id)
{
	return CacheDir() + "/" + std::to_string(id) + ".tlyric";
}

inline std::string RomajiPath(long long id)
{
	return CacheDir() + "/" + std::to_string(id) + ".romalrc";
}

inline std::string CommentsPath(long long id)
{
	return CacheDir() + "/" + std::to_string(id) + ".comments";
}

inline std::string AudioPath(long long id, const std::string& type)
{
	return CacheDir() + "/" + std::to_string(id) + "."
		+ (type.empty() ? "mp3" : type);
}

inline std::string SessionPath()
{
	return SessionDir() + "/session.txt";
}

} // namespace ncm

#endif
