#!/usr/bin/env bash
# File Name: Run.sh
# File Version: 0.4.35
# Author: OpenAI
# Email: dave66samaa@gmail.com
# Operating System Name: Liberation OS
# Purpose: Automates Liberation OS build, packaging, runtime, or maintenance tasks.

set -euo pipefail

ScriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

clear || true

action="${1:-}"
update_flag="${2:-}"

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "[Liberation] Usage: ./Scripts/Run.sh {D|H|I} [U]"
    echo "[Liberation]   D or d -> RunDir.sh"
    echo "[Liberation]   H or h -> RunHD.sh"
    echo "[Liberation]   I or i -> RunISO.sh"
    echo "[Liberation]   U or u -> run update before the selected action"
    exit 1
fi

if [[ -n "${update_flag}" ]]; then
    case "${update_flag}" in
        U|u)
            echo "[Liberation] Running update..."
            if [[ -f "${ScriptDir}/update.sh" ]]; then
                bash "${ScriptDir}/update.sh"
            elif [[ -f "${ScriptDir}/Update.sh" ]]; then
                bash "${ScriptDir}/Update.sh"
            else
                echo "[Liberation] Error: update script not found."
                exit 1
            fi
            ;;
        *)
            echo "[Liberation] Usage: ./Scripts/Run.sh {D|H|I} [U]"
            echo "[Liberation] Error: second argument must be U or u when provided."
            exit 1
            ;;
    esac
fi

case "${action}" in
    D|d)
        exec bash "${ScriptDir}/RunDir.sh"
        ;;
    H|h)
        exec bash "${ScriptDir}/RunHD.sh"
        ;;
    I|i)
        exec bash "${ScriptDir}/RunISO.sh"
        ;;
    *)
        echo "[Liberation] Usage: ./Scripts/Run.sh {D|H|I} [U]"
        echo "[Liberation]   D or d -> RunDir.sh"
        echo "[Liberation]   H or h -> RunHD.sh"
        echo "[Liberation]   I or i -> RunISO.sh"
        echo "[Liberation]   U or u -> run update before the selected action"
        exit 1
        ;;
esac
