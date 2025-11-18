/******************************************************************************
 *  File        : sd_functions.c
 *  Author      : ControllersTech
 *  Website     : https://controllerstech.com
 *  Date        : June 26, 2025
 *  Updated on  : Sep 27, 2025
 *
 *  Description :
 *    This file is part of a custom STM32/Embedded tutorial series.
 *    For documentation, updates, and more examples, visit the website above.
 *
 *  Note :
 *    This code is written and maintained by ControllersTech.
 *    You are free to use and modify it for learning and development.
 ******************************************************************************/


#include "fatfs.h"
#include "sd_diskio_spi.h"
#include "sd_spi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "ffconf.h"

char sd_path[4];
FATFS fs;

int sd_get_space_kb(void) {
	FATFS *pfs;
	DWORD fre_clust, tot_sect, fre_sect, total_kb, free_kb;
	FRESULT res = f_getfree(sd_path, &fre_clust, &pfs);
	if (res != FR_OK) return res;

	tot_sect = (pfs->n_fatent - 2) * pfs->csize;
	fre_sect = fre_clust * pfs->csize;
	total_kb = tot_sect / 2;
	free_kb = fre_sect / 2;
	printf("💾 Total: %lu KB, Free: %lu KB\r\n", total_kb, free_kb);
	return FR_OK;
}

int sd_mount(void) {
	FRESULT res;
	extern uint8_t sd_is_sdhc(void);

	printf("Linking SD driver...\r\n");
	if (FATFS_LinkDriver(&SD_Driver, sd_path) != 0) {
		printf("FATFS_LinkDriver failed\n");
		return FR_DISK_ERR;
	}

	printf("Initializing disk...\r\n");
	DSTATUS stat = disk_initialize(0);
	if (stat != 0) {
		printf("disk_initialize failed: 0x%02X\n", stat);
		printf("FR_NOT_READY\tTry Hard Reset or Check Connection/Power\r\n");
		printf("Make sure \"MX_FATFS_Init\" is not being called in the main function\n"\
				"You need to disable its call in CubeMX->Project Manager->Advance Settings->Uncheck Generate code for MX_FATFS_Init\r\n");
		return FR_NOT_READY;
	}

	printf("Attempting mount at %s...\r\n", sd_path);
	res = f_mount(&fs, sd_path, 1);
	if (res == FR_OK)
	{
		printf("SD card mounted successfully at %s\r\n", sd_path);
		printf("Card Type: %s\r\n", sd_is_sdhc() ? "SDHC/SDXC" : "SDSC");

		// Capacity and free space reporting
		sd_get_space_kb();
		return FR_OK;
	}

	// Any other mount error
	printf("Mount failed with code: %d\r\n", res);
	return res;
}
