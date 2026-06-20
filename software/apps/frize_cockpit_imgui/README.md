# FRIZE Cockpit (Dear ImGui) — ⏸ 보류(WIP)

지휘통제(C2) 콕핏을 Dear ImGui로 재설계 중인 WIP. **디자인 재작업은 보류** 상태.
실제 빌드/헤드리스 캡처는 동작한다(`build_and_shoot.sh` → PNG). 나중에 디자인 갈아엎고 재개.

```bash
IMGUI_DIR=/tmp/imgui ./build_and_shoot.sh out.png   # GLFW+GL+Xvfb 헤드리스 캡처
```
- `theme.hpp` — 다크 C2 디자인 시스템(팔레트/스타일/위젯)
- `main.cpp`  — 레이아웃 + 텍스처 + 오프스크린 캡처
