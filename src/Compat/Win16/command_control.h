/* Cooperative native command-line cancellation and output error reporting.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_COMMAND_CONTROL_H
#define Z7_WIN16_COMMAND_CONTROL_H
#include <signal.h>
#ifdef __WATCOMC__
#include <conio.h>
#include <dos.h>
#endif
static volatile sig_atomic_t commandInterrupted;
#ifdef __WATCOMC__
static void (__interrupt __far *commandPreviousBreak)(void);
static int commandBreakInstalled;
/* The DOS runtime's SIGBREAK registration does not handle the BIOS Int 1Bh
   callback. Only set a flag here: DOS/file/heap calls are not reentrant in
   this interrupt. Normal progress callbacks perform transactional cleanup. */
static void __interrupt __far commandBiosBreak(void){commandInterrupted=1;}
static void commandEnd(void){
 if(commandBreakInstalled){
  _dos_setvect(0x1b,commandPreviousBreak);commandBreakInstalled=0;
 }
}
#endif
static void commandInterrupt(int number){
 commandInterrupted=1;
 signal(number,commandInterrupt);
}
static void commandBegin(void){
 commandInterrupted=0;
#ifdef __WATCOMC__
 commandPreviousBreak=_dos_getvect(0x1b);
#endif
 signal(SIGINT,commandInterrupt);
#if defined(SIGBREAK) && !defined(__WATCOMC__)
 signal(SIGBREAK,commandInterrupt);
#endif
#ifdef __WATCOMC__
 /* Int 1Bh is not one of the vectors DOS restores from the process's PSP.
    Register cleanup before replacing it, including for early main returns. */
 if(atexit(commandEnd)){
  fputs("ERROR: Cannot install command interruption cleanup.\n",stderr);
  exit(2);
 }
 _dos_setvect(0x1b,commandBiosBreak);commandBreakInstalled=1;
#endif
}
static int commandProgress(UInt64 position){
 (void)position;
#ifdef __WATCOMC__
 /* DOS checks Ctrl+C on console calls even while stdout is redirected.
    Status polling leaves ordinary queued keystrokes available to the shell. */
 (void)kbhit();
#endif
 return !commandInterrupted;
}
static int commandFinish(SRes result){
 if((fflush(stdout)||ferror(stdout))&&result==SZ_OK){
  fputs("ERROR: Cannot write command output.\n",stderr);
  result=SZ_ERROR_WRITE;
 }
#ifdef __WATCOMC__
 commandEnd();
#endif
 return result==SZ_OK?0:result==SZ_ERROR_PROGRESS?255:2;
}
#endif
