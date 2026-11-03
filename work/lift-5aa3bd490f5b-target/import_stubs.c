/* Import bridge stubs - manual implementation needed */

#include "recomp_types.h"


/* ===== ADVAPI32.dll ===== */

/* 0x0067C000 */ void bridge_CryptAcquireContextA(void) {
    /* TODO: implement ADVAPI32.dll::CryptAcquireContextA */
    STUB("ADVAPI32.dll::CryptAcquireContextA");
}

/* 0x0067C004 */ void bridge_RegOpenKeyExA(void) {
    /* TODO: implement ADVAPI32.dll::RegOpenKeyExA */
    STUB("ADVAPI32.dll::RegOpenKeyExA");
}

/* 0x0067C008 */ void bridge_RegQueryValueExA(void) {
    /* TODO: implement ADVAPI32.dll::RegQueryValueExA */
    STUB("ADVAPI32.dll::RegQueryValueExA");
}

/* 0x0067C00C */ void bridge_RegCloseKey(void) {
    /* TODO: implement ADVAPI32.dll::RegCloseKey */
    STUB("ADVAPI32.dll::RegCloseKey");
}

/* 0x0067C010 */ void bridge_CryptGenRandom(void) {
    /* TODO: implement ADVAPI32.dll::CryptGenRandom */
    STUB("ADVAPI32.dll::CryptGenRandom");
}

/* 0x0067C014 */ void bridge_CryptReleaseContext(void) {
    /* TODO: implement ADVAPI32.dll::CryptReleaseContext */
    STUB("ADVAPI32.dll::CryptReleaseContext");
}


/* ===== DINPUT8.dll ===== */

/* 0x006B2220 */ void bridge_DirectInput8Create(void) {
    /* TODO: implement DINPUT8.dll::DirectInput8Create */
    STUB("DINPUT8.dll::DirectInput8Create");
}


/* ===== DSOUND.dll ===== */

/* 0x006B2228 */ void bridge_ordinal_11(void) {
    /* TODO: implement DSOUND.dll::ordinal_11 */
    STUB("DSOUND.dll::ordinal_11");
}


/* ===== GDI32.dll ===== */

/* 0x0067C02C */ void bridge_DeleteObject(void) {
    /* TODO: implement GDI32.dll::DeleteObject */
    STUB("GDI32.dll::DeleteObject");
}


/* ===== KERNEL32.dll ===== */

/* 0x006B2238 */ void bridge_GetStringTypeW(void) {
    /* TODO: implement KERNEL32.dll::GetStringTypeW */
    STUB("KERNEL32.dll::GetStringTypeW");
}

/* 0x006B223C */ void bridge_GetStringTypeA(void) {
    /* TODO: implement KERNEL32.dll::GetStringTypeA */
    STUB("KERNEL32.dll::GetStringTypeA");
}

/* 0x006B2240 */ void bridge_GetSystemInfo(void) {
    /* TODO: implement KERNEL32.dll::GetSystemInfo */
    STUB("KERNEL32.dll::GetSystemInfo");
}

/* 0x006B2244 */ void bridge_VirtualProtect(void) {
    /* TODO: implement KERNEL32.dll::VirtualProtect */
    STUB("KERNEL32.dll::VirtualProtect");
}

/* 0x006B2248 */ void bridge_GetLocaleInfoA(void) {
    /* TODO: implement KERNEL32.dll::GetLocaleInfoA */
    STUB("KERNEL32.dll::GetLocaleInfoA");
}

/* 0x006B224C */ void bridge_GetCPInfo(void) {
    /* TODO: implement KERNEL32.dll::GetCPInfo */
    STUB("KERNEL32.dll::GetCPInfo");
}

/* 0x006B2250 */ void bridge_GetOEMCP(void) {
    /* TODO: implement KERNEL32.dll::GetOEMCP */
    STUB("KERNEL32.dll::GetOEMCP");
}

/* 0x006B2254 */ void bridge_GetACP(void) {
    /* TODO: implement KERNEL32.dll::GetACP */
    STUB("KERNEL32.dll::GetACP");
}

/* 0x006B2258 */ void bridge_IsBadCodePtr(void) {
    /* TODO: implement KERNEL32.dll::IsBadCodePtr */
    STUB("KERNEL32.dll::IsBadCodePtr");
}

/* 0x006B225C */ void bridge_IsBadReadPtr(void) {
    /* TODO: implement KERNEL32.dll::IsBadReadPtr */
    STUB("KERNEL32.dll::IsBadReadPtr");
}

/* 0x006B2260 */ void bridge_VirtualQuery(void) {
    /* TODO: implement KERNEL32.dll::VirtualQuery */
    STUB("KERNEL32.dll::VirtualQuery");
}

/* 0x006B2264 */ void bridge_InterlockedExchange(void) {
    /* TODO: implement KERNEL32.dll::InterlockedExchange */
    STUB("KERNEL32.dll::InterlockedExchange");
}

/* 0x006B2268 */ void bridge_SetFilePointer(void) {
    /* TODO: implement KERNEL32.dll::SetFilePointer */
    STUB("KERNEL32.dll::SetFilePointer");
}

