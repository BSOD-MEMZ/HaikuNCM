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
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(NET_SRC) $(UI_SRC) src/main.cpp -lbe -lmedia -ltranslation -llocalestub $(LDLIBS)

$(BUILD_DIR)/ncmcli: $(NET_SRC) tools/ncmcli.cpp third_party/nlohmann/json.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(NET_SRC) tools/ncmcli.cpp $(LDLIBS)

$(BUILD_DIR)/audiotest: tools/audiotest.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ tools/audiotest.cpp -lmedia -lbe -lroot

$(BUILD_DIR)/wavdump: tools/wavdump.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ tools/wavdump.cpp -lmedia -lbe -lroot

# 翻译：抽取 B_TRANSLATE 字符串，再链接成 Haiku 的二进制 catalog
TRANSLATABLE = $(UI_SRC)

# 翻译键用 tools/make_catkeys.py 抽（beta6 的 collectcatkeys 认不出展开后的 B_CATKEY）
CATALOG_SIG = x-vnd.HaikuNCM
CATALOG_LANG = zh-Hans

catkeys: $(BUILD_DIR)
	python3 tools/make_catkeys.py $(CATALOG_SIG) $(CATALOG_LANG) $(TRANSLATABLE) > $(BUILD_DIR)/$(CATALOG_LANG).catkeys

# 二进制 .catalog 需要 makecat 生成的校验和（beta6 未随开发包提供），
# 目前只产出翻译模板；等 makecat 可用后再补 linkcatkeys 这一步。

# 应用资源（签名、版本、图标）
rdef: $(BUILD_DIR)/HaikuNCM
	rc -o $(BUILD_DIR)/HaikuNCM.rsrc rdef/app.rdef
	xres -o $(BUILD_DIR)/HaikuNCM $(BUILD_DIR)/HaikuNCM.rsrc

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean catkeys rdef
