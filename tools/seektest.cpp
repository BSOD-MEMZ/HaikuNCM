// Player 的 seek 与结束行为回归测试（在有声卡的 Haiku 上跑）
#include "../src/audio/Player.h"

#include <OS.h>

#include <cstdio>

using namespace ncm;

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: seektest <audio file>\n");
		return 1;
	}

	Player player;
	std::string error;
	if (!player.Load(argv[1], &error)) {
		fprintf(stderr, "load failed: %s\n", error.c_str());
		return 2;
	}
	printf("loaded: %s\n", player.Description().c_str());

	if (!player.Play(&error)) {
		fprintf(stderr, "play failed: %s\n", error.c_str());
		return 3;
	}
	snooze(2000000);
	printf("T+2s   pos=%.2f (expect ~2)\n", player.PositionSeconds());

	if (!player.Seek(120.0, &error)) {
		fprintf(stderr, "seek failed: %s\n", error.c_str());
		return 4;
	}
	printf("seek 120  pos=%.2f (expect ~120)\n", player.PositionSeconds());
	snooze(2000000);
	printf("T+2s   pos=%.2f (expect ~122)\n", player.PositionSeconds());

	const double duration = player.DurationSeconds();
	player.Seek(duration - 1.5, &error);
	printf("seek %.2f  pos=%.2f\n", duration - 1.5, player.PositionSeconds());

	// 等播完，确认 finished 且位置停住
	int checks = 0;
	while (!player.IsFinished() && checks < 60) {
		snooze(100000);
		checks++;
	}
	printf("finished=%d after %.1fs of waiting\n", player.IsFinished() ? 1 : 0,
		checks / 10.0);

	const double posA = player.PositionSeconds();
	snooze(1000000);
	const double posB = player.PositionSeconds();
	printf("pos after finish: %.2f -> %.2f (expect equal)\n", posA, posB);

	const bool ok = player.IsFinished() && posA == posB;
	printf("%s\n", ok ? "SEEKTEST_OK" : "SEEKTEST_FAIL");
	return ok ? 0 : 5;
}
