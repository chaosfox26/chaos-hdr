#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$task_root"
if [[ $# -ne 0 ]]; then echo "Usage: ./bootstrap.sh"; exit 1; fi
mkdir -p output/scratch output/cache/uv output/dependencies/python
export TMPDIR="$task_root/output/scratch"
export UV_CACHE_DIR="$task_root/output/cache/uv"
export UV_PYTHON_INSTALL_DIR="$task_root/output/dependencies/python"
task_missing=0
for program in uv cmake pkg-config glslangValidator qdbus6 kwin_wayland kscreen-doctor notify-send; do
    command -v "$program" >/dev/null || task_missing=1
done
[[ -f /usr/include/kwin/effect/effect.h ]] || task_missing=1
for task_pkg in epoxy libdrm wayland-server Qt6Quick; do
    pkg-config --exists "$task_pkg" 2>/dev/null || task_missing=1
done
if [[ $task_missing -eq 1 ]]; then
    if command -v pacman >/dev/null && command -v pkexec >/dev/null; then
        echo "Installing required CachyOS/Arch build components. The system may request administrator authentication."
        task_packages=(base-devel cmake vulkan-headers glslang qt6-base qt6-declarative qt6-tools kglobalaccel kcoreaddons kconfig kwindowsystem kwin kscreen libepoxy libdrm wayland uv libnotify)
        pkexec pacman -S --needed --noconfirm "${task_packages[@]}"
    else
        echo "This installer targets CachyOS/Arch with KDE Wayland. Required: C++23 compiler, CMake, Vulkan headers, glslang, Qt6 Base/Declarative, KDE frameworks, matching KWin headers, libepoxy, libdrm, Wayland and uv."
        exit 1
    fi
fi
if [[ ! -x .venv/bin/python ]]; then
    uv venv --python 3.12 .venv
fi
if ! .venv/bin/python -c 'import importlib.metadata as m; from pathlib import Path; import sys; assert m.version("nvidia-vfx")=="0.2.0.0"; assert next(Path(sys.prefix).glob("lib/python*/site-packages/nvvfx/libs/libnvidia-ngx-truehdr.so.1.2.3"))' 2>/dev/null; then
    uv pip install --python .venv/bin/python 'nvidia-vfx==0.2.0.0' --extra-index-url https://pypi.nvidia.com/ --index-strategy unsafe-best-match
fi
.venv/bin/python prepare_desktop.py
.venv/bin/python install.py