/* 0x006B226C */ void bridge_Sleep(void) {
    /* TODO: implement KERNEL32.dll::Sleep */
    STUB("KERNEL32.dll::Sleep");
}

/* 0x006B2270 */ void bridge_SetCurrentDirectoryA(void) {
    /* TODO: implement KERNEL32.dll::SetCurrentDirectoryA */
    STUB("KERNEL32.dll::SetCurrentDirectoryA");
}

/* 0x006B2274 */ void bridge_SetStdHandle(void) {
    /* TODO: implement KERNEL32.dll::SetStdHandle */
    STUB("KERNEL32.dll::SetStdHandle");
}

/* 0x006B2278 */ void bridge_GetModuleHandleA(void) {
    /* TODO: implement KERNEL32.dll::GetModuleHandleA */
    STUB("KERNEL32.dll::GetModuleHandleA");
}

/* 0x006B227C */ void bridge_GetTickCount(void) {
    /* TODO: implement KERNEL32.dll::GetTickCount */
    STUB("KERNEL32.dll::GetTickCount");
}

/* 0x006B2280 */ void bridge_FreeLibrary(void) {
    /* TODO: implement KERNEL32.dll::FreeLibrary */
    STUB("KERNEL32.dll::FreeLibrary");
}

/* 0x006B2284 */ void bridge_GetProcAddress(void) {
    /* TODO: implement KERNEL32.dll::GetProcAddress */
    STUB("KERNEL32.dll::GetProcAddress");
}

/* 0x006B2288 */ void bridge_LoadLibraryA(void) {
    /* TODO: implement KERNEL32.dll::LoadLibraryA */
    STUB("KERNEL32.dll::LoadLibraryA");
}

/* 0x006B228C */ void bridge_GetVersionExA(void) {
    /* TODO: implement KERNEL32.dll::GetVersionExA */
    STUB("KERNEL32.dll::GetVersionExA");
}

/* 0x006B2290 */ void bridge_SetEnvironmentVariableA(void) {
    /* TODO: implement KERNEL32.dll::SetEnvironmentVariableA */
    STUB("KERNEL32.dll::SetEnvironmentVariableA");
}

/* 0x006B2294 */ void bridge_DeleteCriticalSection(void) {
    /* TODO: implement KERNEL32.dll::DeleteCriticalSection */
    STUB("KERNEL32.dll::DeleteCriticalSection");
}

/* 0x006B2298 */ void bridge_EnterCriticalSection(void) {
    /* TODO: implement KERNEL32.dll::EnterCriticalSection */
    STUB("KERNEL32.dll::EnterCriticalSection");
}

/* 0x006B229C */ void bridge_TryEnterCriticalSection(void) {
    /* TODO: implement KERNEL32.dll::TryEnterCriticalSection */
    STUB("KERNEL32.dll::TryEnterCriticalSection");
}

/* 0x006B22A0 */ void bridge_CompareStringA(void) {
    /* TODO: implement KERNEL32.dll::CompareStringA */
    STUB("KERNEL32.dll::CompareStringA");
}

/* 0x006B22A4 */ void bridge_CloseHandle(void) {
    /* TODO: implement KERNEL32.dll::CloseHandle */
    STUB("KERNEL32.dll::CloseHandle");
}

/* 0x006B22A8 */ void bridge_ResumeThread(void) {
    /* TODO: implement KERNEL32.dll::ResumeThread */
    STUB("KERNEL32.dll::ResumeThread");
}

/* 0x006B22AC */ void bridge_CreateThread(void) {
    /* TODO: implement KERNEL32.dll::CreateThread */
    STUB("KERNEL32.dll::CreateThread");
}

/* 0x006B22B0 */ void bridge_GetSystemDirectoryA(void) {
    /* TODO: implement KERNEL32.dll::GetSystemDirectoryA */
    STUB("KERNEL32.dll::GetSystemDirectoryA");
}

/* 0x006B22B4 */ void bridge_GetModuleFileNameA(void) {
    /* TODO: implement KERNEL32.dll::GetModuleFileNameA */
    STUB("KERNEL32.dll::GetModuleFileNameA");
}

/* 0x006B22B8 */ void bridge_SetEndOfFile(void) {
    /* TODO: implement KERNEL32.dll::SetEndOfFile */
    STUB("KERNEL32.dll::SetEndOfFile");
}

/* 0x006B22BC */ void bridge_SetUnhandledExceptionFilter(void) {
    /* TODO: implement KERNEL32.dll::SetUnhandledExceptionFilter */
    STUB("KERNEL32.dll::SetUnhandledExceptionFilter");
}

/* 0x006B22C0 */ void bridge_FlushFileBuffers(void) {
    /* TODO: implement KERNEL32.dll::FlushFileBuffers */
    STUB("KERNEL32.dll::FlushFileBuffers");
}

/* 0x006B22C4 */ void bridge_LCMapStringW(void) {
    /* TODO: implement KERNEL32.dll::LCMapStringW */
    STUB("KERNEL32.dll::LCMapStringW");
}

/* 0x006B22C8 */ void bridge_LCMapStringA(void) {
    /* TODO: implement KERNEL32.dll::LCMapStringA */
    STUB("KERNEL32.dll::LCMapStringA");
}

