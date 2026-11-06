/* Draw the original indexed-color logo without the system icon loader's
   square-icon resizing. Uses only Windows 3.x-era resource and GDI APIs. */
#ifndef ZIP7_ORIGINAL_LOGO_H
#define ZIP7_ORIGINAL_LOGO_H
#define Z7_LOGO_WIDTH 110
#define Z7_LOGO_HEIGHT 63
static BOOL DrawOriginalLogo(HINSTANCE instance, UINT id, HDC dc, int x, int y)
{
  HRSRC resource;
  HGLOBAL loaded;
  const BYTE FAR *data;
  DWORD length, offset, stride;
  BOOL result = FALSE;
  HBITMAP colorBitmap, maskBitmap;
  HDC source;
  HGDIOBJ previous;
  struct { BITMAPINFOHEADER header; RGBQUAD colors[256]; } color;
  struct { BITMAPINFOHEADER header; RGBQUAD colors[2]; } mask;
  /* Win16's RT_ICON resource loader converts the DIB to a device-dependent
     icon before LockResource. RCDATA preserves the original file bytes. */
  resource = FindResource(instance, MAKEINTRESOURCE(id), RT_RCDATA);
  if (!resource) return FALSE;
  length = SizeofResource(instance, resource);
  loaded = LoadResource(instance, resource);
  data = (const BYTE FAR *)LockResource(loaded);
  if (!data || length < 22 + sizeof(color))
  { if (loaded) { if (data) UnlockResource(loaded); FreeResource(loaded); } return FALSE; }
  if (data[0] || data[1] || data[2] != 1 || data[3]
      || data[4] != 1 || data[5] || data[18] != 22
      || data[19] || data[20] || data[21])
  { UnlockResource(loaded); FreeResource(loaded); return FALSE; }
  data += 22;
  length -= 22;
  memcpy(&color, data, sizeof(color));
  if (color.header.biSize != sizeof(BITMAPINFOHEADER)
      || color.header.biWidth != Z7_LOGO_WIDTH
      || color.header.biHeight != Z7_LOGO_HEIGHT * 2
      || color.header.biPlanes != 1 || color.header.biBitCount != 8
      || color.header.biCompression != BI_RGB)
  { UnlockResource(loaded); FreeResource(loaded); return FALSE; }
  color.header.biHeight = Z7_LOGO_HEIGHT;
  stride = (Z7_LOGO_WIDTH + 3) & ~3;
  offset = sizeof(color) + stride * Z7_LOGO_HEIGHT;
  if (length < offset + ((Z7_LOGO_WIDTH + 31) / 32) * 4 * Z7_LOGO_HEIGHT)
  { UnlockResource(loaded); FreeResource(loaded); return FALSE; }
  color.header.biSizeImage = stride * Z7_LOGO_HEIGHT;
  memset(&mask, 0, sizeof(mask));
  mask.header.biSize = sizeof(BITMAPINFOHEADER);
  mask.header.biWidth = Z7_LOGO_WIDTH;
  mask.header.biHeight = Z7_LOGO_HEIGHT;
  mask.header.biPlanes = 1;
  mask.header.biBitCount = 1;
  mask.colors[1].rgbRed = mask.colors[1].rgbGreen = mask.colors[1].rgbBlue = 255;
  /* Some Windows 3.x display drivers cannot perform raster operations in
     StretchDIBits. Convert each plane, then use ordinary bitmap blits. */
  colorBitmap = CreateDIBitmap(dc, &color.header, CBM_INIT,
      data + sizeof(color), (BITMAPINFO FAR *)&color, DIB_RGB_COLORS);
  maskBitmap = CreateDIBitmap(dc, &mask.header, CBM_INIT,
      data + offset, (BITMAPINFO FAR *)&mask, DIB_RGB_COLORS);
  source = CreateCompatibleDC(dc);
  if (source && colorBitmap && maskBitmap)
  {
    previous = SelectObject(source, maskBitmap);
    result = BitBlt(dc, x, y, Z7_LOGO_WIDTH, Z7_LOGO_HEIGHT, source, 0, 0, SRCAND);
    SelectObject(source, colorBitmap);
    result = BitBlt(dc, x, y, Z7_LOGO_WIDTH, Z7_LOGO_HEIGHT, source, 0, 0, SRCINVERT) && result;
    SelectObject(source, previous);
  }
  if (source) DeleteDC(source);
  if (colorBitmap) DeleteObject(colorBitmap);
  if (maskBitmap) DeleteObject(maskBitmap);
  UnlockResource(loaded);
  FreeResource(loaded);
  return result;
}
#endif
