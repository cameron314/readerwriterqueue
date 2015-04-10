// ©2013-2015 Cameron Desrochers
// Unit tests for moodycamel::ReaderWriterQueue

#include <cstdio>
#include <cstdio>
#include <cstring>
#include <string>

#include "minitest.h"
#include "../common/simplethread.h"
#include "../../readerwriterqueue.h"

using namespace moodycamel;


// *NOT* thread-safe
struct Foo
{
	Foo() : copied(false) { id = _id()++; }
	Foo(Foo const& other) : id(other.id), copied(true) { }
	~Foo()
	{
		if (copied) return;
		if (id != _last_destroyed_id() + 1) {
			_destroyed_in_order() = false;
		}
		_last_destroyed_id() = id;
		++_destroy_count();
	}
	static void reset() { _destroy_count() = 0; _id() = 0; _destroyed_in_order() = true; _last_destroyed_id() = -1; }
	static int destroy_count() { return _destroy_count(); }
	static bool destroyed_in_order() { return _destroyed_in_order(); }
	
private:
	static int& _destroy_count() { static int c = 0; return c; }
	static int& _id() { static int i = 0; return i; }
	static bool& _destroyed_in_order() { static bool d = true; return d; }
	static int& _last_destroyed_id() { static int i = -1; return i; }
	
	int id;
	bool copied;
};



class ReaderWriterQueueTests : public TestClass<ReaderWriterQueueTests>
{
public:
	ReaderWriterQueueTests()
	{
		REGISTER_TEST(create_empty_queue);
		REGISTER_TEST(enqueue_one);
		REGISTER_TEST(enqueue_many);
		REGISTER_TEST(nonempty_destroy);
		REGISTER_TEST(try_enqueue);
		REGISTER_TEST(try_dequeue);
		REGISTER_TEST(peek);
		REGISTER_TEST(pop);
		REGISTER_TEST(size_approx);
		REGISTER_TEST(threaded);
		REGISTER_TEST(blocking);
	}
	
	bool create_empty_queue()
	{
		{
			ReaderWriterQueue<int> q;
		}
		
		{
			ReaderWriterQueue<int> q(1234);
		}
		
		return true;
	}
	
	bool enqueue_one()
	{
		int item;
		
		{
			item = 0;
			ReaderWriterQueue<int> q(1);
			q.enqueue(12345);
			ASSERT_OR_FAIL(q.try_dequeue(item));
			ASSERT_OR_FAIL(item == 12345);
		}
		
		{
			item = 0;
			ReaderWriterQueue<int> q(1);
			ASSERT_OR_FAIL(q.try_enqueue(12345));
			ASSERT_OR_FAIL(q.try_dequeue(item));
			ASSERT_OR_FAIL(item == 12345);
		}
		
		return true;
	}
	
	bool enqueue_many()
	{
		int item = -1;
		
		{
			ReaderWriterQueue<int> q(100);
			for (int i = 0; i != 100; ++i) {
				q.enqueue(i);
			}
			
			for (int i = 0; i != 100; ++i) {
				ASSERT_OR_FAIL(q.try_dequeue(item));
				ASSERT_OR_FAIL(item == i);
			}
		}
		
		{
			ReaderWriterQueue<int> q(100);
			for (int i = 0; i != 1200; ++i) {
				q.enqueue(i);
			}
			
			for (int i = 0; i != 1200; ++i) {
				ASSERT_OR_FAIL(q.try_dequeue(item));
				ASSERT_OR_FAIL(item == i);
			}
		}
		
		return true;
	}
	