/* 0x006B22CC */ void bridge_GetTimeZoneInformation(void) {
    /* TODO: implement KERNEL32.dll::GetTimeZoneInformation */
    STUB("KERNEL32.dll::GetTimeZoneInformation");
}

/* 0x006B22D0 */ void bridge_IsBadWritePtr(void) {
    /* TODO: implement KERNEL32.dll::IsBadWritePtr */
    STUB("KERNEL32.dll::IsBadWritePtr");
}

/* 0x006B22D4 */ void bridge_VirtualAlloc(void) {
    /* TODO: implement KERNEL32.dll::VirtualAlloc */
    STUB("KERNEL32.dll::VirtualAlloc");
}

/* 0x006B22D8 */ void bridge_VirtualFree(void) {
    /* TODO: implement KERNEL32.dll::VirtualFree */
    STUB("KERNEL32.dll::VirtualFree");
}

/* 0x006B22DC */ void bridge_HeapCreate(void) {
    /* TODO: implement KERNEL32.dll::HeapCreate */
    STUB("KERNEL32.dll::HeapCreate");
}

/* 0x006B22E0 */ void bridge_HeapDestroy(void) {
    /* TODO: implement KERNEL32.dll::HeapDestroy */
    STUB("KERNEL32.dll::HeapDestroy");
}

/* 0x006B22E4 */ void bridge_GetFileType(void) {
    /* TODO: implement KERNEL32.dll::GetFileType */
    STUB("KERNEL32.dll::GetFileType");
}

/* 0x006B22E8 */ void bridge_SetHandleCount(void) {
    /* TODO: implement KERNEL32.dll::SetHandleCount */
    STUB("KERNEL32.dll::SetHandleCount");
}

/* 0x006B22EC */ void bridge_GetEnvironmentStringsW(void) {
    /* TODO: implement KERNEL32.dll::GetEnvironmentStringsW */
    STUB("KERNEL32.dll::GetEnvironmentStringsW");
}

/* 0x006B22F0 */ void bridge_WideCharToMultiByte(void) {
    /* TODO: implement KERNEL32.dll::WideCharToMultiByte */
    STUB("KERNEL32.dll::WideCharToMultiByte");
}

/* 0x006B22F4 */ void bridge_FreeEnvironmentStringsW(void) {
    /* TODO: implement KERNEL32.dll::FreeEnvironmentStringsW */
    STUB("KERNEL32.dll::FreeEnvironmentStringsW");
}

/* 0x006B22F8 */ void bridge_GetEnvironmentStrings(void) {
    /* TODO: implement KERNEL32.dll::GetEnvironmentStrings */
    STUB("KERNEL32.dll::GetEnvironmentStrings");
}

/* 0x006B22FC */ void bridge_FreeEnvironmentStringsA(void) {
    /* TODO: implement KERNEL32.dll::FreeEnvironmentStringsA */
    STUB("KERNEL32.dll::FreeEnvironmentStringsA");
}

/* 0x006B2300 */ void bridge_UnhandledExceptionFilter(void) {
    /* TODO: implement KERNEL32.dll::UnhandledExceptionFilter */
    STUB("KERNEL32.dll::UnhandledExceptionFilter");
}

/* 0x006B2304 */ void bridge_GetStdHandle(void) {
    /* TODO: implement KERNEL32.dll::GetStdHandle */
    STUB("KERNEL32.dll::GetStdHandle");
}

/* 0x006B2308 */ void bridge_LeaveCriticalSection(void) {
    /* TODO: implement KERNEL32.dll::LeaveCriticalSection */
    STUB("KERNEL32.dll::LeaveCriticalSection");
}

/* 0x006B230C */ void bridge_CompareStringW(void) {
    /* TODO: implement KERNEL32.dll::CompareStringW */
    STUB("KERNEL32.dll::CompareStringW");
}

/* 0x006B2310 */ void bridge_InitializeCriticalSection(void) {
    /* TODO: implement KERNEL32.dll::InitializeCriticalSection */
    STUB("KERNEL32.dll::InitializeCriticalSection");
}

/* 0x006B2314 */ void bridge_GetFileAttributesA(void) {
    /* TODO: implement KERNEL32.dll::GetFileAttributesA */
    STUB("KERNEL32.dll::GetFileAttributesA");
}

/* 0x006B2318 */ void bridge_HeapSize(void) {
    /* TODO: implement KERNEL32.dll::HeapSize */
    STUB("KERNEL32.dll::HeapSize");
}

/* 0x006B231C */ void bridge_GetCurrentProcessId(void) {
    /* TODO: implement KERNEL32.dll::GetCurrentProcessId */
    STUB("KERNEL32.dll::GetCurrentProcessId");
}

/* 0x006B2320 */ void bridge_RaiseException(void) {
    /* TODO: implement KERNEL32.dll::RaiseException */
    STUB("KERNEL32.dll::RaiseException");
}

/* 0x006B2324 */ void bridge_GetSystemTimeAsFileTime(void) {
    /* TODO: implement KERNEL32.dll::GetSystemTimeAsFileTime */
    STUB("KERNEL32.dll::GetSystemTimeAsFileTime");
}

/* 0x006B2328 */ void bridge_HeapReAlloc(void) {
    /* TODO: implement KERNEL32.dll::HeapReAlloc */
    STUB("KERNEL32.dll::HeapReAlloc");
}

