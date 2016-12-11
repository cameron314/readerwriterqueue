#include <readerwriterqueue/readerwriterqueue.h>
#include <iostream>
#include <cassert>

using namespace moodycamel;

int main()
{
    ReaderWriterQueue<int> queue;

    bool successEnqueue = queue.try_enqueue(42);
    int i;
    bool successDequeue = queue.try_dequeue(i);

    if(successEnqueue && successDequeue)
    {
        assert(i == 42);
        std::cout << i << std::endl;
    }
}
