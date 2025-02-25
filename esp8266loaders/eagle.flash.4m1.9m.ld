/* Flash Split for 4M chips with 128KB SPIFFS and Maximized Program + OTA */

MEMORY
{
  /* DPORT, DRAM, and IROM Segments */
  dport0_0_seg :  org = 0x3FF00000, len = 0x10
  dram0_0_seg :   org = 0x3FFE8000, len = 0x14000
  irom0_0_seg :   org = 0x40201010, len = 0xFEFF0
}

/* Partition Definitions */

/* OTA Slot 1: Starts at 0x10000 and takes ~1.9MB */
PROVIDE(_OTA1_start = 0x10000);    /* Start of OTA1 */
PROVIDE(_OTA1_end = 0x1F0000);     /* End of OTA1 (~1.9MB) */

/* OTA Slot 2: Starts at 0x1F0000 for OTA2 */
PROVIDE(_OTA2_start = 0x1F0000);    /* Start of OTA2 */
PROVIDE(_OTA2_end = 0x3D0000);     /* End of OTA2 (~1.9MB) */

/* SPIFFS (LittleFS): 128KB storage from 0x3D0000 to 0x3E0000 */
PROVIDE(_SPIFFS_start = 0x3D0000);  /* Start of SPIFFS */
PROVIDE(_SPIFFS_end = 0x3E0000);    /* End of SPIFFS (128KB) */
PROVIDE(_SPIFFS_page = 0x100);      /* Page size for SPIFFS */
PROVIDE(_SPIFFS_block = 0x2000);    /* Block size for SPIFFS */

/* System Data (WiFi, RF calibration, etc.) */
PROVIDE(_system_data_start = 0x3E0000); 
PROVIDE(_system_data_end = 0x3F0000);  /* 64KB for system data */

/* EEPROM (if needed) */
PROVIDE(_EEPROM_start = 0x405FB000); /* If EEPROM used, otherwise not needed */

/* Deprecated: SPIFFS symbols */
PROVIDE(_FS_start = 0x3D0000); 
PROVIDE(_FS_end = 0x3E0000);  
PROVIDE(_FS_page = 0x100);  
PROVIDE(_FS_block = 0x2000);  

/* Include the common linker script (if necessary) */
INCLUDE "local.eagle.app.v6.common.ld"