/* 0x006B232C */ void bridge_HeapFree(void) {
    /* TODO: implement KERNEL32.dll::HeapFree */
    STUB("KERNEL32.dll::HeapFree");
}

/* 0x006B2330 */ void bridge_HeapAlloc(void) {
    /* TODO: implement KERNEL32.dll::HeapAlloc */
    STUB("KERNEL32.dll::HeapAlloc");
}

/* 0x006B2334 */ void bridge_GetCommandLineA(void) {
    /* TODO: implement KERNEL32.dll::GetCommandLineA */
    STUB("KERNEL32.dll::GetCommandLineA");
}

/* 0x006B2338 */ void bridge_GetStartupInfoA(void) {
    /* TODO: implement KERNEL32.dll::GetStartupInfoA */
    STUB("KERNEL32.dll::GetStartupInfoA");
}

/* 0x006B233C */ void bridge_TerminateProcess(void) {
    /* TODO: implement KERNEL32.dll::TerminateProcess */
    STUB("KERNEL32.dll::TerminateProcess");
}

/* 0x006B2340 */ void bridge_RtlUnwind(void) {
    /* TODO: implement KERNEL32.dll::RtlUnwind */
    STUB("KERNEL32.dll::RtlUnwind");
}

/* 0x006B2344 */ void bridge_ExitProcess(void) {
    /* TODO: implement KERNEL32.dll::ExitProcess */
    STUB("KERNEL32.dll::ExitProcess");
}

/* 0x006B2348 */ void bridge_GetCurrentProcess(void) {
    /* TODO: implement KERNEL32.dll::GetCurrentProcess */
    STUB("KERNEL32.dll::GetCurrentProcess");
}

/* 0x006B234C */ void bridge_GetProcessAffinityMask(void) {
    /* TODO: implement KERNEL32.dll::GetProcessAffinityMask */
    STUB("KERNEL32.dll::GetProcessAffinityMask");
}

/* 0x006B2350 */ void bridge_SetProcessAffinityMask(void) {
    /* TODO: implement KERNEL32.dll::SetProcessAffinityMask */
    STUB("KERNEL32.dll::SetProcessAffinityMask");
}

/* 0x006B2354 */ void bridge_WaitForSingleObject(void) {
    /* TODO: implement KERNEL32.dll::WaitForSingleObject */
    STUB("KERNEL32.dll::WaitForSingleObject");
}

/* 0x006B2358 */ void bridge_SetThreadPriority(void) {
    /* TODO: implement KERNEL32.dll::SetThreadPriority */
    STUB("KERNEL32.dll::SetThreadPriority");
}

/* 0x006B235C */ void bridge_CreateEventA(void) {
    /* TODO: implement KERNEL32.dll::CreateEventA */
    STUB("KERNEL32.dll::CreateEventA");
}

/* 0x006B2360 */ void bridge_SetEvent(void) {
    /* TODO: implement KERNEL32.dll::SetEvent */
    STUB("KERNEL32.dll::SetEvent");
}

/* 0x006B2364 */ void bridge_ResetEvent(void) {
    /* TODO: implement KERNEL32.dll::ResetEvent */
    STUB("KERNEL32.dll::ResetEvent");
}

/* 0x006B2368 */ void bridge_WaitForMultipleObjects(void) {
    /* TODO: implement KERNEL32.dll::WaitForMultipleObjects */
    STUB("KERNEL32.dll::WaitForMultipleObjects");
}

/* 0x006B236C */ void bridge_SetFilePointerEx(void) {
    /* TODO: implement KERNEL32.dll::SetFilePointerEx */
    STUB("KERNEL32.dll::SetFilePointerEx");
}

/* 0x006B2370 */ void bridge_ReadFile(void) {
    /* TODO: implement KERNEL32.dll::ReadFile */
    STUB("KERNEL32.dll::ReadFile");
}

/* 0x006B2374 */ void bridge_CreateFileA(void) {
    /* TODO: implement KERNEL32.dll::CreateFileA */
    STUB("KERNEL32.dll::CreateFileA");
}

/* 0x006B2378 */ void bridge_DebugBreak(void) {
    /* TODO: implement KERNEL32.dll::DebugBreak */
    STUB("KERNEL32.dll::DebugBreak");
}

/* 0x006B237C */ void bridge_QueryPerformanceCounter(void) {
    /* TODO: implement KERNEL32.dll::QueryPerformanceCounter */
    STUB("KERNEL32.dll::QueryPerformanceCounter");
}

/* 0x006B2380 */ void bridge_QueryPerformanceFrequency(void) {
    /* TODO: implement KERNEL32.dll::QueryPerformanceFrequency */
    STUB("KERNEL32.dll::QueryPerformanceFrequency");
}

/* 0x006B2384 */ void bridge_MultiByteToWideChar(void) {
    /* TODO: implement KERNEL32.dll::MultiByteToWideChar */
    STUB("KERNEL32.dll::MultiByteToWideChar");
}

/* 0x006B2388 */ void bridge_GetTimeFormatA(void) {
    /* TODO: implement KERNEL32.dll::GetTimeFormatA */
    STUB("KERNEL32.dll::GetTimeFormatA");
}

