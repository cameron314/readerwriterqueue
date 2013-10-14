// ©2013 Cameron Desrochers.
// Distributed under the simplified BSD license (see the license file that
// should have come with this header).

#pragma once

#include "atomicops.h"
#include <type_traits>
#include <utility>
#include <cassert>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>		// For malloc/free & size_t


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

#ifdef AE_VCPP
#pragma warning(push)
#pragma warning(disable: 4324)	// structure was padded due to __declspec(align())
#pragma warning(disable: 4820)	// padding was added
#pragma warning(disable: 4127)	// conditional expression is constant
#endif

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
	// allocations. Allocates maxSize + 1, rounded up to the nearest power
	// of 2, elements.
	explicit ReaderWriterQueue(size_t maxSize = 15)
		: largestBlockSize(ceilToPow2(maxSize + 1))		// We need a spare slot to fit maxSize elements in the block
#ifndef NDEBUG
		,enqueuing(false)
		,dequeuing(false)
#endif
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
			size_t blockFront = block->front;
			size_t blockTail = block->tail;

			for (size_t i = blockFront; i != blockTail; i = (i + 1) & block->sizeMask()) {
				auto element = reinterpret_cast<T*>(block->data + i * sizeof(T));
				element->~T();
				(void)element;
			}

			delete block;
			block = nextBlock;

		} while (block != tailBlock_);
	}


	// Enqueues a copy of element if there is room in the queue.
	// Returns true if the element was enqueued, false otherwise.
	// Does not allocate memory.
	AE_FORCEINLINE bool try_enqueue(T const& element)
	{
		return inner_enqueue<CannotAlloc>(element);
	}

	// Enqueues a moved copy of element if there is room in the queue.
	// Returns true if the element was enqueued, false otherwise.
	// Does not allocate memory.
	AE_FORCEINLINE bool try_enqueue(T&& element)
	{
		return inner_enqueue<CannotAlloc>(std::forward<T>(element));
	}


	// Enqueues a copy of element on the queue.
	// Allocates an additional block of memory if needed.
	AE_FORCEINLINE void enqueue(T const& element)
	{
		inner_enqueue<CanAlloc>(element);
	}

	// Enqueues a moved copy of element on the queue.
	// Allocates an additional block of memory if needed.
	AE_FORCEINLINE void enqueue(T&& element)
	{
		inner_enqueue<CanAlloc>(std::forward<T>(element));
	}


	// Attempts to dequeue an element; if the queue is empty,
	// returns false instead. If the queue has at least one element,
	// moves front to result using operator=, then returns true.
	bool try_dequeue(T& result)
	{
#ifndef NDEBUG
		ReentrantGuard guard(this->dequeuing);
#endif

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

		Block* frontBlock_ = frontBlock.load();
		size_t blockTail = frontBlock_->tail.load();
		size_t blockFront = frontBlock_->front.load();
		fence(memory_order_acquire);
		
		if (blockFront != blockTail) {
			// Front block not empty, dequeue from here
			auto element = reinterpret_cast<T*>(frontBlock_->data + blockFront * sizeof(T));
			result = std::move(*element);
			element->~T();

			blockFront = (blockFront + 1) & frontBlock_->sizeMask();

			fence(memory_order_release);
			frontBlock_->front = blockFront;
		}
		else if (frontBlock_ != tailBlockAtStart) {
			// Front block is empty but there's another block ahead, advance to it
			Block* nextBlock = frontBlock_->next;
			// Don't need an acquire fence here since next can only ever be set on the tailBlock,
			// and we're not the tailBlock, and we did an acquire earlier after reading tailBlock which
			// ensures next is up-to-date on this CPU in case we recently were at tailBlock.

			size_t nextBlockFront = nextBlock->front.load();
			size_t nextBlockTail = nextBlock->tail;
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

			nextBlockFront = (nextBlockFront + 1) & frontBlock_->sizeMask();
			
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
#ifndef NDEBUG
		ReentrantGuard guard(this->enqueuing);
#endif

		// High-level pseudocode (assuming we're allowed to alloc a new block):
		// If room in tail block, add to tail
		// Else check next block
		//     If next block is not the head block, enqueue on next block
		//     Else create a new block and enqueue there
		//     Advance tail to the block we just enqueued to

		Block* tailBlock_ = tailBlock.load();
		size_t blockFront = tailBlock_->front.load();
		size_t blockTail = tailBlock_->tail.load();
		fence(memory_order_acquire);

		size_t nextBlockTail = (blockTail + 1) & tailBlock_->sizeMask();
		if (nextBlockTail != blockFront) {
			// This block has room for at least one more element
			char* location = tailBlock_->data + blockTail * sizeof(T);
			new (location) T(std::forward<U>(element));

			fence(memory_order_release);
			tailBlock_->tail = nextBlockTail;
		}
		else if (tailBlock_->next.load() != frontBlock) {
			// Note that the reason we can't advance to the frontBlock and start adding new entries there
			// is because if we did, then dequeue would stay in that block, eventually reading the new values,
			// instead of advancing to the next full block (whose values were enqueued first and so should be
			// consumed first).
			
			fence(memory_order_acquire);		// Ensure we get latest writes if we got the latest frontBlock

			// tailBlock is full, but there's a free block ahead, use it
			Block* tailBlockNext = tailBlock_->next.load();
			size_t nextBlockFront = tailBlockNext->front.load();
			nextBlockTail = tailBlockNext->tail.load();
			fence(memory_order_acquire);

			// This block must be empty since it's not the head block and we
			// go through the blocks in a circle
			assert(nextBlockFront == nextBlockTail);

			char* location = tailBlockNext->data + nextBlockTail * sizeof(T);
			new (location) T(std::forward<U>(element));

			tailBlockNext->tail = (nextBlockTail + 1) & tailBlockNext->sizeMask();

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

			newBlock->next = tailBlock_->next.load();
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



	AE_FORCEINLINE static size_t ceilToPow2(size_t x)
	{
		// From http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
		--x;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		for (size_t i = 1; i < sizeof(size_t); i <<= 1) {
			x |= x >> (i << 3);
		}
		++x;
		return x;
	}
private:
#ifndef NDEBUG
	struct ReentrantGuard
	{
		ReentrantGuard(bool& _inSection)
			: inSection(_inSection)
		{
			assert(!inSection);
			if (inSection) {
				throw std::runtime_error("ReaderWriterQueue does not support enqueuing or dequeuing elements from other elements' ctors and dtors");
			}

			inSection = true;
		}

		~ReentrantGuard() { inSection = false; }

	private:
		ReentrantGuard& operator=(ReentrantGuard const&);

	private:
		bool& inSection;
	};
#endif

	struct Block
	{
		// Avoid false-sharing by putting highly contended variables on their own cache lines
		AE_ALIGN(CACHE_LINE_SIZE)
		weak_atomic<size_t> front;	// (Atomic) Elements are read from here
		
		AE_ALIGN(CACHE_LINE_SIZE)
		weak_atomic<size_t> tail;	// (Atomic) Elements are enqueued here
		
		AE_ALIGN(CACHE_LINE_SIZE)	// next isn't very contended, but we don't want it on the same cache line as tail (which is)
		weak_atomic<Block*> next;	// (Atomic)
		
		char* data;		// Contents (on heap) are aligned to T's alignment

		const size_t size;

		AE_FORCEINLINE size_t sizeMask() const { return size - 1; }


		// size must be a power of two (and greater than 0)
		Block(size_t const& _size)
			: front(0), tail(0), next(nullptr), size(_size)
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
		// C4512 - Assignment operator could not be generated
		Block& operator=(Block const&);

	private:
		char* rawData;
	};

private:
	AE_ALIGN(CACHE_LINE_SIZE)
	weak_atomic<Block*> frontBlock;		// (Atomic) Elements are enqueued to this block
	
	AE_ALIGN(CACHE_LINE_SIZE)
	weak_atomic<Block*> tailBlock;		// (Atomic) Elements are dequeued from this block

	AE_ALIGN(CACHE_LINE_SIZE)	// Ensure tailBlock gets its own cache line
	size_t largestBlockSize;

#ifndef NDEBUG
	bool enqueuing;
	bool dequeuing;
#endif
};

}    // end namespace moodycamel

#ifdef AE_VCPP
#pragma warning(pop)
#endif
