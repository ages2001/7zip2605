/* Test fixture: report the DOS kernel version independently of VER text. */
#include <dos.h>
#include <stdio.h>

int main(void)
{
  union REGS input, output;
  input.x.ax = 0x3000;
  intdos(&input, &output);
  printf("DOS kernel version %u.%02u (INT 21h/AH=30h)\n",
      (unsigned)output.h.al, (unsigned)output.h.ah);
  return output.h.al >= 5 ? 0 : 1;
}
