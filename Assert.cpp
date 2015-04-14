#include <cstdarg>
#include <string.h>
#include <stdio.h>
#include <cstdlib>
#include "Assert.h"

namespace Odin
{
	bool _ignoreAll = false;

#if defined(ODIN_ASSERT_LOG_FILE)
	struct AssertLogFile
	{
		AssertLogFile()
		{
			if (FILE* f = fopen(ODIN_ASSERT_LOG_FILE, "w"))
				fclose(f);
		}
	};

	static AssertLogFile assertLogFile;
#endif
	//------------------------------------------------------------------------------------------
	int32 print(FILE* out, int32 level, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		int32 count = vfprintf(out, format, args);
		fflush(out);
		va_end(args);

#if defined(ODIN_ASSERT_LOG_FILE)
		struct Local
		{
			static void logAssert(const char* format, va_list args)
			{
				if (FILE* f = fopen(ODIN_ASSERT_LOG_FILE, "a"))
				{
					vfprintf(f, format, args);
					fclose(f);
				}
			}
		};

		va_start(args, format);
		Local::logAssert(format, args);
		va_end(args);
#endif

#if defined(_WIN64)
		char buffer[2048];
		va_start(args, format);
		vsnprintf(buffer, 2048, format, args);
		::OutputDebugStringA(buffer);
		va_end(args);
#endif

		return count;
	}
	//------------------------------------------------------------------------------------------
	int32 formatLevel(int32 level, const char* expression, FILE* out)
	{
		const char* levelString = 0;

		switch (level)
		{
		case AssertLevel::ASSERT_LEVEL_DEBUG:
			levelString = "DEBUG";
			break;
		case AssertLevel::ASSERT_LEVEL_WARNING:
			levelString = "WARNING";
			break;
		case AssertLevel::ASSERT_LEVEL_ERROR:
			levelString = "ERROR";
			break;
		case AssertLevel::ASSERT_LEVEL_FATAL:
			levelString = "FATAL";
			break;
		default:
			break;

		}
		if (levelString)
			return print(out, level, "Assertion '%s' failed (%s)\n", expression, levelString);
		else
			return print(out, level, "Assertion '%s' failed (level = %d)\n", expression, level);
	}
	//------------------------------------------------------------------------------------------
	AssertAction defaultHandler(const char* file, int32 line, const char* function, const char* expression,
								int32 level, const char* message)
	{
#if defined(_WIN64)
		if (::GetConsoleWindow() == NULL)
		{
			if (::AllocConsole())
			{
				(void)freopen("CONIN$", "r", stdin);
				(void)freopen("CONOUT$", "w", stdout);
				(void)freopen("CONOUT$", "w", stderr);

				SetFocus(::GetConsoleWindow());
			}
		}
#endif

		formatLevel(level, expression, stderr);
		print(stderr, level, "  in file %s, line %d\n  function: %s\n", file, line, function);

		if (message)
			print(stderr, level, "  with message: %s\n\n", message);

		if (level == AssertLevel::ASSERT_LEVEL_DEBUG || level == AssertLevel::ASSERT_LEVEL_WARNING)
			return AssertAction::ASSERT_ACTION_NONE;
		else if (level == AssertLevel::ASSERT_LEVEL_ERROR)
		{
			for (;;)
			{
				fprintf(stderr, "Press (I)gnore / Ignore (F)orever / Ignore (A)ll / (D)ebug / A(b)ort: ");
				fflush(stderr);

				char line[256];
				if (!fgets(line, sizeof(line), stdin))
				{
					clearerr(stdin);
					fprintf(stderr, "\n");
					fflush(stderr);
					continue;
				}

				char input[2] = { 'b', 0 };
				if (sscanf_s(line, " %1[a-zA-Z] ", input) != 1)
					continue;

				switch (*input)
				{
				case 'b':
				case 'B':
					return AssertAction::ASSERT_ACTION_ABORT;

				case 'd':
				case 'D':
					return AssertAction::ASSERT_ACTION_BREAK;

				case 'i':
				case 'I':
					return AssertAction::ASSERT_ACTION_IGNORE;

				case 'f':
				case 'F':
					return AssertAction::ASSERT_ACTION_IGNORE_LINE;

				case 'a':
				case 'A':
					return AssertAction::ASSERT_ACTION_IGNORE_ALL;

				default:
					break;
				}
			}
		}
		else
			return AssertAction::ASSERT_ACTION_ABORT;
	}
	//------------------------------------------------------------------------------------------
	AssertAction handleAssert(const char* file, int line, const char* function, const char* expression, int level,
		bool& ignoreLine, const char* message, ...)
	{
		char message_[2048] = { 0 };
		const char* file_;

		if (message)
		{
			va_list args;
			va_start(args, message);
			vsnprintf_s(message_, 2048, message, args);
			va_end(args);

			message = message_;
		}

#if defined(_WIN64)
		file_ = strrchr(file, '\\');
#else
		file_ = strrchr(file, '/');
#endif // #if defined(_WIN64)

		file = file_ ? file_ + 1 : file;
		AssertAction action = defaultHandler(file, line, function, expression, level, message);

		switch (action)
		{
		case AssertAction::ASSERT_ACTION_ABORT:
			abort();

		case AssertAction::ASSERT_ACTION_IGNORE_LINE:
			ignoreLine = true;
			break;

		case AssertAction::ASSERT_ACTION_IGNORE_ALL:
			ignoreAllAsserts(true);
			break;

		case AssertAction::ASSERT_ACTION_IGNORE:
		case AssertAction::ASSERT_ACTION_BREAK:
		case AssertAction::ASSERT_ACTION_NONE:
		default:
			return action;
		}

		return AssertAction::ASSERT_ACTION_NONE;
	}
	//------------------------------------------------------------------------------------------
	void ignoreAllAsserts(bool value)
	{
		_ignoreAll = value;
	}
	//------------------------------------------------------------------------------------------
	bool ignoreAllAsserts()
	{
		return _ignoreAll;
	}
}