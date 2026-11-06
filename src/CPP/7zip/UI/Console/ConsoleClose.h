// ConsoleClose.h

#ifndef ZIP7_INC_CONSOLE_CLOSE_H
#define ZIP7_INC_CONSOLE_CLOSE_H

#ifdef Z7_WIN32S_CONSOLE
extern "C" void z7_win32s_console_pump(void);
#endif

namespace NConsoleClose {

// class CCtrlBreakException {};

#ifdef UNDER_CE

inline bool TestBreakSignal() { return false; }
struct CCtrlHandlerSetter {};

#else

extern unsigned g_BreakCounter;

inline bool TestBreakSignal()
{
  #ifdef Z7_WIN32S_CONSOLE
  z7_win32s_console_pump();
  #endif
  return (g_BreakCounter != 0);
}

class CCtrlHandlerSetter Z7_final
{
  #ifndef _WIN32
  void (*memo_sig_int)(int);
  void (*memo_sig_term)(int);
  #endif
public:
  CCtrlHandlerSetter();
  ~CCtrlHandlerSetter();
};

#endif

}

#endif
