/*
 * NXCTRL BeagleBone Black Control Library
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

#ifndef __NXCTRL_OLED_HEADER__
#define __NXCTRL_OLED_HEADER__

#include <NXCTRL.h>
#include <inttypes.h>

#define OLED_WIDTH     128
#define OLED_HEIGHT    64

#define OLED_BUF_SIZE  (OLED_WIDTH*OLED_HEIGHT/8)

typedef struct __tagNXCTRLOLED {
  NXCTRL_UINT8 nDCBank, nDCPin;
  NXCTRL_UINT8 nRSTBank, nRSTPin;
  int nSPIFD;
  NXCTRL_UINT8 rchBuf[OLED_BUF_SIZE];
} NXCTRLOLED;

NXCTRL_VOID NXCTRLOLEDInit (NXCTRLOLED *pOLED,
                            NXCTRL_UINT8 nDCBank, NXCTRL_UINT8 nDCPin,
                            NXCTRL_UINT8 nRSTBank, NXCTRL_UINT8 nRSTPin,
                            int nSPIFD);
NXCTRL_VOID NXCTRLOLEDDisplayNormal (NXCTRLOLED *pOLED);
NXCTRL_VOID NXCTRLOLEDDisplayInverse (NXCTRLOLED *pOLED);

#endif /* __NXCTRL_OLED_HEADER__ */
