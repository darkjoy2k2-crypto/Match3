#include "genesis.h"

__attribute__((externally_visible))
const ROMHeader rom_header = {
#if (ENABLE_BANK_SWITCH != 0)
    "SEGA SSF        ",
#elif (MODULE_MEGAWIFI != 0)
    "SEGA MEGAWIFI   ",
#else
    "SEGA MEGA DRIVE ",
#endif
    "(C)Darkjoy..SGDK",
    "TETRIS HOMEBREW SGDK                            ",
    "TETRIS HOMEBREW SGDK                            ",
    "GM 00000000-00",
    0x000,
    "JD              ",
    0x00000000,
#if (ENABLE_BANK_SWITCH != 0)
    0x003FFFFF,
#else
    0x000FFFFF,
#endif
    0xE0FF0000,
    0xE0FFFFFF,
    "RA",               // SRAM vorhanden
    0xF820,             // Backup-Flags (Standard für SRAM)
    0x00200000,         // Start-Adresse (WICHTIG: 01 am Ende für Odd-Byte)
    0x002001FF,         // End-Adresse
    "            ",
    "VIBE CODE... SEE WHAT AI CAN DO FOR YOU ",
    "E               "
};
