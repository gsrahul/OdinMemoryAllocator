#ifndef _ASSERT_H_
#define _ASSERT_H_

#include "CompileOptions.h"
#include "DataTypes.h"

namespace Odin
{
	enum AssertLevel
	{
		ASSERT_LEVEL_DEBUG,
		ASSERT_LEVEL_WARNING,
		ASSERT_LEVEL_ERROR,
		ASSERT_LEVEL_FATAL
	};

	enum AssertAction
	{
		ASSERT_ACTION_NONE,
		ASSERT_ACTION_ABORT,
		ASSERT_ACTION_BREAK,
		ASSERT_ACTION_IGNORE,
		ASSERT_ACTION_IGNORE_LINE,
		ASSERT_ACTION_IGNORE_ALL
		//ASSERT_ACTION_THROW
	};

#if ODIN_COMPILER == ODIN_COMPILER_MSVC
#define ODIN_DEBUG_BREAK __debugbreak()
#else
#define ODIN_DEBUG_BREAK asm {int 3}
#endif

#if ODIN_COMPILER == ODIN_COMPILER_MSVC && ODIN_COMPILER_VER > 1400
#define ODIN_ASSERT_3(level, expression, ...)\
		__pragma(warning(push))\
		__pragma(warning(disable: 4127))\
		do\
				{\
			static bool _ignore = false; \
			if (_ignore || ignoreAllAsserts()); \
						else\
					{\
				if (handleAssert(__FILE__, __LINE__, __FUNCTION__, #expression, level, _ignore, __VA_ARGS__) == AssertAction::ASSERT_ACTION_BREAK)\
					ODIN_DEBUG_BREAK; \
					}\
				} while (false)\
		__pragma(warning(pop))

#else

#define ODIN_ASSERT_3(level, expression, ...)\
		do\
				{\
			static bool _ignore = false; \
			if (_ignore || ignoreAllAsserts()); \
						else\
					{\
				if (handleAssert(__FILE__, __LINE__, __func__, #expression, level, _ignore, __VA_ARGS__) == AssertAction::ASSERT_ACTION_BREAK)\
					ODIN_DEBUG_BREAK; \
					}\
				}while (false)

#endif


#define ASSERT_WARNING(...)			ODIN_ASSERT_(AssertLevel::ASSERT_LEVEL_WARNING, __VA_ARGS__)
#define ASSERT_DEBUG(...)				ODIN_ASSERT_(AssertLevel::ASSERT_LEVEL_DEBUG, __VA_ARGS__)
#define ASSERT_ERROR(...)				ODIN_ASSERT_(AssertLevel::ASSERT_LEVEL_ERROR, __VA_ARGS__)
#define ASSERT_FATAL(...)				ODIN_ASSERT_(AssertLevel::ASSERT_LEVEL_FATAL, __VA_ARGS__)

#define ODIN_JOIN(lhs, rhs)					ODIN_JOIN_(lhs, rhs)
#define ODIN_JOIN_(lhs, rhs)				ODIN_JOIN__(lhs, rhs)
#define ODIN_JOIN__(lhs, rhs)				lhs##rhs

#define ODIN_ASSERT_APPLY_VA_ARGS(M, ...)			ODIN_ASSERT_APPLY_VA_ARGS_(M, (__VA_ARGS__))
#define ODIN_ASSERT_APPLY_VA_ARGS_(M, args)		M args

#define ODIN_ASSERT_NO_MACRO

#define ODIN_ASSERT_NARG(...) ODIN_ASSERT_APPLY_VA_ARGS(ODIN_ASSERT_NARG_, ODIN_ASSERT_NO_MACRO,##__VA_ARGS__,\
	32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, \
	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, ODIN_ASSERT_NO_MACRO)
#define ODIN_ASSERT_NARG_(_0, _1, _2, _3, _4, _5, _6, _7, _8,\
	_9, _10, _11, _12, _13, _14, _15, _16, \
	_17, _18, _19, _20, _21, _22, _23, _24, \
	_25, _26, _27, _28, _29, _30, _31, _32, _33, ...) _33

#define ODIN_ASSERT_HAS_ONE_ARG(...) ODIN_ASSERT_APPLY_VA_ARGS(ODIN_ASSERT_NARG_, ODIN_ASSERT_NO_MACRO,##__VA_ARGS__,\
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, ODIN_ASSERT_NO_MACRO)

#define ODIN_ASSERT_2(level, expression, ...)	ODIN_ASSERT_3(level, expression, __VA_ARGS__)
#define ODIN_ASSERT_(level, ...)				ODIN_JOIN(ODIN_ASSERT_, ODIN_ASSERT_HAS_ONE_ARG(__VA_ARGS__))(level, __VA_ARGS__)
#define ODIN_ASSERT_0(level, ...)				ODIN_ASSERT_APPLY_VA_ARGS(ODIN_ASSERT_2, level, __VA_ARGS__)
#define ODIN_ASSERT_1(level, expression)		ODIN_ASSERT_2(level, expression, nullptr)


	AssertAction handleAssert(const char* file, int line, const char* function, const char* expression,
		int level, bool& ignore_line, const char* message, ...);
	void ignoreAllAsserts(bool value);
	bool ignoreAllAsserts();
}

#endif	// _ASSERT_H_