	bool nonempty_destroy()
	{
		Foo item;
		
		// Some elements at beginning
		Foo::reset();
		{
			ReaderWriterQueue<Foo> q(31);
			for (int i = 0; i != 10; ++i) {
				q.enqueue(Foo());
			}
		}
		ASSERT_OR_FAIL(Foo::destroy_count() == 10);
		ASSERT_OR_FAIL(Foo::destroyed_in_order());
		
		// Entire block
		Foo::reset();
		{
			ReaderWriterQueue<Foo> q(31);
			for (int i = 0; i != 31; ++i) {
				q.enqueue(Foo());
			}
		}
		ASSERT_OR_FAIL(Foo::destroy_count() == 31);
		ASSERT_OR_FAIL(Foo::destroyed_in_order());
		
		// Multiple blocks
		Foo::reset();
		{
			ReaderWriterQueue<Foo> q(31);
			for (int i = 0; i != 94; ++i) {
				q.enqueue(Foo());
			}
		}
		ASSERT_OR_FAIL(Foo::destroy_count() == 94);
		ASSERT_OR_FAIL(Foo::destroyed_in_order());
		
		// Some elements in another block
		Foo::reset();
		{
			ReaderWriterQueue<Foo> q(31);
			for (int i = 0; i != 42; ++i) {
				q.enqueue(Foo());
			}
			for (int i = 0; i != 31; ++i) {
				ASSERT_OR_FAIL(q.try_dequeue(item));
			}
		}
		ASSERT_OR_FAIL(Foo::destroy_count() == 42);
		ASSERT_OR_FAIL(Foo::destroyed_in_order());
		
		// Some elements in multiple blocks
		Foo::reset();
		{
			ReaderWriterQueue<Foo> q(31);
			for (int i = 0; i != 123; ++i) {
				q.enqueue(Foo());
			}
			for (int i = 0; i != 25; ++i) {
				ASSERT_OR_FAIL(q.try_dequeue(item));
			}
			for (int i = 0; i != 47; ++i) {
				q.enqueue(Foo());
			}
			for (int i = 0; i != 140; ++i) {
				ASSERT_OR_FAIL(q.try_dequeue(item));
			}
			for (int i = 0; i != 230; ++i) {
				q.enqueue(Foo());
			}
			for (int i = 0; i != 130; ++i) {
				ASSERT_OR_FAIL(q.try_dequeue(item));
			}
			for (int i = 0; i != 100; ++i) {
				q.enqueue(Foo());
			}
		}
		ASSERT_OR_FAIL(Foo::destroy_count() == 500);
		ASSERT_OR_FAIL(Foo::destroyed_in_order());
		
		return true;
	}
	
	bool try_enqueue()
	{
		ReaderWriterQueue<int> q(31);
		int item;
		int size = 0;
		
		for (int i = 0; i < 10000; ++i) {
			if ((rand() & 1) == 1) {
				bool result = q.try_enqueue(i);
				if (size == 31) {
					ASSERT_OR_FAIL(!result);
				}
				else {
					ASSERT_OR_FAIL(result);
					++size;
				}
			}
			else {
				bool result = q.try_dequeue(item);
				if (size == 0) {
					ASSERT_OR_FAIL(!result);
				}
				else {
					ASSERT_OR_FAIL(result);
					--size;
				}
			}
		}
		
		return true;
	}
	
	bool try_dequeue()
	{
		int item;
		
		{
			ReaderWriterQueue<int> q(1);
			ASSERT_OR_FAIL(!q.try_dequeue(item));
		}
		
		{
			ReaderWriterQueue<int, 2> q(10);
			ASSERT_OR_FAIL(!q.try_dequeue(item));
		}
		
		return true;
	}
	
	bool threaded()
	{
		weak_atomic<int> result;
		result = 1;
		
		ReaderWriterQueue<int> q(100);
		SimpleThread reader([&]() {
			int item;
			int prevItem = -1;
			for (int i = 0; i != 1000000; ++i) {
				if (q.try_dequeue(item)) {
					if (item <= prevItem) {
						result = 0;
					}
					prevItem = item;
				}
			}
		});
		SimpleThread writer([&]() {
			for (int i = 0; i != 1000000; ++i) {
				if (((i >> 7) & 1) == 0) {
					q.enqueue(i);
				}
				else {
					q.try_enqueue(i);
				}
			}
		});
		
		writer.join();
		reader.join();
		
		return result.load() == 1 ? true : false;
	}

