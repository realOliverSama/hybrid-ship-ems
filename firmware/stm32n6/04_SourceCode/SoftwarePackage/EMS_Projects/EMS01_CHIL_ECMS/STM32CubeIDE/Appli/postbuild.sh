#!/bin/bash

set -euo pipefail
ProjectDir="$(cd "$(dirname "$0")" && pwd -P)"
ProjectOut="$(find "$ProjectDir" -name '*_Appli.bin' -print -quit)"
if [[ -z "${ProjectOut:-}" ]]; then
    echo "postbuild: no *_Appli.bin found under $ProjectDir" >&2
    exit 1
fi
mkdir -p "$ProjectDir/../../Binary"
arm-none-eabi-objcopy -I binary "$ProjectOut" --change-addresses 0x70100400 -O ihex "$ProjectDir/../../Binary/appli.hex"