/* 0x006B238C */ void bridge_GetDateFormatA(void) {
    /* TODO: implement KERNEL32.dll::GetDateFormatA */
    STUB("KERNEL32.dll::GetDateFormatA");
}

/* 0x006B2390 */ void bridge_GetUserDefaultLangID(void) {
    /* TODO: implement KERNEL32.dll::GetUserDefaultLangID */
    STUB("KERNEL32.dll::GetUserDefaultLangID");
}

/* 0x006B2394 */ void bridge_ReleaseMutex(void) {
    /* TODO: implement KERNEL32.dll::ReleaseMutex */
    STUB("KERNEL32.dll::ReleaseMutex");
}

/* 0x006B2398 */ void bridge_GetLastError(void) {
    /* TODO: implement KERNEL32.dll::GetLastError */
    STUB("KERNEL32.dll::GetLastError");
}

/* 0x006B239C */ void bridge_CreateMutexA(void) {
    /* TODO: implement KERNEL32.dll::CreateMutexA */
    STUB("KERNEL32.dll::CreateMutexA");
}

/* 0x006B23A0 */ void bridge_DeleteFileA(void) {
    /* TODO: implement KERNEL32.dll::DeleteFileA */
    STUB("KERNEL32.dll::DeleteFileA");
}

/* 0x006B23A4 */ void bridge_ExpandEnvironmentStringsA(void) {
    /* TODO: implement KERNEL32.dll::ExpandEnvironmentStringsA */
    STUB("KERNEL32.dll::ExpandEnvironmentStringsA");
}

/* 0x006B23A8 */ void bridge_CreateDirectoryA(void) {
    /* TODO: implement KERNEL32.dll::CreateDirectoryA */
    STUB("KERNEL32.dll::CreateDirectoryA");
}

/* 0x006B23AC */ void bridge_SetThreadAffinityMask(void) {
    /* TODO: implement KERNEL32.dll::SetThreadAffinityMask */
    STUB("KERNEL32.dll::SetThreadAffinityMask");
}

/* 0x006B23B0 */ void bridge_GetCurrentThread(void) {
    /* TODO: implement KERNEL32.dll::GetCurrentThread */
    STUB("KERNEL32.dll::GetCurrentThread");
}

/* 0x006B23B4 */ void bridge_OutputDebugStringA(void) {
    /* TODO: implement KERNEL32.dll::OutputDebugStringA */
    STUB("KERNEL32.dll::OutputDebugStringA");
}

/* 0x006B23B8 */ void bridge_GetCurrentThreadId(void) {
    /* TODO: implement KERNEL32.dll::GetCurrentThreadId */
    STUB("KERNEL32.dll::GetCurrentThreadId");
}

/* 0x006B23BC */ void bridge_WriteFile(void) {
    /* TODO: implement KERNEL32.dll::WriteFile */
    STUB("KERNEL32.dll::WriteFile");
}

/* 0x006B23C0 */ void bridge_MoveFileA(void) {
    /* TODO: implement KERNEL32.dll::MoveFileA */
    STUB("KERNEL32.dll::MoveFileA");
}


/* ===== OLEAUT32.dll ===== */

/* 0x006B23C8 */ void bridge_ordinal_2(void) {
    /* TODO: implement OLEAUT32.dll::ordinal_2 */
    STUB("OLEAUT32.dll::ordinal_2");
}

/* 0x006B23CC */ void bridge_ordinal_6(void) {
    /* TODO: implement OLEAUT32.dll::ordinal_6 */
    STUB("OLEAUT32.dll::ordinal_6");
}


/* ===== RPCRT4.dll ===== */

/* 0x0067C1D0 */ void bridge_RpcStringFreeA(void) {
    /* TODO: implement RPCRT4.dll::RpcStringFreeA */
    STUB("RPCRT4.dll::RpcStringFreeA");
}

/* 0x0067C1D4 */ void bridge_UuidToStringA(void) {
    /* TODO: implement RPCRT4.dll::UuidToStringA */
    STUB("RPCRT4.dll::UuidToStringA");
}


/* ===== SHELL32.dll ===== */

/* 0x0067C1DC */ void bridge_SHCreateDirectoryExA(void) {
    /* TODO: implement SHELL32.dll::SHCreateDirectoryExA */
    STUB("SHELL32.dll::SHCreateDirectoryExA");
}

/* 0x0067C1E0 */ void bridge_SHGetFolderPathA(void) {
    /* TODO: implement SHELL32.dll::SHGetFolderPathA */
    STUB("SHELL32.dll::SHGetFolderPathA");
}

/* 0x0067C1E4 */ void bridge_ShellExecuteA(void) {
    /* TODO: implement SHELL32.dll::ShellExecuteA */
    STUB("SHELL32.dll::ShellExecuteA");
}


/* ===== USER32.dll ===== */

/* 0x0067C1EC */ void bridge_DispatchMessageA(void) {
    /* TODO: implement USER32.dll::DispatchMessageA */
    STUB("USER32.dll::DispatchMessageA");
}

/* 0x0067C1F0 */ void bridge_MoveWindow(void) {
    /* TODO: implement USER32.dll::MoveWindow */
    STUB("USER32.dll::MoveWindow");
}

