CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra
INCLUDES = -Isrc -Ithird_party
LDLIBS = -lcurl -lssl -lcrypto -lnetwork

NET_SRC = $(wildcard src/net/*.cpp)
UI_SRC = $(wildcard src/ui/*.cpp src/audio/*.cpp)
BUILD_DIR = build

all: $(BUILD_DIR)/HaikuNCM $(BUILD_DIR)/ncmcli $(BUILD_DIR)/audiotest $(BUILD_DIR)/wavdump

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/HaikuNCM: $(NET_SRC) $(UI_SRC) src/main.cpp third_party/nlohmann/json.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(NET_SRC) $(UI_SRC) src/main.cpp -lbe -lmedia -ltranslation $(LDLIBS)

$(BUILD_DIR)/ncmcli: $(NET_SRC) tools/ncmcli.cpp third_party/nlohmann/json.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(NET_SRC) tools/ncmcli.cpp $(LDLIBS)

$(BUILD_DIR)/audiotest: tools/audiotest.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ tools/audiotest.cpp -lmedia -lbe -lroot

$(BUILD_DIR)/wavdump: tools/wavdump.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ tools/wavdump.cpp -lmedia -lbe -lroot

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
