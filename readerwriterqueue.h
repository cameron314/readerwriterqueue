#pragma once

#include "atomicops.h"
#include <type_traits>
#include <utility>
#include <cassert>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>		// For malloc/free


// A lock-free queue for a single-consumer, single-producer architecture.
// The queue is also wait-free in the common path (except if more memory
// needs to be allocated, in which case malloc is called).
// Allocates memory sparingly (O(lg(n) times, amortized), and only once if
// the original maximum size estimate is never exceeded.
// Tested on x86/x64 processors, but semantics should be correct for all
// architectures (given the right implementations in atomicops.h), provided
// that aligned integer and pointer accesses are naturally atomic.
// Note that there should only be one consumer thread and producer thread;
// Switching roles of the threads, or using multiple consecutive threads for
// one role, is not safe unless properly synchronized.
// Using the queue exclusively from one thread is fine, though a bit silly.

#define CACHE_LINE_SIZE 64

namespace moodycamel {

template<typename T>
class ReaderWriterQueue
{
	// Design: Based on a queue-of-queues. The low-level queues are just
	// circular buffers with front and tail indices indicating where the
	// next element to dequeue is and where the next element can be enqueued,
	// respectively. Each low-level queue is called a "block". Each block
	// wastes exactly one element's worth of space to keep the design simple
	// (if front == tail then the queue is empty, and can't be full).
	// The high-level queue is a circular linked list of blocks; again there
	// is a front and tail, but this time they are pointers to the blocks.
	// The front block is where the next element to be dequeued is, provided
	// the block is not empty. The back block is where elements are to be
	// enqueued, provided the block is not full.
	// The producer thread owns all the tail indices/pointers. The consumer
	// thread owns all the front indices/pointers. Both threads read each
	// other's variables, but only the owning thread updates them. E.g. After
	// the consumer reads the producer's tail, the tail may change before the
	// consumer is done dequeuing an object, but the consumer knows the tail
	// will never go backwards, only forwards.
	// If there is no room to enqueue an object, an additional block (of
	// greater size than the last block) is added. Blocks are never removed.

public:
	// Constructs a queue that can hold maxSize elements without further
	// allocations.
	explicit ReaderWriterQueue(int maxSize = 15)
		: largestBlockSize(maxSize + 1),		// We need a spare slot to fit maxSize elements in the block
		enqueuing(false),
		dequeuing(false)
	{
		assert(maxSize > 0);

		auto firstBlock = new Block(largestBlockSize);
		firstBlock->next = firstBlock;
		
		frontBlock = firstBlock;
		tailBlock = firstBlock;

		// Make sure the reader/writer threads will have the initialized memory setup above:
		fence(memory_order_sync);
	}

	// Note: The queue should not be accessed concurrently while it's
	// being deleted. It's up to the user to synchronize this.
	~ReaderWriterQueue()
	{
		// Make sure we get the latest version of all variables from other CPUs:
		fence(memory_order_sync);

		// Destroy any remaining objects in queue and free memory
		Block* tailBlock_ = tailBlock;
		Block* block = frontBlock;
		do {
			Block* nextBlock = block->next;
			int blockFront = block->front;
			int blockTail = block->tail;

			for (int i = blockFront; i != blockTail; i = (i + 1) % block->size) {
				auto element = reinterpret_cast<T*>(block->data + i * sizeof(T));
				element->~T();
			}

			delete block;
			block = nextBlock;

		} while (block != tailBlock_);
	}


	// Enqueues a copy of element if there is room in the queue.
	// Returns true if the element was enqueued, false otherwise.
	// Does not allocate memory.
	inline bool try_enqueue(T const& element)
	{
		return inner_enqueue<CannotAlloc>(element);
	}

	// Enqueues a moved copy of element if there is room in the queue.
	// Returns true if the element was enqueued, false otherwise.
	// Does not allocate memory.
	inline bool try_enqueue(T&& element)
	{
		return inner_enqueue<CannotAlloc>(element);
	}


	// Enqueues a copy of element on the queue.
	// Allocates an additional block of memory if needed.
	inline void enqueue(T const& element)
	{
		inner_enqueue<CanAlloc>(element);
	}