	bool peek()
	{
		weak_atomic<int> result;
		result = 1;
		
		ReaderWriterQueue<int> q(100);
		SimpleThread reader([&]() {
			int item;
			int prevItem = -1;
			int* peeked;
			for (int i = 0; i != 100000; ++i) {
				peeked = q.peek();
				if (peeked != nullptr) {
					if (q.try_dequeue(item)) {
						if (item <= prevItem || item != *peeked) {
							result = 0;
						}
						prevItem = item;
					}
					else {
						result = 0;
					}
				}
			}
		});
		SimpleThread writer([&]() {
			for (int i = 0; i != 100000; ++i) {
				if (((i >> 7) & 1) == 0) {
					q.enqueue(i);
				}
				else {
					q.try_enqueue(i);
				}
			}
		});
		
		writer.join();
		reader.join();
		
		return result.load() == 1 ? true : false;
	}
	
	bool pop()
	{
		weak_atomic<int> result;
		result = 1;
		
		ReaderWriterQueue<int> q(100);
		SimpleThread reader([&]() {
			int item;
			int prevItem = -1;
			int* peeked;
			for (int i = 0; i != 100000; ++i) {
				peeked = q.peek();
				if (peeked != nullptr) {
					item = *peeked;
					if (q.pop()) {
						if (item <= prevItem) {
							result = 0;
						}
						prevItem = item;
					}
					else {
						result = 0;
					}
				}
			}
		});
		SimpleThread writer([&]() {
			for (int i = 0; i != 100000; ++i) {
				if (((i >> 7) & 1) == 0) {
					q.enqueue(i);
				}
				else {
					q.try_enqueue(i);
				}
			}
		});
		
		writer.join();
		reader.join();
		
		return result.load() == 1 ? true : false;
	}
	
	bool size_approx()
	{
		weak_atomic<int> result;
		weak_atomic<int> front;
		weak_atomic<int> tail;
		
		result = 1;
		front = 0;
		tail = 0;
		
		ReaderWriterQueue<int> q(10);
		SimpleThread reader([&]() {
			int item;
			for (int i = 0; i != 100000; ++i) {
				if (q.try_dequeue(item)) {
					fence(memory_order_release);
					front = front.load() + 1;
				}
				int size = (int)q.size_approx();
				fence(memory_order_acquire);
				int tail_ = tail.load();
				int front_ = front.load();
				if (size > tail_ - front_ || size < 0) {
					result = 0;
				}
			}
		});
		SimpleThread writer([&]() {
			for (int i = 0; i != 100000; ++i) {
				tail = tail.load() + 1;
				fence(memory_order_release);
				q.enqueue(i);
				int tail_ = tail.load();
				int front_ = front.load();
				fence(memory_order_acquire);
				int size = (int)q.size_approx();
				if (size > tail_ - front_ || size < 0) {
					result = 0;
				}
			}
		});
		
		writer.join();
		reader.join();
		
		return result.load() == 1 ? true : false;
	}
	