/* 0x0067C1F4 */ void bridge_AdjustWindowRect(void) {
    /* TODO: implement USER32.dll::AdjustWindowRect */
    STUB("USER32.dll::AdjustWindowRect");
}

/* 0x0067C1F8 */ void bridge_SystemParametersInfoA(void) {
    /* TODO: implement USER32.dll::SystemParametersInfoA */
    STUB("USER32.dll::SystemParametersInfoA");
}

/* 0x0067C1FC */ void bridge_MessageBoxA(void) {
    /* TODO: implement USER32.dll::MessageBoxA */
    STUB("USER32.dll::MessageBoxA");
}

/* 0x0067C200 */ void bridge_UpdateWindow(void) {
    /* TODO: implement USER32.dll::UpdateWindow */
    STUB("USER32.dll::UpdateWindow");
}

/* 0x0067C204 */ void bridge_ShowWindow(void) {
    /* TODO: implement USER32.dll::ShowWindow */
    STUB("USER32.dll::ShowWindow");
}

/* 0x0067C208 */ void bridge_LoadIconA(void) {
    /* TODO: implement USER32.dll::LoadIconA */
    STUB("USER32.dll::LoadIconA");
}

/* 0x0067C20C */ void bridge_LoadCursorA(void) {
    /* TODO: implement USER32.dll::LoadCursorA */
    STUB("USER32.dll::LoadCursorA");
}

/* 0x0067C210 */ void bridge_RegisterClassExA(void) {
    /* TODO: implement USER32.dll::RegisterClassExA */
    STUB("USER32.dll::RegisterClassExA");
}

/* 0x0067C214 */ void bridge_CreateWindowExA(void) {
    /* TODO: implement USER32.dll::CreateWindowExA */
    STUB("USER32.dll::CreateWindowExA");
}

/* 0x0067C218 */ void bridge_PostQuitMessage(void) {
    /* TODO: implement USER32.dll::PostQuitMessage */
    STUB("USER32.dll::PostQuitMessage");
}

/* 0x0067C21C */ void bridge_DefWindowProcA(void) {
    /* TODO: implement USER32.dll::DefWindowProcA */
    STUB("USER32.dll::DefWindowProcA");
}

/* 0x0067C220 */ void bridge_DestroyWindow(void) {
    /* TODO: implement USER32.dll::DestroyWindow */
    STUB("USER32.dll::DestroyWindow");
}

/* 0x0067C224 */ void bridge_SetCursorPos(void) {
    /* TODO: implement USER32.dll::SetCursorPos */
    STUB("USER32.dll::SetCursorPos");
}

/* 0x0067C228 */ void bridge_SetCursor(void) {
    /* TODO: implement USER32.dll::SetCursor */
    STUB("USER32.dll::SetCursor");
}

/* 0x0067C22C */ void bridge_PeekMessageA(void) {
    /* TODO: implement USER32.dll::PeekMessageA */
    STUB("USER32.dll::PeekMessageA");
}

/* 0x0067C230 */ void bridge_TranslateMessage(void) {
    /* TODO: implement USER32.dll::TranslateMessage */
    STUB("USER32.dll::TranslateMessage");
}

/* 0x0067C234 */ void bridge_SendMessageA(void) {
    /* TODO: implement USER32.dll::SendMessageA */
    STUB("USER32.dll::SendMessageA");
}

/* 0x0067C238 */ void bridge_GetSystemMetrics(void) {
    /* TODO: implement USER32.dll::GetSystemMetrics */
    STUB("USER32.dll::GetSystemMetrics");
}

/* 0x0067C23C */ void bridge_SetForegroundWindow(void) {
    /* TODO: implement USER32.dll::SetForegroundWindow */
    STUB("USER32.dll::SetForegroundWindow");
}

/* 0x0067C240 */ void bridge_IsIconic(void) {
    /* TODO: implement USER32.dll::IsIconic */
    STUB("USER32.dll::IsIconic");
}


/* ===== WINMM.dll ===== */

/* 0x006B244C */ void bridge_timeGetTime(void) {
    /* TODO: implement WINMM.dll::timeGetTime */
    STUB("WINMM.dll::timeGetTime");
}


/* ===== WS2_32.dll ===== */

/* 0x006B2454 */ void bridge_ordinal_9(void) {
    /* TODO: implement WS2_32.dll::ordinal_9 */
    STUB("WS2_32.dll::ordinal_9");
}

/* 0x006B2458 */ void bridge_ordinal_14(void) {
    /* TODO: implement WS2_32.dll::ordinal_14 */
    STUB("WS2_32.dll::ordinal_14");
}

/* 0x006B245C */ void bridge_ordinal_15(void) {
    /* TODO: implement WS2_32.dll::ordinal_15 */
    STUB("WS2_32.dll::ordinal_15");
}

/* 0x006B2460 */ void bridge_ordinal_8(void) {
    /* TODO: implement WS2_32.dll::ordinal_8 */
    STUB("WS2_32.dll::ordinal_8");
}

/* 0x006B2464 */ void bridge_ordinal_115(void) {
    /* TODO: implement WS2_32.dll::ordinal_115 */
    STUB("WS2_32.dll::ordinal_115");
}

