/*
 * NXCTRL BeagleBone Black Control Library
 *
 * Connection Information App Program
 *
 * Copyright (C) 2014 Sungjin Chun <chunsj@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <ctype.h>
#include <NXCTRL_appEx.h>

#define FONT_WIDTH                  6
#define FONT_HEIGHT                 8
#define MENU_SEL_CHAR               ((unsigned char)16)

#define DPY_IDLE_COUNT_MAX          300
#define MIN_ACTION_DURATION         200

#define MENU_IDX_COUNT              4

#define MENU_IDX_SYSTEM_MENU        0
#define MENU_IDX_RELOAD_MENU        1
#define MENU_IDX_PING_GW_MENU       2
#define MENU_IDX_EXIT_MENU          3

static NXCTRL_BOOL                  MENU_U_BUTTON_STATE = NXCTRL_LOW;
static NXCTRL_BOOL                  MENU_D_BUTTON_STATE = NXCTRL_LOW;
static NXCTRL_BOOL                  EXEC_BUTTON_STATE = NXCTRL_LOW;
static unsigned int                 DPY_IDLE_COUNT = 0;
static unsigned char                MENU_IDX = MENU_IDX_SYSTEM_MENU;
static NXCTRL_BOOL                  IN_MENU = NXCTRL_FALSE;
static unsigned long long           LAST_ACTION_TIME = 0;

static NXCTRL_BOOL
canAction (NXCTRL_VOID) {
  struct timespec tm;
  unsigned long long timeInMillis;
  extern int clock_gettime(int, struct timespec *);
  clock_gettime(_POSIX_CPUTIME, &tm);
  timeInMillis = tm.tv_sec * 1000 + tm.tv_nsec/1000000;
  if ((timeInMillis - LAST_ACTION_TIME) > MIN_ACTION_DURATION) {
    LAST_ACTION_TIME = timeInMillis;
    return NXCTRL_TRUE;
  } else
    return NXCTRL_FALSE;
}

static NXCTRL_BOOL
getMacAddress (char *pszIFName, char *pszMacIP) {
  char rchIFName[BUFSIZ];
  FILE *pFile;
  int i;

  if (!pszIFName || !pszMacIP || strlen(pszIFName) == 0)
    return NXCTRL_FALSE;

  memset(pszMacIP, 0, 17);

  sprintf(rchIFName, "/sys/class/net/%s/address", pszIFName);
  pFile = fopen(rchIFName, "r");
  if (!pFile)
    return NXCTRL_FALSE;

  fscanf(pFile, "%s", pszMacIP);
  fclose(pFile);
  for (i = 0; i < 17 /* strlen(pszMacIP) */; i++)
    pszMacIP[i] = toupper(pszMacIP[i]);
  return NXCTRL_TRUE;
}

static NXCTRL_BOOL
getWIFIInfo (int *pnLink, int *pnLevel, int *pnNoise) {
  FILE *pFile = fopen("/proc/net/wireless", "r");
  char rch[1024];
  char rchLabel[8];

  *pnLink = *pnLevel = *pnNoise = 0;
  
  if (!pFile)
    return NXCTRL_FALSE;

  while (fgets(rch, 1023, pFile)) {
    sscanf(rch, "%s", rchLabel);
    if (!strcmp(rchLabel, "wlan0:")) {
      sscanf(rch, "%s %s %d. %d. %d.", rchLabel, rchLabel, pnLink, pnLevel, pnNoise);
    }
  }
  fclose(pFile);
  
  return NXCTRL_TRUE;
}

static NXCTRL_BOOL
getDefaultGW (char *pszGW) {
  FILE *pFile = fopen("/proc/net/route", "r");
  char rch[1024];
  char rchIFace[32], rchDest[32], rchGW[32];
  NXCTRL_BOOL bFound = NXCTRL_OFF;
  int i0, i1, i2, i3;

  if (!pszGW)
    return NXCTRL_FALSE;
  
  if (!pFile)
    return NXCTRL_FALSE;
  
  while (fgets(rch, 1023, pFile)) {
    sscanf(rch, "%s %s %s", rchIFace, rchDest, rchGW);
    if (!strcmp(rchDest, "00000000")) {
      bFound = NXCTRL_ON;
      break;
    }
  }
  fclose(pFile);

  if (!bFound)
    return NXCTRL_FALSE;

  sscanf(rchGW, "%2x%2x%2x%2x", &i0, &i1, &i2, &i3);
  snprintf(pszGW, 15, "%d.%d.%d.%d", i3, i2, i1, i0);

  return NXCTRL_TRUE;
}

