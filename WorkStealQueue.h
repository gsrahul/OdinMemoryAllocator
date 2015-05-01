#ifndef _WORK_STEAL_QUEUE_H_
#define _WORK_STEAL_QUEUE_H_

#include "Allocator.h"

namespace Odin
{
	/*
		Chase-Lev work stealing deque
	*/

	// Max size of the queue
#define WORK_QUEUE_SIZE	1024
	// Forward declaration
	struct Task;

	class WorkStealQueue
	{
	private:
		Allocator* mAlloc;							// Pointer to the passed allocator
		std::atomic<size_t> mTop, mBottom;			// The top and bottom indexes
		std::atomic<std::atomic<Task*>*> mArray;	// Pointer to the array
	public:
		WorkStealQueue(Allocator* alloc) : mAlloc(alloc), mTop(0), mBottom(0)
		{
			// ASSERT_ERROR(alloc != nullptr, "No allocator passed to WorkStealQueue");
			mArray = ODIN_NEW_ARRAY(std::atomic<Task*>[WORK_QUEUE_SIZE], mAlloc);
		}
		
		WorkStealQueue(const WorkStealQueue& other) = delete;
		
		WorkStealQueue& operator = (const WorkStealQueue& other) = delete;

		~WorkStealQueue()
		{
			std::atomic<Task*>* p = mArray.load(std::memory_order_relaxed);
			if (p)
				ODIN_DELETE_ARRAY(p, mAlloc);
		}

		void push(Task* task)
		{
			size_t b = mBottom.load(std::memory_order_relaxed);
			size_t t = mTop.load(std::memory_order_acquire);
			std::atomic<Task*>* a = mArray.load(std::memory_order_relaxed);
			if (b - t <= WORK_QUEUE_SIZE - 1)
			{
				a[b].store(task, std::memory_order_relaxed);
				std::atomic_thread_fence(std::memory_order_release);
				mBottom.store(b + 1, std::memory_order_relaxed);
			}
			else
			{
				// Log error message
			}
		}

		Task* pop()
		{
			size_t b = mBottom.load(std::memory_order_relaxed) - 1;
			std::atomic<Task*>* a = mArray.load(std::memory_order_relaxed);
			mBottom.store(b, std::memory_order_relaxed);
			std::atomic_thread_fence(std::memory_order_seq_cst);
			size_t t = mTop.load(std::memory_order_relaxed);
			if (t <= b)
			{
				Task* x = a[b].load(std::memory_order_relaxed);
				if (t == b)
				{
					if (!mTop.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
						x = nullptr;
					mBottom.store(b + 1, std::memory_order_relaxed);
				}
				return x;
			}
			else
			{
				mBottom.store(b + 1, std::memory_order_relaxed);
				return nullptr;
			}
		}

		Task* steal()
		{
			size_t t = mTop.load(std::memory_order_acquire);
			std::atomic_thread_fence(std::memory_order_seq_cst);
			size_t b = mBottom.load(std::memory_order_acquire);
			Task*	x = nullptr;
			if (t < b)
			{
				std::atomic<Task*>* a = mArray.load(std::memory_order_relaxed);
				x = a[t].load(std::memory_order_relaxed);
				if (!mTop.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
					return nullptr;
			}
			return x;
		}
	};
	//-----------------------------------------------------------------------------------------
	
	/*
		A simple queue representing the global work queue in the scheduler.
		Pushing tasks into the back of this queue should be done by ONLY ONE THREAD.
		Popping tasks from the front of the queue are thread safe.
	*/
	class GlobalWorkQueue
	{
	private:
		Allocator* mAlloc;							// Pointer to the passed allocator
		std::atomic<size_t> mTop, mBottom;			// The top and bottom indexes
		std::atomic<Task*>* mArray;					// Pointer to the array
	
		GlobalWorkQueue(Allocator* alloc) : mAlloc(alloc), mTop(0), mBottom(0)
		{
			ASSERT_ERROR(alloc != nullptr, "No allocator passed to GlobalWorkQueue");
			mArray = ODIN_NEW_ARRAY(std::atomic<Task*>[WORK_QUEUE_SIZE], mAlloc);
		}
	public:
		GlobalWorkQueue(const GlobalWorkQueue& other) = delete;

		GlobalWorkQueue& operator = (const GlobalWorkQueue& other) = delete;

		~GlobalWorkQueue()
		{
			if (mArray)
				ODIN_DELETE_ARRAY(mArray, mAlloc);
		}

		void push(Task* task)
		{
			if (mArray)
			{
				// This operation should be performed by a single thread only
				if (mBottom < WORK_QUEUE_SIZE)
					mArray[mBottom++] = task;
				else
				{
					// TODO: Log error message
				}
			}
			else
			{
				// TODO: Log error message
			}
		}

		Task* pop()
		{
			if (mArray)
			{

				// This operation is thread safe
				size_t t = mTop.load(std::memory_order_acquire);
				if (t < mBottom)
				{
					// TODO: Check for race condition
					mTop.store(t + 1, std::memory_order_release);
					Task* x = mArray[t].load(std::memory_order_relaxed);
					mArray[t].store(nullptr, std::memory_order_relaxed);
				}
				else
					return nullptr;
			}
			else
				return nullptr;
		}
		friend class Scheduler;
	};
}

#endif	// _WORK_STEAL_QUEUE_H_