/* 0x006B2468 */ void bridge_ordinal_23(void) {
    /* TODO: implement WS2_32.dll::ordinal_23 */
    STUB("WS2_32.dll::ordinal_23");
}

/* 0x006B246C */ void bridge_ordinal_10(void) {
    /* TODO: implement WS2_32.dll::ordinal_10 */
    STUB("WS2_32.dll::ordinal_10");
}

/* 0x006B2470 */ void bridge_ordinal_21(void) {
    /* TODO: implement WS2_32.dll::ordinal_21 */
    STUB("WS2_32.dll::ordinal_21");
}

/* 0x006B2474 */ void bridge_ordinal_7(void) {
    /* TODO: implement WS2_32.dll::ordinal_7 */
    STUB("WS2_32.dll::ordinal_7");
}

/* 0x006B2478 */ void bridge_ordinal_4(void) {
    /* TODO: implement WS2_32.dll::ordinal_4 */
    STUB("WS2_32.dll::ordinal_4");
}

/* 0x006B247C */ void bridge_ordinal_16(void) {
    /* TODO: implement WS2_32.dll::ordinal_16 */
    STUB("WS2_32.dll::ordinal_16");
}

/* 0x006B2480 */ void bridge_ordinal_19(void) {
    /* TODO: implement WS2_32.dll::ordinal_19 */
    STUB("WS2_32.dll::ordinal_19");
}

/* 0x006B2484 */ void bridge_ordinal_18(void) {
    /* TODO: implement WS2_32.dll::ordinal_18 */
    STUB("WS2_32.dll::ordinal_18");
}

/* 0x006B2488 */ void bridge_ordinal_3(void) {
    /* TODO: implement WS2_32.dll::ordinal_3 */
    STUB("WS2_32.dll::ordinal_3");
}

/* 0x006B248C */ void bridge_ordinal_116(void) {
    /* TODO: implement WS2_32.dll::ordinal_116 */
    STUB("WS2_32.dll::ordinal_116");
}

/* 0x006B2490 */ void bridge_ordinal_51(void) {
    /* TODO: implement WS2_32.dll::ordinal_51 */
    STUB("WS2_32.dll::ordinal_51");
}

/* 0x006B2494 */ void bridge_ordinal_55(void) {
    /* TODO: implement WS2_32.dll::ordinal_55 */
    STUB("WS2_32.dll::ordinal_55");
}

/* 0x006B2498 */ void bridge_ordinal_12(void) {
    /* TODO: implement WS2_32.dll::ordinal_12 */
    STUB("WS2_32.dll::ordinal_12");
}

/* 0x006B249C */ void bridge_ordinal_52(void) {
    /* TODO: implement WS2_32.dll::ordinal_52 */
    STUB("WS2_32.dll::ordinal_52");
}

/* 0x006B24A0 */ void bridge_ordinal_111(void) {
    /* TODO: implement WS2_32.dll::ordinal_111 */
    STUB("WS2_32.dll::ordinal_111");
}

/* 0x006B24A4 */ void bridge_ordinal_11(void) {
    /* TODO: implement WS2_32.dll::ordinal_11 */
    STUB("WS2_32.dll::ordinal_11");
}

/* 0x006B24A8 */ void bridge_ordinal_56(void) {
    /* TODO: implement WS2_32.dll::ordinal_56 */
    STUB("WS2_32.dll::ordinal_56");
}


/* ===== XINPUT1_3.dll ===== */

/* 0x006B24B0 */ void bridge_ordinal_2(void) {
    /* TODO: implement XINPUT1_3.dll::ordinal_2 */
    STUB("XINPUT1_3.dll::ordinal_2");
}

/* 0x006B24B4 */ void bridge_ordinal_4(void) {
    /* TODO: implement XINPUT1_3.dll::ordinal_4 */
    STUB("XINPUT1_3.dll::ordinal_4");
}


/* ===== binkw32.dll ===== */

/* 0x006B24BC */ void bridge__BinkOpen@8(void) {
    /* TODO: implement binkw32.dll::_BinkOpen@8 */
    STUB("binkw32.dll::_BinkOpen@8");
}

/* 0x006B24C0 */ void bridge__BinkClose@4(void) {
    /* TODO: implement binkw32.dll::_BinkClose@4 */
    STUB("binkw32.dll::_BinkClose@4");
}

/* 0x006B24C4 */ void bridge__BinkSetVolume@12(void) {
    /* TODO: implement binkw32.dll::_BinkSetVolume@12 */
    STUB("binkw32.dll::_BinkSetVolume@12");
}

/* 0x006B24C8 */ void bridge__BinkOpenDirectSound@4(void) {
    /* TODO: implement binkw32.dll::_BinkOpenDirectSound@4 */
    STUB("binkw32.dll::_BinkOpenDirectSound@4");
}

/* 0x006B24CC */ void bridge__BinkSetSoundTrack@8(void) {
    /* TODO: implement binkw32.dll::_BinkSetSoundTrack@8 */
    STUB("binkw32.dll::_BinkSetSoundTrack@8");
}

/* 0x006B24D0 */ void bridge__BinkDoFrame@4(void) {
    /* TODO: implement binkw32.dll::_BinkDoFrame@4 */
    STUB("binkw32.dll::_BinkDoFrame@4");
}