static NXCTRL_VOID
pingToDefaultGW (NXCTRL_VOID) {
  char rch[1024], rchGW[32];
  memset(rchGW, 0, 32);
  if (getDefaultGW(rchGW)) {
    sprintf(rch, "ping -c 1 -W 1 %s >& /dev/null", rchGW);
    system(rch);
  }
}

static NXCTRL_VOID
displayConnInfo (LPNXCTRLAPP pApp) {
  struct ifaddrs *ifaddr, *ifa;
  int n;
  char rchHost[NI_MAXHOST];
  char rchBuffer[1024];
  char rchGW[32];
  char rchMacIP[20];
  int nLink, nLevel, nNoise;

  pApp->clearDisplay();
  pApp->setCursor(0, 0);

  if (getifaddrs(&ifaddr) == -1) {
    pApp->writeSTR("ERROR IN CONN INFO");
    pApp->updateDisplay();
    return;
  }

  pApp->setCursor(FONT_WIDTH*3, 0);
  pApp->writeSTR("CONNECTION INFO");

  pApp->setCursor(0, FONT_HEIGHT + 8);

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) continue;
    n = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                    rchHost, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if (n == 0 && strcmp("lo", ifa->ifa_name)) {
      memset(rchBuffer, 0, 1024);
      sprintf(rchBuffer, "%5s: %s\n", ifa->ifa_name, rchHost);
      pApp->writeSTR(rchBuffer);
    }
  }
  if (getDefaultGW(rchGW)) {
    sprintf(rchBuffer, "%5s: %s\n", "gw", rchGW);
    pApp->writeSTR(rchBuffer);
  }

  if (getWIFIInfo(&nLink, &nLevel, &nNoise)) {
    int nCount;
    sprintf(rchBuffer, "%d/%d/%d", nLink, nLevel, nNoise);
    nCount = (21 - strlen(rchBuffer))/2;
    nCount = nCount < 0 ? 0 : nCount;
    pApp->setCursor(nCount*FONT_WIDTH, 47);
    pApp->writeSTR(rchBuffer);
  }

  pApp->setCursor(0, 57);
  if (getMacAddress("wlan0", rchMacIP)) {
    sprintf(rchBuffer, "  %s", rchMacIP);
    pApp->writeSTR(rchBuffer);
  }

  pApp->updateDisplay();
}

static NXCTRL_VOID
updateMenuButtonState (LPNXCTRLAPP pApp) {
  if (MENU_U_BUTTON_STATE == NXCTRL_LOW) {
    if (pApp->digitalRead(MENU_U_BUTTON_BANK, MENU_U_BUTTON_PIN) == NXCTRL_HIGH) {
      MENU_U_BUTTON_STATE = NXCTRL_HIGH;
      DPY_IDLE_COUNT = 0;
    }
  } else {
    if (pApp->digitalRead(MENU_U_BUTTON_BANK, MENU_U_BUTTON_PIN) == NXCTRL_LOW) {
      MENU_U_BUTTON_STATE = NXCTRL_LOW;
      DPY_IDLE_COUNT = 0;
    }
  }

  if (MENU_D_BUTTON_STATE == NXCTRL_LOW) {
    if (pApp->digitalRead(MENU_D_BUTTON_BANK, MENU_D_BUTTON_PIN) == NXCTRL_HIGH) {
      MENU_D_BUTTON_STATE = NXCTRL_HIGH;
      DPY_IDLE_COUNT = 0;
    }
  } else {
    if (pApp->digitalRead(MENU_D_BUTTON_BANK, MENU_D_BUTTON_PIN) == NXCTRL_LOW) {
      MENU_D_BUTTON_STATE = NXCTRL_LOW;
      DPY_IDLE_COUNT = 0;
    }
  }
}

static NXCTRL_VOID
updateExecButtonState (LPNXCTRLAPP pApp) {
  if (EXEC_BUTTON_STATE == NXCTRL_LOW) {
    if (pApp->digitalRead(EXEC_BUTTON_BANK, EXEC_BUTTON_PIN) == NXCTRL_HIGH) {
      EXEC_BUTTON_STATE = NXCTRL_HIGH;
      DPY_IDLE_COUNT = 0;
    }
  } else {
    if (pApp->digitalRead(EXEC_BUTTON_BANK, EXEC_BUTTON_PIN) == NXCTRL_LOW) {
      EXEC_BUTTON_STATE = NXCTRL_LOW;
      DPY_IDLE_COUNT = 0;
    }
  }
}

static char *
mkMenuSTR (char *rch, const char *pszName, int nMenu) {
  sprintf(rch, "%c %s\n",
          (MENU_IDX == nMenu ? MENU_SEL_CHAR : ' '),
          pszName);
  return rch;
}

