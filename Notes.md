# SPSC Lockfree Queue

For SPSC Queues with a fixed storage type, the implementation follows the approach of using a write index and a read index. The optimizations are as follows:

+ Constrain the buffer size to be a power of 2, and the compiler will use "(x & (SIZE - 1))" to replace "(X % SIZE)".

+ Use a cached read index in the producer thread. Only update the cached read index to the latest read index when it is used to determine that the queue is full.

+ Consider adding a boolean value at the head of each block to indicate whether the memory is readable. This way, the consumer thread does not need to read the write index; it only needs to read this flag to determine if the queue is empty. This avoids the overhead of cache coherence caused by sharing the write index, and since the boolean flag is distributed across each block, the probability of collision is very low.