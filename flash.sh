#!/bin/bash
set -e

PROJECT_DIR="/home/du/STM32CubeIDE/workspace_1.19.0/FOMO_H7"
ELF_PATH="$PROJECT_DIR/Debug/FOMO_H7.elf"
PROG="/home/du/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.linux64_2.2.200.202503041107/tools/bin/STM32_Programmer_CLI"
STLINK_SN="E1007200D0D2139393740544"

if [ ! -f "$ELF_PATH" ]; then
    echo "[-] Error: $ELF_PATH does not exist. Please build first."
    exit 1
fi

echo "[*] Connecting ST-LINK SN: $STLINK_SN and flashing $ELF_PATH..."
"$PROG" -c port=SWD freq=1000 sn="$STLINK_SN" -w "$ELF_PATH" -v -rst
echo "[+] Flash & Reset successful!"
