#!/usr/bin/env bash
set -euo pipefail
ScriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${ScriptDir}/update.sh" "$@"
