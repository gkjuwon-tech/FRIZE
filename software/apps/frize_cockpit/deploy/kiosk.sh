#!/usr/bin/env bash
# FRIZE CONSOLE-1 키오스크 런처 ― Qt EGLFS 전체화면(데스크톱 없음)
set -e
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_HIDECURSOR=1
export QT_QPA_EGLFS_ALWAYS_SET_MODE=1
export FRIZE_CORE_WS="${FRIZE_CORE_WS:-ws://localhost:8000/ws/cockpit}"
# 화면보호기/블랭킹 방지(현장 상시 표시)
( command -v xset >/dev/null && xset s off -dpms ) 2>/dev/null || true
exec /usr/local/bin/frize_cockpit
