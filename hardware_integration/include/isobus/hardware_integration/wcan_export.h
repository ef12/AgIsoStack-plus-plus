#ifndef WCAN_EXPORT_H
#define WCAN_EXPORT_H

/*
 * Export/visibility control.
 *
 * The library can be consumed three ways, and the segment layout — not this
 * header — is what makes them interoperate:
 *
 *   static  : link libwcan.a into a C or C++ program built with your toolchain
 *   shared  : load wcan.dll from Python (ctypes) or any other runtime
 *   source  : compile the sources directly into your project
 *
 * Define WCAN_BUILD_SHARED when building the DLL, and WCAN_USE_SHARED when
 * consuming it. Neither is needed for static or source use.
 */

#if defined(_WIN32)
#if defined(WCAN_BUILD_SHARED)
#define WCAN_API __declspec(dllexport)
#elif defined(WCAN_USE_SHARED)
#define WCAN_API __declspec(dllimport)
#else
#define WCAN_API
#endif
#else
#define WCAN_API
#endif

/*
 * Every exported function uses the C calling convention explicitly. On 32-bit
 * Windows the default can be changed by a compiler switch, which would break
 * ctypes and any other consumer that assumes cdecl.
 */
#if defined(_WIN32) && !defined(_WIN64)
#define WCAN_CALL __cdecl
#else
#define WCAN_CALL
#endif

#endif /* WCAN_EXPORT_H */