static NXCTRL_VOID
displayMenu (LPNXCTRLAPP pApp) {
  char rch[21];

  pApp->clearDisplay();

  pApp->setCursor(0, 0);
  pApp->writeSTR("CONNECTION");
  pApp->drawLine(61, 6, 127, 6, NXCTRL_ON);
  pApp->setCursor(0, 16);

  pApp->writeSTR(mkMenuSTR(rch, "TC>>", MENU_IDX_SYSTEM_MENU));
  pApp->writeSTR(mkMenuSTR(rch, "RELOAD INFO", MENU_IDX_RELOAD_MENU));
  pApp->writeSTR(mkMenuSTR(rch, "PING TO GW", MENU_IDX_PING_GW_MENU));
  pApp->writeSTR(mkMenuSTR(rch, "EXIT MENU", MENU_IDX_EXIT_MENU));

  pApp->updateDisplay();
}

NXCTRL_VOID
NXCTRLAPP_init (LPNXCTRLAPP pApp) {
  MENU_U_BUTTON_STATE = pApp->digitalRead(MENU_U_BUTTON_BANK, MENU_U_BUTTON_PIN);
  MENU_D_BUTTON_STATE = pApp->digitalRead(MENU_D_BUTTON_BANK, MENU_D_BUTTON_PIN);
  EXEC_BUTTON_STATE = pApp->digitalRead(EXEC_BUTTON_BANK, EXEC_BUTTON_PIN);
  DPY_IDLE_COUNT = 0;
  MENU_IDX = MENU_IDX_SYSTEM_MENU;
  IN_MENU = NXCTRL_FALSE;
  LAST_ACTION_TIME = 0;

  while (MENU_U_BUTTON_STATE == NXCTRL_HIGH) {
    pApp->sleep(100, 0);
    MENU_U_BUTTON_STATE = pApp->digitalRead(MENU_U_BUTTON_BANK, MENU_U_BUTTON_PIN);
  }

  while (MENU_D_BUTTON_STATE == NXCTRL_HIGH) {
    pApp->sleep(100, 0);
    MENU_D_BUTTON_STATE = pApp->digitalRead(MENU_D_BUTTON_BANK, MENU_D_BUTTON_PIN);
  }

  pApp->clearDisplay();
  pApp->updateDisplay();
  displayConnInfo(pApp);
}

NXCTRL_VOID
NXCTRLAPP_clean (LPNXCTRLAPP pApp) {
}

NXCTRL_VOID
NXCTRLAPP_run (LPNXCTRLAPP pApp) {
  updateMenuButtonState(pApp);
  updateExecButtonState(pApp);

  if (MENU_U_BUTTON_STATE != NXCTRL_HIGH && MENU_D_BUTTON_STATE != NXCTRL_HIGH &&
      EXEC_BUTTON_STATE != NXCTRL_HIGH) {
    DPY_IDLE_COUNT++;
    if (DPY_IDLE_COUNT > DPY_IDLE_COUNT_MAX) {
      pApp->nCmd = 2;
      return;
    }
    return;
  }

  if (MENU_U_BUTTON_STATE == NXCTRL_ON || MENU_D_BUTTON_STATE == NXCTRL_ON) {
    if (IN_MENU) {
      if (canAction()) {
        if (MENU_D_BUTTON_STATE == NXCTRL_ON) {
          if (MENU_IDX < MENU_IDX_COUNT - 1)
            MENU_IDX++;
          else
            MENU_IDX = 0;
        } else {
          if (MENU_IDX > 0)
            MENU_IDX--;
          else
            MENU_IDX = MENU_IDX_COUNT - 1;
        }
        displayMenu(pApp);
      }
    } else {
      IN_MENU = NXCTRL_TRUE;
      displayMenu(pApp);
      canAction();
    }
  }

  if (EXEC_BUTTON_STATE == NXCTRL_ON) {
    if (IN_MENU) {
      if (canAction()) {
        switch (MENU_IDX) {
        case MENU_IDX_EXIT_MENU:
          IN_MENU = NXCTRL_FALSE;
          displayConnInfo(pApp);
          break;
        case MENU_IDX_SYSTEM_MENU:
          pApp->nCmd = 1;
          return;
        case MENU_IDX_RELOAD_MENU:
          IN_MENU = NXCTRL_FALSE;
          displayConnInfo(pApp);
          break;
        case MENU_IDX_PING_GW_MENU:
          pApp->clearDisplay();
          pApp->setCursor(FONT_WIDTH*5, FONT_HEIGHT*3);
          pApp->writeSTR("PING TO GW...");
          pApp->updateDisplay();
          pingToDefaultGW();
          IN_MENU = NXCTRL_FALSE;
          displayConnInfo(pApp);
          break;
        default:
          break;
        }
      }
    }
  }
}
