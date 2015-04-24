#include "LinearAllocator.h"
#include "PoolAllocator.h"
#include "GeneralAllocator.h"
#include <iostream>

// A static buffer to hold the linear allocator
char global_buffer[sizeof(Odin::LinearAllocator)];

void main()
{
	// Create a Linear Allocator
	Odin::LinearAllocator* linear_alloc = new(&global_buffer) Odin::LinearAllocator(PAGE_SIZE);
	// Initialize it
	if (!linear_alloc->init())
	{
		std::cout << "Error allocating memory";
		return;
	}
	// Create a General Allocator
	Odin::GeneralAllocator* general_alloc = ODIN_NEW(Odin::GeneralAllocator, Odin::Allocator::kDefaultAlignment,
		linear_alloc)(PAGE_SIZE, PAGE_SIZE, PAGE_SIZE, PAGE_SIZE / 3);
	general_alloc->init();
	// Destroy the allocators
	linear_alloc->reset();
	delete linear_alloc;
	delete general_alloc;
}