	// Enqueues a moved copy of element on the queue.
	// Allocates an additional block of memory if needed.
	inline void enqueue(T&& element)
	{
		inner_enqueue<CanAlloc>(element);
	}


	// Attempts to dequeue an element; if the queue is empty,
	// returns false instead. If the queue has at least one element,
	// moves front to result using operator=, then returns true.
	bool try_dequeue(T& result)
	{
		ReentrantGuard guard(this->dequeuing);

		// High-level pseudocode:
		// Remember where the tail block is
		// If the front block has an element in it, dequeue it
		// Else
		//     If front block was the tail block when we entered the function, return false
		//     Else advance to next block and dequeue the item there

		// Note that we have to use the value of the tail block from before we check if the front
		// block is full or not, in case the front block is empty and then, before we check if the
		// tail block is at the front block or not, the producer fills up the front block *and
		// moves on*, which would make us skip a filled block. Seems unlikely, but was consistently
		// reproducible in practice.
		Block* tailBlockAtStart = tailBlock;
		fence(memory_order_acquire);
		
		Block* frontBlock_ = frontBlock;
		int blockFront = frontBlock_->front;
		int blockTail = frontBlock_->tail;
		fence(memory_order_acquire);
		
		if (blockFront != blockTail) {
			// Front block not empty, dequeue from here
			auto element = reinterpret_cast<T*>(frontBlock_->data + blockFront * sizeof(T));
			result = std::move(*element);
			element->~T();

			blockFront = (blockFront + 1) % frontBlock_->size;

			fence(memory_order_release);
			frontBlock_->front = blockFront;
		}
		else if (frontBlock_ != tailBlockAtStart) {
			// Front block is empty but there's another block ahead, advance to it
			Block* nextBlock = frontBlock_->next;
			// Don't need an acquire fence here since next can only ever be set on the tailBlock,
			// and we're not the tailBlock, and we did an acquire earlier after reading tailBlock which
			// ensures next is up-to-date on this CPU in case we recently were at tailBlock.

			int nextBlockFront = nextBlock->front;
			int nextBlockTail = nextBlock->tail;
			fence(memory_order_acquire);

			// Since the tailBlock is only ever advanced after being written to,
			// we know there's for sure an element to dequeue on it
			assert(nextBlockFront != nextBlockTail);

			// We're done with this block, let the producer use it if it needs
			fence(memory_order_release);		// Expose possibly pending changes to frontBlock->front from last dequeue
			frontBlock = frontBlock_ = nextBlock;

			compiler_fence(memory_order_release);	// Not strictly needed

			auto element = reinterpret_cast<T*>(frontBlock_->data + nextBlockFront * sizeof(T));
			
			result = std::move(*element);
			element->~T();

			nextBlockFront = (nextBlockFront + 1) % frontBlock_->size;
			
			fence(memory_order_release);
			frontBlock_->front = nextBlockFront;
		}
		else {
			// No elements in current block and no other block to advance to
			return false;
		}

		return true;
	}


private:
	enum AllocationMode { CanAlloc, CannotAlloc };

