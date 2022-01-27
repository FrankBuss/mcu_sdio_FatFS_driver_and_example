/***************************************************************************//**
 * @file
 * @brief FAT example using FatFS for access to the MicroSD card on the SLWSTK.
 * @version 0.0.1
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Labs, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "em_chip.h"
#include "em_vdac.h"
#include "em_device.h"
#include "em_timer.h"
#include "em_cmu.h"
#include "bsp.h"
#include "bsp_trace.h"
#include "retargetserial.h"
#include "ff.h"
#include "sdio.h"
#include "diskio.h"

/* File system */
FATFS Fatfs;
FIL fh;
char path[100];

/* Command buffer and read data buffer */
#define CBUFSIZE    80
static char buffer[CBUFSIZE];
static UINT bufRead;
static char commandLine[CBUFSIZE];
static int commandIndex = 0;
volatile uint32_t msTicks; /* counts 1ms timeTicks */

void BSP_SLSTK3701A_SDIO_HWInit(void);
void Delay(uint32_t dlyTicks);

/***************************************************************************//**
 * @brief SysTick_Handler
 * Interrupt Service Routine for system tick counter
 ******************************************************************************/
void SysTick_Handler(void)
{
  msTicks++; /* increment counter necessary in Delay()*/
}

/***************************************************************************//**
 * @brief Delays number of msTick Systicks (typically 1 ms)
 * @param dlyTicks Number of ticks to delay
 ******************************************************************************/
void Delay(uint32_t dlyTicks)
{
  uint32_t curTicks;

  curTicks = msTicks;
  while ((msTicks - curTicks) < dlyTicks);
}

/***************************************************************************//**
 * @brief
 *   This function is required by the FAT file system in order to provide
 *   timestamps for created files. Since we do not have a reliable clock we
 *   hardcode a value here.
 *
 *   Refer to drivers/fatfs/doc/en/fattime.html for the format of this DWORD.
 * @return
 *    A DWORD containing the current time and date as a packed datastructure.
 ******************************************************************************/
DWORD get_fattime(void)
{
  return (28 << 25) | (2 << 21) | (1 << 16);
}

/***************************************************************************//**
 * @brief Output an array of characters
 * @param buf Pointer to string buffer
 * @param length Number of characters to output
 ******************************************************************************/
void PrintBuf(char *buf, int length)
{
  while (length--)
    putchar(*buf++);
}

/***************************************************************************//**
 * @brief Development board related HW configuration
 ******************************************************************************/
void BSP_SLSTK3701A_SDIO_HWInit(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Soldered sdCard slot
  GPIO_PinModeSet(gpioPortE, 7u, gpioModePushPull, 1);
  GPIO_PinOutSet(gpioPortE, 7u);

  GPIO_PinModeSet(gpioPortB, 10, gpioModeInput, 0);             // SDIO_CD
  GPIO_PinModeSet(gpioPortE, 15, gpioModePushPullAlternate, 0); // SDIO_CMD
  GPIO_PinModeSet(gpioPortE, 14, gpioModePushPullAlternate, 1); // SDIO_CLK
  GPIO_PinModeSet(gpioPortA, 0, gpioModePushPullAlternate, 1);  // SDIO_DAT0
  GPIO_PinModeSet(gpioPortA, 1, gpioModePushPullAlternate, 1);  // SDIO_DAT1
  GPIO_PinModeSet(gpioPortA, 2, gpioModePushPullAlternate, 1);  // SDIO_DAT2
  GPIO_PinModeSet(gpioPortA, 3, gpioModePushPullAlternate, 1);  // SDIO_DAT3
  GPIO_PinModeSet(gpioPortB, 9, gpioModePushPullAlternate, 0);  // WP
}

/***************************************************************************//**
 * @brief scan_files from FatFS documentation
 * @param path to traverse
 ******************************************************************************/
