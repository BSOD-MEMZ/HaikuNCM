#ifndef NCM_HTTP_RANGE_SOURCE_H
#define NCM_HTTP_RANGE_SOURCE_H

#include <DataIO.h>
#include <Locker.h>

#include <map>
#include <string>
#include <vector>

// 流式音频数据源：把 HTTP Range 请求包装成 BPositionIO，
// 喂给 BMediaFile 边下边播（MediaPlayer 播网络流就是这条路）。
// 请求落到未缓存的块时同步阻塞拉取——解码线程阻塞没问题，
// BSoundPlayer 有缓冲垫着；seek 也一样，代价是那一下的网络往返。
class HttpRangeSource : public BPositionIO {
public:
	// expectedSize 可为 0（从 Content-Range 推）。失败时 error 带原因。
	HttpRangeSource(const std::string& url, long long expectedSize,
		std::string* error);
	~HttpRangeSource() override;

	bool Ok() const { return fOk; }
	long long Size() const { return fSize; }

	ssize_t ReadAt(off_t position, void* buffer, size_t size) override;
	ssize_t WriteAt(off_t position, const void* buffer,
		size_t size) override;
	off_t Seek(off_t position, uint32 seekMode) override;
	off_t Position() const override;

private:
	bool _FetchChunk(long long blockStart, std::string* error);
	void _EvictIfNeeded(long long keepNear);

	std::string fUrl;
	bool fOk = false;
	bool fRangeSupported = false;
	long long fSize = 0;
	off_t fPosition = 0;

	static const size_t kBlockSize = 64 * 1024;
	static const size_t kMaxBlocks = 128; // 内存里最多 8MB
	std::map<long long, std::vector<char> > fBlocks;

	BLocker fLock;
};

#endif
