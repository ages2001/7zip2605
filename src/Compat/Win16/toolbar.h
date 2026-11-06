/* Original small toolbar artwork on Windows 3.0-era GDI.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_TOOLBAR_H
#define Z7_WIN16_TOOLBAR_H
#include <windows.h>
typedef struct { HBITMAP color[7], mask[7]; BOOL ready; } Z7ToolbarCache;
BOOL Z7Win16_InitToolbarCache(Z7ToolbarCache FAR *cache,const BYTE FAR *pixels);
void Z7Win16_FreeToolbarCache(Z7ToolbarCache FAR *cache);
BOOL Z7Win16_DrawToolbarCache(const Z7ToolbarCache FAR *cache,unsigned index,HDC dc,int x,int y);
BOOL Z7Win16_InitToolbar(HINSTANCE instance);
void Z7Win16_FreeToolbar(void);
BOOL Z7Win16_DrawToolbar(HINSTANCE instance,const DRAWITEMSTRUCT FAR *item,UINT bitmapId);
#endif