	bool blocking()
	{
		{
			BlockingReaderWriterQueue<int> q;
			int item;
			
			q.enqueue(123);
			ASSERT_OR_FAIL(q.try_dequeue(item));
			ASSERT_OR_FAIL(item == 123);
			ASSERT_OR_FAIL(q.size_approx() == 0);
			
			q.enqueue(234);
			ASSERT_OR_FAIL(q.size_approx() == 1);
			ASSERT_OR_FAIL(*q.peek() == 234);
			ASSERT_OR_FAIL(*q.peek() == 234);
			ASSERT_OR_FAIL(q.pop());
			
			ASSERT_OR_FAIL(q.try_enqueue(345));
			q.wait_dequeue(item);
			ASSERT_OR_FAIL(item == 345);
			ASSERT_OR_FAIL(!q.peek());
			ASSERT_OR_FAIL(q.size_approx() == 0);
			ASSERT_OR_FAIL(!q.try_dequeue(item));
		}
		
		weak_atomic<int> result;
		result = 1;
		
		BlockingReaderWriterQueue<int> q(100);
		SimpleThread reader([&]() {
			int item = -1;
			int prevItem = -1;
			for (int i = 0; i != 1000000; ++i) {
				q.wait_dequeue(item);
				if (item <= prevItem) {
					result = 0;
				}
				prevItem = item;
			}
		});
		SimpleThread writer([&]() {
			for (int i = 0; i != 1000000; ++i) {
				q.enqueue(i);
			}
		});
		
		writer.join();
		reader.join();
		
		ASSERT_OR_FAIL(q.size_approx() == 0);
		
		return result.load() ? 1 : 0;
	}
};



void printTests(ReaderWriterQueueTests const& tests)
{
	std::printf("   Supported tests are:\n");
	
	std::vector<std::string> names;
	tests.getAllTestNames(names);
	for (auto it = names.cbegin(); it != names.cend(); ++it) {
		std::printf("      %s\n", it->c_str());
	}
}


// Basic test harness
int main(int argc, char** argv)
{
	bool disablePrompt = false;
	std::vector<std::string> selectedTests;
	
	// Disable buffering (so that when run in, e.g., Sublime Text, the output appears as it is written)
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	
	// Isolate the executable name
	std::string progName = argv[0];
	auto slash = progName.find_last_of("/\\");
	if (slash != std::string::npos) {
		progName = progName.substr(slash + 1);
	}
	
	ReaderWriterQueueTests tests;
	
	// Parse command line options
	if (argc == 1) {
		std::printf("Running all unit tests for moodycamel::ReaderWriterQueue.\n(Run %s --help for other options.)\n\n", progName.c_str());
	}
	else {
		bool printHelp = false;
		bool printedTests = false;
		bool error = false;
		for (int i = 1; i < argc; ++i) {
			if (std::strcmp(argv[i], "--help") == 0) {
				printHelp = true;
			}
			else if (std::strcmp(argv[i], "--disable-prompt") == 0) {
				disablePrompt = true;
			}
			else if (std::strcmp(argv[i], "--run") == 0) {
				if (i + 1 == argc || argv[i + 1][0] == '-') {
					std::printf("Expected test name argument for --run option.\n");
					if (!printedTests) {
						printTests(tests);
						printedTests = true;
					}
					error = true;
					continue;
				}
				
				if (!tests.validateTestName(argv[++i])) {
					std::printf("Unrecognized test '%s'.\n", argv[i]);
					if (!printedTests) {
						printTests(tests);
						printedTests = true;
					}
					error = true;
					continue;
				}
				
				selectedTests.push_back(argv[i]);
			}
			else {
				std::printf("Unrecognized option '%s'.\n", argv[i]);
				error = true;
			}
		}
		
		if (error || printHelp) {
			if (error) {
				std::printf("\n");
			}
			std::printf("%s\n    Description: Runs unit tests for moodycamel::ReaderWriterQueue\n", progName.c_str());
			std::printf("    --help            Prints this help blurb\n");
			std::printf("    --run test        Runs only the specified test(s)\n");
			std::printf("    --disable-prompt  Disables prompt before exit when the tests finish\n");
			return error ? -1 : 0;
		}
	}
	
	
	int exitCode = 0;
	
	bool result;
	if (selectedTests.size() > 0) {
		result = tests.run(selectedTests);
	}
	else {
		result = tests.run();
	}
	
	if (result) {
		std::printf("All %stests passed.\n", (selectedTests.size() > 0 ? "selected " : ""));
	}
	else {
		std::printf("Test(s) failed!\n");
		exitCode = 2;
	}
	
	if (!disablePrompt) {
		std::printf("Press ENTER to exit.\n");
		getchar();
	}
	return exitCode;
}