FRESULT scan_files(char* path, bool loption)
{
  FRESULT res;
  FILINFO fno;
  DIR dir;
  int i;
  char *fn;
#if _USE_LFN
  static char lfn[_MAX_LFN * (_DF1S ? 2 : 1) + 1];
  fno.lfname = lfn;
  fno.lfsize = sizeof(lfn);
#endif

  res = f_opendir(&dir, path);
  if (res == FR_OK)
  {
    i = strlen(path);
    for (;;)
    {
      res = f_readdir(&dir, &fno);
      if (res != FR_OK || fno.fname[0] == 0)
      {
        /* printf("f_readdir failure %d\n", res); */
        break;
      }
      if (fno.fname[0] == '.')
      {
        continue;
      }
#if _USE_LFN
      fn = *fno.lfname ? fno.lfname : fno.fname;
#else
      fn = fno.fname;
#endif
      if (fno.fattrib & AM_DIR)
      {
        if (loption)
        {
          char attrib[7];

          attrib[0] = '\0';
          if (fno.fattrib & AM_RDO)
          {
            strcat(attrib, "R");
          }
          if (fno.fattrib & AM_HID)
          {
            strcat(attrib, "H");
          }
          if (fno.fattrib & AM_SYS)
          {
            strcat(attrib, "S");
          }
          if (fno.fattrib & AM_LFN)
          {
            strcat(attrib, "L");
          }
          if (fno.fattrib & AM_DIR)
          {
            strcat(attrib, "D");
          }
          if (fno.fattrib & AM_ARC)
          {
            strcat(attrib, "A");
          }
          printf("%10u %s ", (unsigned int) fno.fsize, attrib);
        }
        if (strlen(path))
        {
          printf("%s/", path);
        }
        printf("%s\n", fn);
        sprintf(&path[i], "/%s", fn);
        res = scan_files(path, loption);
        if (res != FR_OK)
        {
          break;
        }
        path[i] = 0;
      } else
      {
        if (loption)
        {
          char attrib[7];

          attrib[0] = '\0';
          if (fno.fattrib & AM_RDO)
          {
            strcat(attrib, "R");
          }
          if (fno.fattrib & AM_HID)
          {
            strcat(attrib, "H");
          }
          if (fno.fattrib & AM_SYS)
          {
            strcat(attrib, "S");
          }
          if (fno.fattrib & AM_LFN)
          {
            strcat(attrib, "L");
          }
          if (fno.fattrib & AM_DIR)
          {
            strcat(attrib, "D");
          }
          if (fno.fattrib & AM_ARC)
          {
            strcat(attrib, "A");
          }
          printf("%10u %s ", (unsigned int) fno.fsize, attrib);
        }
        if (strlen(path))
        {
          printf("%s/", path);
        }
        printf("%s\n", fn);
      }
    }
  } else
  {
    printf("f_opendir failure %d\n", res);
  }

  return res;
}

/***************************************************************************//**
 * @brief  Main function
 ******************************************************************************/
