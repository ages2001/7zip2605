/* Synchronous file-drop transport for targets without native OLE2.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_FILE_DROP_PROTOCOL_H
#define Z7_FILE_DROP_PROTOCOL_H
#define Z7_DROP_PROTOCOL_PROPERTY "Z7.NT31.FileDrop.v1"
#define Z7_DROP_PROTOCOL_TAG 0x31504437UL
#define Z7_DROP_REPLY_TAG 0x5A000000UL
#define Z7_DROP_REPLY_ACCEPTED 0x00010000UL
#define Z7_DROP_MAX_BYTES 0x01000000UL
static UINT fileDropQueryMessage(void)
{
 static UINT message;
 if (!message) message = RegisterWindowMessageA("Z7.NT31.FileDrop.Query.v1");
 return message;
}
/* NT 3.1 does not expose another process's private window properties.
   Ask the registered window procedure instead; bound the wait for an
   ordinary legacy receiver which is not responding to messages. */
static DWORD fileDropCapabilities(HWND window)
{
 DWORD pid;
 DWORD_PTR reply = 0;
 UINT message;
 if (!window) return 0;
 GetWindowThreadProcessId(window, &pid);
 if (pid == GetCurrentProcessId())
  return (DWORD)(ULONG_PTR)GetPropA(window, Z7_DROP_PROTOCOL_PROPERTY) & 3;
 message = fileDropQueryMessage();
 if (!message || !SendMessageTimeoutA(window, message, Z7_DROP_PROTOCOL_TAG,
       0, SMTO_ABORTIFHUNG, 100, &reply)
     || (reply & 0xFFFFFF00UL) != Z7_DROP_REPLY_TAG) return 0;
 return (DWORD)reply & 3;
}
typedef struct {
 DWORD version, effects, keyState;
 POINTL point;
 DWORD dataSize;
} Z7FileDropPacket;
/* Wire layout of DROPFILES, independent of optional shell SDK headers. */
typedef struct {
 DWORD pFiles;
 POINT point;
 BOOL nonClient, fWide;
} Z7DropFilesHeader;
static DWORD fileDropEffect(DWORD allowed, DWORD keyState)
{
 allowed &= DROPEFFECT_COPY | DROPEFFECT_MOVE;
 if ((keyState & (MK_CONTROL | MK_SHIFT)) == (MK_CONTROL | MK_SHIFT)) return DROPEFFECT_NONE;
 if (keyState & MK_CONTROL) return allowed & DROPEFFECT_COPY;
 if (keyState & MK_SHIFT) return allowed & DROPEFFECT_MOVE;
 return allowed & DROPEFFECT_COPY ? DROPEFFECT_COPY : allowed & DROPEFFECT_MOVE;
}
/* Notify sources that the target has already performed a physical move.
   They must not delete the source a second time. */
static HRESULT fileDropPerformed(IDataObject *data, DWORD effect)
{
 FORMATETC format;
 STGMEDIUM medium;
 DWORD *value;
 HRESULT result;
 ZeroMemory(&format, sizeof(format)); ZeroMemory(&medium, sizeof(medium));
 format.cfFormat = (CLIPFORMAT)RegisterClipboardFormatA("Performed DropEffect");
 format.dwAspect = DVASPECT_CONTENT; format.lindex = -1; format.tymed = TYMED_HGLOBAL;
 if (!format.cfFormat) return E_FAIL;
 medium.tymed = TYMED_HGLOBAL; medium.hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
 if (!medium.hGlobal) return E_OUTOFMEMORY;
 value = GlobalLock(medium.hGlobal);
 if (!value) { GlobalFree(medium.hGlobal); return E_OUTOFMEMORY; }
 *value = effect; GlobalUnlock(medium.hGlobal);
 result = IDataObject_SetData(data, &format, &medium, TRUE);
 if (FAILED(result)) GlobalFree(medium.hGlobal);
 return result;
}
#endif
