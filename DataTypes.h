#ifndef _DATA_TYPES_H_
#define _DATA_TYPES_H_

#include <cstdint>
#include "CompileOptions.h"

namespace Odin
{
	// Data types
	typedef int8_t		int8;
	typedef int16_t		int16;
	typedef int32_t		int32;
	typedef int64_t		int64;

	typedef uint8_t		uint8;
	typedef uint16_t	uin16;
	typedef uint32_t	uint32;
	typedef uint64_t	uint64;

	typedef float		float32;
	typedef double		float64;

#if ODIN_COMPILER == ODIN_COMPILER_MSVC
//#include <xmmintrin.h>
//	typedef	__m128		vfloat32;
#endif
}

#endif