int main(void)
{

  CHIP_Init();

  CMU_ClockEnable(cmuClock_HFPER, true);

  BSP_SLSTK3701A_SDIO_HWInit();

  /* If first word of user data page is non-zero, enable Energy Profiler trace */
  BSP_TraceProfilerSetup();

  /* Initialize LEUART/USART and map LF to CRLF */
  RETARGET_SerialInit();
  RETARGET_SerialCrLf(1);

  /* Setup SysTick Timer for 1 msec interrupts  */
  if (SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 1000))
  {
    while (1);
  }

  FRESULT res;
  int c;
  char *command;

  printf(
      "\nEFM32 FAT Console Example. Type \"h\" (+ Enter) for command list.\n");

  /* Initialize filesystem */
  res = f_mount(0, &Fatfs);
  if (res != FR_OK)
  {
    printf("FAT-mount failed: %d\n", res);
  }
  else
  {
    printf("FAT-mount successful\n");
  }

  /* Read command lines, and perform requested action */
  while (1)
  {
    /* Read line */
    printf("\n$ ");
    do
    {
      c = getchar();
      if (c == '\b')
      {
        printf("\b \b");
        if (commandIndex)
        {
          commandIndex--;
        }
      }
      else if (c > 0)
      {
        /* Local echo */
        putchar(c);
        commandLine[commandIndex] = c;
        commandIndex++;
      }
    } while ((c != '\r') && (c != '\n') && (commandIndex < (CBUFSIZE - 1)));
    commandLine[--commandIndex] = '\0';
    commandIndex = 0;
    if (strlen(commandLine) == 0)
    {
      continue;
    }

    /* Get command */
    command = strtok(commandLine, " ");

    /* HELP command */
    if (!strcmp(command, "h"))
    {
      printf("h                  - help\n");
      printf("ls [-l]            - list files\n");
      printf("rm <file>          - remove file\n");
      printf("mkdir <dirname>    - create dir\n");
      printf("cat <file>         - display file\n");
      printf("mv <old> <new>     - rename file\n");
      printf("mount              - mount file system\n");
      printf("umount             - unmount file system\n");
      continue;
    }

    /* LS command */
    if (!strcmp(command, "ls"))
    {
      bool loption = false;
      char *option = strtok(NULL, " ");
      if (option != NULL && !strcmp(option, "-l"))
      {
        loption = true;
      }
      strcpy(path, "");
      scan_files(path, loption);
      continue;
    }

    /* RM command */
    if (!strcmp(command, "rm"))
    {
      char *fileName = strtok(NULL, " ");
      if (fileName == NULL)
      {
        printf("usage: rm <file>");
        continue;
      }
      res = f_unlink(fileName);
      if (res != FR_OK)
      {
        printf("rm %s failed, error %u\n", fileName, res);
      }
      continue;
    }

    /* MV (rename) command */
    if (!strcmp(command, "mv"))
    {
      char *old = strtok(NULL, " ");
      char *new = strtok(NULL, " ");
      if ((old == NULL) || (new == NULL))
      {
        printf("usage: mv <old> <new>");
        continue;
      }
      res = f_rename(old, new);
      if (res != FR_OK)
      {
        printf("mv %s %s failed, error %u\n", old, new, res);
      }
      continue;
    }

    /* MKDIR command */
    if (!strcmp(command, "mkdir"))
    {
      char *fileName = strtok(NULL, " ");
      if (fileName == NULL)
      {
        printf("usage: mkdir <dirname>");
        continue;
      }
      res = f_mkdir(fileName);
      if (res != FR_OK)
      {
        printf("mkdir %s failed, error %u\n", fileName, res);
      }
      continue;
    }

    /* CAT text file */
    if (!strcmp(command, "cat"))
    {
      /* Get first argument */
      char *fileName = strtok(NULL, " ");

      if (fileName == (char *) NULL)
      {
        printf("cat: Missing argument\n");
        continue;
      }

      res = f_open(&fh, fileName, FA_READ);
      if (res == FR_OK)
      {
        printf("Content of file %s\n", fileName);
        printf("-----------------------------------------\n");
        res = f_read(&fh, buffer, CBUFSIZE, &bufRead);
        if (res == FR_OK)
        {
          PrintBuf(buffer, bufRead);
        } else
        {
          printf("Read Failure: %d\n", res);
        }
        f_close(&fh);
      } else
      {
        printf("Failed to open %s, error %u\n", fileName, res);
      }
      continue;
    }

    /* UNMOUNT drive */
    if (!strcmp(command, "umount"))
    {
      f_mount(0, NULL);
      continue;
    }

    /* MOUNT drive */
    if (!strcmp(command, "mount"))
    {
      res = f_mount(0, &Fatfs);
      if (res != FR_OK)
      {
        printf("FAT-mount failed: %d\n", res);
      }
      continue;
    }

    printf("Unknown command: %s, \"h\" for help.", command);
  } /* end of eternal while loop */
}
