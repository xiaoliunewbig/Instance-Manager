#!/bin/bash

# 设置编译线程数
MAKE_JOBS=6

# 显示帮助信息
show_help() {
    echo "用法: $0 [选项]"
    echo "选项:"
    echo "  -c, --clean      清理构建目录"
    echo "  -r, --run        构建后运行测试"
    echo "  -t, --test-type  测试类型 (0=基本测试, 1=性能测试, 2=全部测试, -1=仅诊断)"
    echo "  -d, --diagnose   只运行诊断 (等同于 -r -t -1)"
    echo "  -s, --server     服务器地址 (默认: localhost:50051)"
    echo "  -j, --jobs       编译线程数 (默认: 6)"
    echo "  -h, --help       显示帮助信息"
    exit 0
}

# 设置默认值
CLEAN=0
RUN=0
TEST_TYPE=0
SERVER="localhost:50051"
DIAGNOSE_ONLY=0

# 处理命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN=1
            shift
            ;;
        -r|--run)
            RUN=1
            shift
            ;;
        -t|--test-type)
            TEST_TYPE="$2"
            shift 2
            ;;
        -d|--diagnose)
            DIAGNOSE_ONLY=1
            RUN=1
            TEST_TYPE="-1"
            shift
            ;;
        -s|--server)
            SERVER="$2"
            shift 2
            ;;
        -j|--jobs)
            MAKE_JOBS="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo "未知选项: $1"
            show_help
            ;;
    esac
done

# 设置颜色
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # 无颜色

# 显示当前目录
echo -e "${YELLOW}当前目录: $(pwd)${NC}"

# 如果需要，清理构建目录
if [ $CLEAN -eq 1 ]; then
    echo -e "${YELLOW}正在清理构建目录...${NC}"
    rm -rf build
fi

# 创建构建目录
mkdir -p build
cd build || { echo -e "${RED}无法进入构建目录${NC}"; exit 1; }

# 运行 CMake
echo -e "${YELLOW}正在配置项目...${NC}"
cmake .. || { echo -e "${RED}CMake 配置失败${NC}"; exit 1; }

# 编译项目
echo -e "${YELLOW}正在使用 $MAKE_JOBS 个线程编译项目...${NC}"
make -j$MAKE_JOBS || { echo -e "${RED}编译失败${NC}"; exit 1; }

echo -e "${GREEN}编译成功!${NC}"

# 运行测试
if [ $RUN -eq 1 ]; then
    if [ $DIAGNOSE_ONLY -eq 1 ]; then
        echo -e "${YELLOW}运行服务器诊断...${NC}"
        echo -e "${YELLOW}服务器地址: $SERVER${NC}"
        
        ./test_cases "$SERVER" -1 || { echo -e "${RED}诊断失败${NC}"; exit 1; }
    else
        echo -e "${YELLOW}运行测试用例...${NC}"
        echo -e "${YELLOW}服务器地址: $SERVER${NC}"
        echo -e "${YELLOW}测试类型: $TEST_TYPE${NC}"
        
        ./test_cases "$SERVER" "$TEST_TYPE" || { echo -e "${RED}测试失败${NC}"; exit 1; }
    fi
    
    echo -e "${GREEN}测试完成!${NC}"
fi

echo -e "${GREEN}完成!${NC}"
exit 0 