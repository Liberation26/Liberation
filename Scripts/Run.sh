#!/usr/bin/env bash
set -euo pipefail

ScriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

clear || true

echo "[Liberation] Running update..."
"${ScriptDir}/update.sh"

action="${1:-}"
case "${action}" in
    D|d)
        exec "${ScriptDir}/RunDir.sh"
        ;;
    H|h)
        exec "${ScriptDir}/RunHD.sh"
        ;;
    I|i)
        exec "${ScriptDir}/RunISO.sh"
        ;;
    *)
        echo "[Liberation] Usage: ./Scripts/Run.sh {D|H|I}"
        echo "[Liberation]   D or d -> RunDir.sh"
        echo "[Liberation]   H or h -> RunHD.sh"
        echo "[Liberation]   I or i -> RunISO.sh"
        exit 1
        ;;
esac
