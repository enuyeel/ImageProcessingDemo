#include "misc.h"

#if defined (_MSC_VER) && (_DEBUG)

#include <strsafe.h> //StringCchPrintf()
#include <string.h> //strlen()
#include <stdlib.h> //mbstowcs_s()

void ErrorExit(unsigned line, LPCTSTR file, const char* msg, bool isExitProcess)
{
  LPVOID lpErrorBuf;
  LPVOID lpDisplayBuf;
  DWORD dw = GetLastError();
  //FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
  //              NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpErrorBuf, 0, NULL);

  //70 for the additional filler message
  //64 bit unsigned (line number) can at most take 20 spaces
  //lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (strlen(msg) + lstrlen((LPCTSTR)lpMsgBuf) + strlen(func) + strlen(file) + 70) * sizeof(TCHAR));
  lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (strlen(msg) + lstrlen((LPCTSTR)lpErrorBuf) + lstrlen(file) + 70) * sizeof(TCHAR));

  wchar_t wcstr[100];
  //Converts a sequence of multibyte characters to a corresponding sequence of wide characters.
  mbstowcs_s(
    0,          //* pReturnValue : If mbstowcs_s successfully converts the source string, it puts the size in wide characters of the converted string, 
                //*                including the null terminator, into *pReturnValue (provided pReturnValue is not NULL).
    wcstr,      //* wcstr : Address of buffer for the resulting converted wide character string.
    100,        //* sizeInWords : The size of the wcstr buffer in words.
    msg,        //* mbstr : The address of a sequence of null-terminated multibyte characters.
    _TRUNCATE); //* count : If count is the special value _TRUNCATE, then mbstowcs_s converts as much of the string as will fit into the destination buffer,
                //*         while still leaving room for a null terminator.

  //StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR),
  //                TEXT("%s\nfunction:%s\nline:%d\nfile:%s\nadditional_info:%s\nerrorcode:%d"),
  //                msg, func, line, file, lpMsgBuf, dw);
  StringCchPrintfW((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                   TEXT("%s\nLine Number : %d\nFile (Path) : %s\nAdditional Info : %s\nErrorcode : %d"),
                   wcstr, line, file, lpErrorBuf, dw);

  if (MessageBoxW(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Press 'Yes' to break."), MB_YESNO | MB_ICONERROR) == IDYES)
    DebugBreak();

  LocalFree(lpErrorBuf);
  LocalFree(lpDisplayBuf);

  if (isExitProcess) ExitProcess(dw);
}

#endif

#include <string>
#include <sstream>

#if defined (_MSC_VER) && (_DEBUG)
  #include <Winuser.h>
  #include <debugapi.h>
#endif

GLenum opengl_check_error()
{
  GLenum errorcode = glGetError();
  if (errorcode != GL_NO_ERROR)                                                       
  {                                                                                     
    std::stringstream ss;
    ss << errorcode;                             
    std::string errorstr("glGetError returns nonzero value : ");                      
    errorstr.append(ss.str());                                                        
    switch (errorcode)                                                                 
    {                                                                                 
      case GL_INVALID_ENUM:                                                           
        errorstr.append("\nGL_INVALID_ENUM"); break;
      case GL_INVALID_VALUE:                                                          
        errorstr.append("\nGL_INVALID_VALUE"); break;                                                                   
      case GL_INVALID_OPERATION:                                                      
        errorstr.append("\nGL_INVALID_OPERATION"); break;                                                                   
      case GL_INVALID_FRAMEBUFFER_OPERATION:                                          
        errorstr.append("\nGL_INVALID_FRAMEBUFFER_OPERATION"); break;                                                                       
      case GL_OUT_OF_MEMORY:                                                          
        errorstr.append("\nGL_OUT_OF_MEMORY"); break;
#if not defined (__EMSCRIPTEN__)
      case GL_STACK_UNDERFLOW:                                                        
        errorstr.append("\nGL_STACK_UNDERFLOW"); break;
      case GL_STACK_OVERFLOW:                                                         
        errorstr.append("\nGL_STACK_OVERFLOW"); break;
#endif
      default:                                                                       
        errorstr.append("\nError Undefined");                                         
    }

//https://web.archive.org/web/20210506154303/https://social.msdn.microsoft.com/Forums/en-US/0f749fd8-8a43-4580-b54b-fbf964d68375/convert-stdstring-to-lpcwstr-best-way-in-c?forum=Vsexpressvc#f59dfd52-580d-43a7-849f-9519d858a8e9
#if defined (_MSC_VER) && (_DEBUG)               
    //TODO: ANSI version should probably be enough for now.
    if (MessageBoxA(0, errorstr.c_str(), "Assert", (MB_OK | MB_ICONQUESTION)) == IDOK)     
      DebugBreak();
#else
    printf("%s\n", errorstr.c_str());
#endif 

  }                                                                                   
  return errorcode;
}
