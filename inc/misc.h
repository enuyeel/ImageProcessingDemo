#ifndef MISC_H
#define MISC_H

#if defined (__EMSCRIPTEN__)

  #include <webgl/webgl2.h>

#else

  #include <GL/glew.h> //GLenum

#endif

//* Strictly Windows specific.
#if defined (_MSC_VER) && (_DEBUG)

//* There are a number of child header files that are automatically included with windows.h. Many of these files cannot simply be included by themselves (they are not self-contained), because of dependencies.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <tchar.h>

//#define WIDE2(x) L##x
//#define WIDE1(x) WIDE2(x)
//#define WFUNCTION WIDE1(__FUNCTION__)

#endif

//* https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170
/*
  * __func__ (ISO C99 and ISO C++11), __LINE__, __FILE__
  TODO: __FILE__ (often) expands to full path, which might return non-ANSI characters.
*/
void ErrorExit(
  unsigned line, 
  LPCTSTR file, //__FILE__ The name of the current source file.
  const char* msg, 
  bool isExitProcess);

#define ERROR_MESSAGE(msg) ErrorExit(__LINE__, TEXT(__FILE__), msg, false)
#define ERROR_EXIT(msg) ErrorExit(__LINE__, TEXT(__FILE__), msg, true)
  
#else

//09/17/2018 | NOTE(yunhyeok): Put every error checks to sleep in release mode.
#define ERROR_MESSAGE(msg) do{}while(0)
#define ERROR_EXIT(msg) do{}while(0)

#endif

GLenum opengl_check_error();
//#define opengl_check_error() opengl_check_error()

#endif