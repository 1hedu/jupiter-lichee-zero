# PINE A64+ 2GB: use the firmware built by build_uboot.sh.
# Its CONFIG_ARMV8_SWITCH_TO_EL1 is mandatory for this ARM32 uImage.
# Code remains below0x43000000; stage files above0x48000000.
# High BSS is NOLOAD at0x50000000; USB DMA at0x58000000.
setenv jupiter_image_addr 0x48000000
setenv jupiter_dtb_addr 0x47000000
setenv bootm_low 0x40000000
setenv bootm_size 0x08000000
setenv fdt_high 0x47ffffff

if load mmc 0:1 ${jupiter_image_addr} jupiter.uImage; then
    if load mmc 0:1 ${jupiter_dtb_addr} sun50i-a64-pine64-plus.dtb; then
        echo "Jupiter PINE64 MENU 4fb5904e / 46 demos: bootm -> AArch32 EL1"
        bootm ${jupiter_image_addr} - ${jupiter_dtb_addr}
    else
        echo "Missing sun50i-a64-pine64-plus.dtb on first FAT partition"
    fi
else
    echo "Missing jupiter.uImage on first FAT partition"
fi
echo "Jupiter did not start. Check the serial log and PINE64 boot guide."
