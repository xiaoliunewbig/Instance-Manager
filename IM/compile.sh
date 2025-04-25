#!/bin/bash
# 创建文件: compile.sh

# 设置并行编译 - 使用所有可用CPU核心
CORES=$(nproc)
echo "编译使用 $CORES 个核心"

# 安装必要工具(如果未安装)
if ! command -v ccache &> /dev/null; then
    echo "安装ccache..."
    sudo apt-get update
    sudo apt-get install -y ccache
fi

# 配置ccache
export PATH="/usr/lib/ccache:$PATH"
export CCACHE_DIR=$HOME/.ccache
export CC="ccache gcc"
export CXX="ccache g++"
ccache -M 10G  # 设置缓存大小为10GB

# 清理部分中间文件(保留配置)
make clean

# 使用并行编译
make -j$CORES

# 显示编译统计
echo "编译完成"
ccache -s  # 显示缓存统计
