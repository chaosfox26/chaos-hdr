#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" && pwd)"
task_purge=0
case "${1:-}" in
    "") ;;
    --purge) task_purge=1 ;;
    --help|-h)
        echo "Usage: chaos-hdr-uninstall [--purge]"
        echo "Remove HDR services, effect, shortcut and menu entry. Keep saved profiles and project files."
        echo "--purge also permanently deletes the entire RTX HDR experimental folder, including settings and development archives."
        exit 0 ;;
    *) echo "Unknown uninstall option. Use --help."; exit 1 ;;
esac
if [[ $task_purge -eq 1 ]]; then
    [[ "$(basename -- "$task_root")" == 'RTX HDR experimental' ]] || { echo "Refusing to purge an unexpected folder."; exit 1; }
    [[ -f "$task_root/AGENTS.md" && -f "$task_root/install.py" && -f "$task_root/desktop.py" ]] || { echo "Project identity check failed; no files removed."; exit 1; }
fi
task_python="$task_root/.venv/bin/python"
if [[ ! -x "$task_python" ]]; then task_python="$(command -v python3)"; fi
QT_QPA_PLATFORM=offscreen "$task_python" "$task_root/install.py" --uninstall
if [[ $task_purge -eq 1 ]]; then
    cd -- "$(dirname -- "$task_root")"
    rm -rf -- "$task_root"
    echo "Removed the complete Chaos HDR project folder and saved settings."
fi
