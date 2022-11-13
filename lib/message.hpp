//
// Copy from: https://www.cnblogs.com/ziwuge/archive/2012/02/18/2357562.html
// Alternative solution(Dynamic): https://www.codeproject.com/Articles/7914/MessageBoxTimeout-API
//

#include <windows.h>

extern "C"
{

    int WINAPI MessageBoxTimeoutA(IN HWND hWnd, IN LPCSTR lpText, IN LPCSTR lpCaption, IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);

    int WINAPI MessageBoxTimeoutW(IN HWND hWnd, IN LPCWSTR lpText, IN LPCWSTR lpCaption, IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);

    void ShowMessage(LPCSTR msg, LPCSTR title)
    {
        MessageBoxTimeoutA(0, msg, title, 0, 0, 5000);
    }
};

#ifdef UNICODE
#define MessageBoxTimeout MessageBoxTimeoutW
#else
#define MessageBoxTimeout MessageBoxTimeoutA
#endif
