#!/usr/bin/env bash
# FRIZE 로컬 컴파일 검증 (MSYS2 UCRT64 g++)
# 무거운 외부 의존(Paho/Crow/OpenCV/PCL/MAVSDK) 없이 컴파일 가능한 부분만 검증.
set -e
ROOT="/c/Users/wonma/Documents/FRIZE/software"
cd "$ROOT"
INC="-I libs/frize_common/include -I third_party"
echo "[1] frize_common 단위 테스트 빌드..."
g++ -std=c++17 -O2 -Wall -Wextra $INC \
    libs/frize_common/test/test_schemas.cpp -o build_test_common.exe
echo "[1] 실행:"
./build_test_common.exe