/* 0x006B24D4 */ void bridge__BinkCopyToBufferRect@44(void) {
    /* TODO: implement binkw32.dll::_BinkCopyToBufferRect@44 */
    STUB("binkw32.dll::_BinkCopyToBufferRect@44");
}

/* 0x006B24D8 */ void bridge__BinkGoto@12(void) {
    /* TODO: implement binkw32.dll::_BinkGoto@12 */
    STUB("binkw32.dll::_BinkGoto@12");
}

/* 0x006B24DC */ void bridge__BinkNextFrame@4(void) {
    /* TODO: implement binkw32.dll::_BinkNextFrame@4 */
    STUB("binkw32.dll::_BinkNextFrame@4");
}

/* 0x006B24E0 */ void bridge__BinkWait@4(void) {
    /* TODO: implement binkw32.dll::_BinkWait@4 */
    STUB("binkw32.dll::_BinkWait@4");
}

/* 0x006B24E4 */ void bridge__BinkSetSoundSystem@8(void) {
    /* TODO: implement binkw32.dll::_BinkSetSoundSystem@8 */
    STUB("binkw32.dll::_BinkSetSoundSystem@8");
}


/* ===== d3d9.dll ===== */

/* 0x006B24EC */ void bridge_Direct3DCreate9(void) {
    /* TODO: implement d3d9.dll::Direct3DCreate9 */
    STUB("d3d9.dll::Direct3DCreate9");
}


/* ===== d3dx9_31.dll ===== */

/* 0x006B24F4 */ void bridge_D3DXMatrixOrthoLH(void) {
    /* TODO: implement d3dx9_31.dll::D3DXMatrixOrthoLH */
    STUB("d3dx9_31.dll::D3DXMatrixOrthoLH");
}

/* 0x006B24F8 */ void bridge_D3DXMatrixMultiply(void) {
    /* TODO: implement d3dx9_31.dll::D3DXMatrixMultiply */
    STUB("d3dx9_31.dll::D3DXMatrixMultiply");
}

/* 0x006B24FC */ void bridge_D3DXCreateCubeTextureFromFileInMemory(void) {
    /* TODO: implement d3dx9_31.dll::D3DXCreateCubeTextureFromFileInMemory */
    STUB("d3dx9_31.dll::D3DXCreateCubeTextureFromFileInMemory");
}

/* 0x006B2500 */ void bridge_D3DXCreateTextureFromFileInMemoryEx(void) {
    /* TODO: implement d3dx9_31.dll::D3DXCreateTextureFromFileInMemoryEx */
    STUB("d3dx9_31.dll::D3DXCreateTextureFromFileInMemoryEx");
}

/* 0x006B2504 */ void bridge_D3DXCreateTextureFromFileInMemory(void) {
    /* TODO: implement d3dx9_31.dll::D3DXCreateTextureFromFileInMemory */
    STUB("d3dx9_31.dll::D3DXCreateTextureFromFileInMemory");
}

/* 0x006B2508 */ void bridge_D3DXSaveSurfaceToFileInMemory(void) {
    /* TODO: implement d3dx9_31.dll::D3DXSaveSurfaceToFileInMemory */
    STUB("d3dx9_31.dll::D3DXSaveSurfaceToFileInMemory");
}

/* 0x006B250C */ void bridge_D3DXCreateEffectCompiler(void) {
    /* TODO: implement d3dx9_31.dll::D3DXCreateEffectCompiler */
    STUB("d3dx9_31.dll::D3DXCreateEffectCompiler");
}

/* 0x006B2510 */ void bridge_D3DXCreateBuffer(void) {
    /* TODO: implement d3dx9_31.dll::D3DXCreateBuffer */
    STUB("d3dx9_31.dll::D3DXCreateBuffer");
}

/* 0x006B2514 */ void bridge_D3DXCompileShader(void) {
    /* TODO: implement d3dx9_31.dll::D3DXCompileShader */
    STUB("d3dx9_31.dll::D3DXCompileShader");
}


/* ===== ole32.dll ===== */

/* 0x0067C318 */ void bridge_CoInitialize(void) {
    /* TODO: implement ole32.dll::CoInitialize */
    STUB("ole32.dll::CoInitialize");
}

/* 0x0067C31C */ void bridge_CoInitializeEx(void) {
    /* TODO: implement ole32.dll::CoInitializeEx */
    STUB("ole32.dll::CoInitializeEx");
}

/* 0x0067C320 */ void bridge_CoSetProxyBlanket(void) {
    /* TODO: implement ole32.dll::CoSetProxyBlanket */
    STUB("ole32.dll::CoSetProxyBlanket");
}

/* 0x0067C324 */ void bridge_CoUninitialize(void) {
    /* TODO: implement ole32.dll::CoUninitialize */
    STUB("ole32.dll::CoUninitialize");
}

/* 0x0067C328 */ void bridge_CoCreateGuid(void) {
    /* TODO: implement ole32.dll::CoCreateGuid */
    STUB("ole32.dll::CoCreateGuid");
}

/* 0x0067C32C */ void bridge_CoCreateInstance(void) {
    /* TODO: implement ole32.dll::CoCreateInstance */
    STUB("ole32.dll::CoCreateInstance");
}