	template<AllocationMode canAlloc, typename U>
	bool inner_enqueue(U&& element)
	{
		ReentrantGuard guard(this->enqueuing);

		// High-level pseudocode (assuming we're allowed to alloc a new block):
		// If room in tail block, add to tail
		// Else check next block
		//     If next block is not the head block, enqueue on next block
		//     Else create a new block and enqueue there
		//     Advance tail to the block we just enqueued to

		Block* tailBlock_ = tailBlock;
		int blockFront = tailBlock_->front;
		int blockTail = tailBlock_->tail;
		fence(memory_order_acquire);

		if (((blockTail + 1) % tailBlock_->size) != blockFront) {
			// This block has room for at least one more element
			char* location = tailBlock_->data + blockTail * sizeof(T);
			new (location) T(std::forward<U>(element));

			blockTail = (blockTail + 1) % tailBlock_->size;

			fence(memory_order_release);
			tailBlock_->tail = blockTail;
		}
		else if (tailBlock_->next != frontBlock) {
			// Note that the reason we can't advance to the frontBlock and start adding new entries there
			// is because if we did, then dequeue would stay in that block, eventually reading the new values,
			// instead of advancing to the next full block (whose values were enqueued first and so should be
			// consumed first).
			
			fence(memory_order_acquire);		// Ensure we get latest writes if we got the latest frontBlock

			// tailBlock is full, but there's a free block ahead, use it
			Block* tailBlockNext = tailBlock_->next;
			int nextBlockFront = tailBlockNext->front;
			int nextBlockTail = tailBlockNext->tail;
			fence(memory_order_acquire);

			// This block must be empty since it's not the head block and we
			// go through the blocks in a circle
			assert(nextBlockFront == nextBlockTail);

			char* location = tailBlockNext->data + nextBlockTail * sizeof(T);
			new (location) T(std::forward<U>(element));

			tailBlockNext->tail = (nextBlockTail + 1) % tailBlockNext->size;

			fence(memory_order_release);
			tailBlock = tailBlockNext;
		}
		else if (canAlloc == CanAlloc) {
			// tailBlock is full and there's no free block ahead; create a new block
			largestBlockSize *= 2;
			Block* newBlock = new Block(largestBlockSize);

			new (newBlock->data) T(std::forward<U>(element));

			assert(newBlock->front == 0);
			newBlock->tail = 1;

			newBlock->next = tailBlock_->next;
			tailBlock_->next = newBlock;

			// Might be possible for the dequeue thread to see the new tailBlock->next
			// *without* seeing the new tailBlock value, but this is OK since it can't
			// advance to the next block until tailBlock is set anyway (because the only
			// case where it could try to read the next is if it's already at the tailBlock,
			// and it won't advance past tailBlock in any circumstance).
			
			fence(memory_order_release);
			tailBlock = newBlock;
		}
		else if (canAlloc == CannotAlloc) {
			// Would have had to allocate a new block to enqueue, but not allowed
			return false;
		}
		else {
			assert(false && "Should be unreachable code");
			return false;
		}

		return true;
	}


	// Disable copying
	ReaderWriterQueue(ReaderWriterQueue const&) {  }

	// Disable assignment
	ReaderWriterQueue& operator=(ReaderWriterQueue const&) {  }


private:
	struct ReentrantGuard
	{
		ReentrantGuard(bool& inSection)
			: inSection(inSection)
		{
			assert(!inSection);
			if (inSection) {
				throw std::runtime_error("ReaderWriterQueue does not support enqueuing or dequeuing elements from other elements' ctors and dtors");
			}

			inSection = true;
		}

		~ReentrantGuard() { inSection = false; }

	private:
		bool& inSection;
	};

	struct Block
	{
		// Avoid false-sharing by putting highly contended variables on their own cache lines
		AE_ALIGN(CACHE_LINE_SIZE)
		weak_atomic<int> front;	// (Atomic) Elements are read from here
		
		AE_ALIGN(CACHE_LINE_SIZE)
		weak_atomic<int> tail;		// (Atomic) Elements are enqueued here
		
		AE_ALIGN(CACHE_LINE_SIZE)	// next isn't very contended, but we don't want it on the same cache line as tail (which is)
		weak_atomic<Block*> next;	// (Atomic)
		
		char* data;		// Contents (on heap) are aligned to T's alignment

		const int size;

		Block(const int size)
			: front(0), tail(0), next(nullptr), size(size)
		{
			// Allocate enough memory for an array of Ts, aligned
			size_t alignment = std::alignment_of<T>::value;
			data = rawData = static_cast<char*>(std::malloc(sizeof(T) * size + alignment - 1));
			assert(rawData);
			auto alignmentOffset = (uintptr_t)rawData % alignment;
			if (alignmentOffset != 0) { 
				data += alignment - alignmentOffset;
			}
		}

		~Block()
		{
			std::free(rawData);
		}

	private:
		char* rawData;
	};

private:
	AE_ALIGN(CACHE_LINE_SIZE)
	weak_atomic<Block*> frontBlock;		// (Atomic) Elements are enqueued to this block
	
	AE_ALIGN(CACHE_LINE_SIZE)
	weak_atomic<Block*> tailBlock;		// (Atomic) Elements are dequeued from this block

	AE_ALIGN(CACHE_LINE_SIZE)	// Ensure tailBlock gets its own cache line
	int largestBlockSize;
	bool enqueuing;
	bool dequeuing;
};

}    // end namespace moodycamel
