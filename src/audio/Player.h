#ifndef NCM_PLAYER_H
#define NCM_PLAYER_H

#include <string>

namespace ncm {

// Thin wrapper around BMediaFile/BMediaTrack/BSoundPlayer.
// Load() only decodes metadata, Play() needs a real output device - on a
// machine without one (a VMware guest for example) Play() reports B_ERROR and
// the caller should surface that to the user instead of hiding it.
class Player {
public:
	Player();
	~Player();

	bool Load(const std::string& path, std::string* error);
	bool Play(std::string* error);
	void Stop();

	bool IsLoaded() const;
	std::string Description() const;

private:
	struct Impl;
	Impl* fImpl;
};

} // namespace ncm

#endif
