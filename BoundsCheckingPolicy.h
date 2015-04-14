#ifndef _BOUNDS_CHECKING_POLICY_H_
#define _BOUNDS_CHECKING_POLICY_H_

#include "DataTypes.h"

namespace Odin
{
	class SimpleBoundsChecking
	{
	private:
		size_t mMagic;
	public:
		static const size_t kSizeFront = sizeof(size_t);
		static const size_t kSizeBack = sizeof(size_t);

		SimpleBoundsChecking()
		{
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN64
			mMagic = static_cast<size_t>(GetTickCount() ^ static_cast<size_t>(0x55555555U));
#endif
			mMagic |= 8U;	// Ensure nonzero
			mMagic &= ~(7U);
		}

		inline void GuardFront(void* ptr) const
		{
			size_t* guard_front_ptr = static_cast<size_t*>(ptr);
			*guard_front_ptr = reinterpret_cast<size_t>(&guard_front_ptr) ^ mMagic;
		}
		inline void GuardBack(void* ptr) const
		{
			size_t* guard_back_ptr = static_cast<size_t*>(ptr);
			*guard_back_ptr = reinterpret_cast<size_t>(&guard_back_ptr) ^ mMagic;
		}

		inline void CheckFront(const void* ptr) const
		{
			const size_t* guard_front_ptr = static_cast<const size_t*>(ptr);
			size_t check_sum = reinterpret_cast<size_t>(&guard_front_ptr) ^ mMagic;
			if (*guard_front_ptr != check_sum)
			{
				// TODO: Log an error message
			}
		}
		inline void CheckBack(const void* ptr) const
		{
			const size_t* guard_bad_ptr = static_cast<const size_t*>(ptr);
			size_t check_sum = reinterpret_cast<size_t>(&guard_bad_ptr) ^ mMagic;
			if (*guard_bad_ptr != check_sum)
			{
				// TODO: Log an error message
			}
		}
	};

	class NoBoundsChecking
	{
	public:
		static const size_t kSizeFront = 0;
		static const size_t kSizeBack = 0;

		inline void GuardFront(void* ptr) const {}
		inline void GuardBack(void* ptr) const {}

		inline void CheckFront(const void* ptr) const {}
		inline void CheckBack(const void* ptr) const {}
	};
}

#endif _BOUNDS_CHECKING_POLICY_H_