
################## 环境准备区 #####################

CC: gcc  # c编译器
CXX: g++  # c++编译器

########### ↓↓↓ 重要! 这个wlroots的版本可能会随着上游更新发生变化, 需要随之修改 #############

WLROOTS_VERSION = 0.19

########### ↑↑↑ 重要! 这个wlroots的版本可能会随着上游更新发生变化, 需要随之修改 #############

WLROOTS_MODULE = wlroots-$(WLROOTS_VERSION)

# 创建构建目录(创建了则跳过)
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 环境变量: 需要将wlroots安装的位置纳入扫描, 因为wlroots安装的文件夹不是一般的pkg安装文件夹, 而是/usr/local/lin/pkgconfig
PKG_CONFIG_ENV = PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$$PKG_CONFIG_PATH

# C 编译选项
# -Wall("Warning all", 输出尽可能多的警告)
# -Wextra("Warning extra", 输出更多警告, 包括all不包括的警告)
# 这么多警告是为了方便排查错误
CFLAGS  = -Wall -Wextra

# C++ 编译选项
# 同上, 多一个-std指定C++版本
# 用C++17嘛
CXXFLAGS = -Wall -Wextra -std=c++17 $(shell $(PKG_CONFIG_ENV) pkg-config --cflags $(WLROOTS_MODULE))

# C/C++ 链接选项
LDFLAGS = $(shell $(PKG_CONFIG_ENV) pkg-config --libs $(WLROOTS_MODULE))

# 构建输出目录
BUILD_DIR = ../build

# 开发时运行库路径(和下面的run配套)
RUN_LIB_PATH_ENV = LD_LIBRARY_PATH=/usr/local/lib:$$LD_LIBRARY_PATH

################## make指令区 ####################

# 使用这个指令: 源文件 -> 目标文件 -> 链接 -> 可执行文件
tiley: tiley.c $(BUILD_DIR)
	$(CC) tiley.c -o $(BUILD_DIR)/tiley $(CFLAGS) $(LDFLAGS)

# C++版
tiley-cpp: tiley.cpp $(BUILD_DIR)
	$(CXX) tiley.cpp -o $(BUILD_DIR)/tiley $(CXXFLAGS) $(LDFLAGS)

# 使用这个指令: 源文件 -> 目标文件
tiley.o: tiley.c $(BUILD_DIR)
	$(C) -c tiley.c -o $(BUILD_DIR)/tiley.o $(CFLAGS) $(LDFLAGS)

# C++版
tiley.o-cpp: tiley.cpp $(BUILD_DIR)
	$(CXX) -c tiley.cpp -o $(BUILD_DIR)/tiley-cpp.o $(CXXFLAGS) $(LDFLAGS)

# 和tiley, tiley-cpp一起执行等价
all: tiley tiley-cpp

# 开发使用, 直接在make中运行, 避免环境集成。
# 但是我们需要在发布时使用其他方法，因为用户肯定需要libwlroots库, 用户不会在make中运行
run: $(BUILD_DIR)/tiley
	$(RUN_LIB_PATH_ENV) $(BUILD_DIR)/tiley
	