#ifndef _ALLOCATOR_H_
#define _ALLOCATOR_H_

#include <new>
#include "DataTypes.h"

// Just hardcode this for now to 4kB
#define PAGE_SIZE	65536

namespace Odin
{
	class Allocator
	{
	public:
		static const uint32 kDefaultAlignment = 8;

		Allocator() {}
		virtual ~Allocator() {}

		// Initialize the allocator
		virtual bool init() = 0;
		// Allocate the specified amount of memory aligned to the specified alignment
		virtual void* allocate(size_t size, size_t alignment, size_t offset = 0,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0) = 0;
		// Allocate a continuous array of fixed sized elements
		virtual void* callocate(size_t num_elements, size_t elem_size,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0) = 0;
		// Free an allocation previously made with allocate
		virtual void deallocate(void* ptr) = 0;
		// Return the amount of usable memory allocated at ptr
		virtual size_t getAllocSize(void* ptr) = 0;
		// Return the total amount of memory allocated by this allocator
		virtual size_t getTotalAllocated() = 0;
	};

	// Call destructor and free the allocated memory
	template <typename T, class allocator>
	void Delete(T* object, allocator* alloc)
	{
		// Call the destructor
		object->~T();
		// Free the memory
		alloc->deallocate(object);
	}

	// Allocate memory for array of objects and call their constructors (Non-POD type)
	template <typename T, class allocator>
	T* NewArray(allocator* alloc, size_t N, const char* file_name, uint32 line, const char* func_name)//, NonPODType)
	{
		void* ptr = arena->allocate(sizeof(T) * N + sizeof(size_t), Allocator::kDefaultAlignment, 0,
			file_name, line, func_name);
		// Store number of instances in first size_t bytes
		*(reinterpret_cast<size_t*>(ptr))++ = N;
		// Construct instances using placement new
		const T* const one_past_last = reinterpret_cast<size_t*>(ptr) + N;
		T* tptr = reinterpret_cast<T*>(ptr);
		while (tptr < one_past_last)
			new (tptr++) T;
		// Return the pointer to the first instance
		return (tptr - N);
	}
	// Overload for POD type
	/*template <typename T, class allocator>
	T* NewArray(allocator* alloc, size_t N, const char* file_name, uint32 line, const char* func_name, PODType)
	{
		return static_cast(alloc->allocate(sizeof(T) * N, Allocator::kDefaultAlignment, 0,
			file_name, line, func_name));
	}*/
	// Call the destructors for the array of objects and free the memory (Non-POD type)
	template <typename T, class allocator>
	void DeleteArray(T* ptr, allocator* alloc)//, NonPODType)
	{
		// Get the number of instances
		const size_t num = reinterpret_cast<size_t*>(ptr)[-1];
		// Call the destructors in the reverse order
		for (size_t i = num; i > 0; --i)
		{
			ptr[i - 1].~T();
		}
		alloc->deallocate(reinterpret_cast<size_t*>(ptr)-1);
	}
	// Overload for POD type
	/*template <typename T, class allocator>
	void DeleteArray(T* ptr, allocator* alloc, PODType)
	{
		alloc->deallocate(ptr);
	}*/
	// A helper function to forward the call to the correct version of DeleteArray
	/*template <typename T, class allocator>
	void DeleteArray(T* ptr, allocator* alloc)
	{
		DeleteArray(ptr, alloc, IntToType<IsPOD<T>::value>());
	}*/

	// Partial template specialization to deduce type of class and count of instances
	template <class T>
	struct TypeCount
	{
	};
	template <class T, size_t N>
	struct TypeCount<T[N]>
	{
		typedef T Type;
		static const size_t count = N;
	};

	// Traits-class to decide whether a given type is POD or not
	/*template <typename T>
	struct IsPOD
	{
		static const bool value = std::is_pod<T>::value;
	};
	// Struct to turn the above value into a type
	template <bool I>
	struct IntToType
	{
	};
	typedef IntToType<false> NonPODType;
	typedef IntToType<true> PODType;
	*/

	// Macros to help select the appropriate version of NEW_ARRAY macros based 
	// on the number of parameters passed
#define EXPAND(x)	x	// This macro is used to work around MSVC __VA_ARGS__ bug
#define _ARG(_0, _1, _2, _3, ...)	_3
#define NARG(...)	EXPAND(_ARG(__VA_ARGS__, 3, 2, 1, 0))
	
	// Specify count at compile time (eg: int32[4])						   
#define NEW_ARRAY_2(type, allocator) NewArray<TypeCount<type>::Type>(allocator,\
								 TypeCount<type>::count, __FILE__, __LINE__, __FUNCTION__)/*,\
								IntToType<IsPOD<TypeCount<type>::Type>::value>() )*/
	// Specify count at run time
#define NEW_ARRAY_3(type, count, allocator) NewArray<type>(allocator,\
								count, __FILE__, __LINE__, __FUNCTION__)/*,\
								IntToType<IsPOD<type>::value>() )*/
	/*
		Only the following macros should be used to allocate and delete memory
	*/
	// Instantiate a single object
#define ODIN_NEW(type, alignment, allocator)	new(allocator->allocate(sizeof(type), alignment, 0,\
								 __FILE__, __LINE__, __FUNCTION__)) type
	// Instantiate an array of objects 
	// ODIN_NEW_ARRAY(type, count, allocator) for run time count
	// ODIN_NEW_ARRAY(type[count], allocator) for compile time count
#define ODIN_NEW_ARRAY(...)	ODIN_JOIN(NEW_ARRAY_, NARG(__VA_ARGS__))EXPAND((__VA_ARGS__))
	// Delete an object
#define ODIN_DELETE(object, allocator)	Delete(object, allocator)
	// Delete an array of objects
#define ODIN_DELETE_ARRAY(object, allocator)	DeleteArray(object, allocator)
}

#endif	// _ALLOCATOR_H_