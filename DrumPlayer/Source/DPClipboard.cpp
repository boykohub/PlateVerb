// Drum Player: reads file paths from the Windows clipboard (a MIDI file copied in Explorer or another program).
// Kept in its own file so Windows headers never meet JUCE's.
#include <vector>
#include <string>
#ifdef _WIN32
 #define NOMINMAX
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #include <shellapi.h>
 #pragma comment (lib, "shell32.lib")
#endif

std::vector<std::wstring> dpClipboardFiles()
{
    std::vector<std::wstring> out;
   #ifdef _WIN32
    if (OpenClipboard (nullptr)) {
        if (HANDLE h = GetClipboardData (CF_HDROP)) {
            HDROP drop = (HDROP) h;
            const UINT n = DragQueryFileW (drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                wchar_t buf[2048] = {};
                if (DragQueryFileW (drop, i, buf, 2048) > 0) out.emplace_back (buf);
            }
        }
        CloseClipboard();
    }
   #endif
    return out;
}
