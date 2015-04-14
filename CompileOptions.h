#ifndef _COMPILE_OPTIONS_H_
#define _COMPILE_OPTIONS_H_

namespace Odin
{
	// List platforms
#define ODIN_PLATFORM_WIN32 1

	// List of compilers
#define ODIN_COMPILER_MSVC 1

	// Find the current platform
#if defined(_WIN32)
#define ODIN_PLATFORM ODIN_PLATFORM_WIN32
#endif

	// Find the compiler and its version
#if defined( _MSC_VER )
#define ODIN_COMPILER ODIN_COMPILER_MSVC
#define ODIN_COMPILER_VER _MSC_VER
#endif

	// Find the current compiler
#if defined(__x86_64__)
#define ODIN_ARCH ODIN_ARCH_64
#else
#define ODIN_ARCH ODIN_ARCH_32
#endif

	// Use __forceinline or just inline
#if ODIN_COMPILER == ODIN_COMPILER_MSVC
#	if ODIN_COMPILER_VER >= 1200
#		define FORCEINLINE __forceinline
#	endif
#endif

	// See if in debug mode
#if ODIN_COMPILER == ODIN_COMPILER_MSVC
#	ifdef _DEBUG
#		define ODIN_DEBUG 1
#	endif
#endif
}

#endif