/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2016
 */

#include "fiber_tasking_lib/task_scheduler.h"
#include "fiber_tasking_lib/tagged_heap_backed_linear_allocator.h"

#include <gtest/gtest.h>


struct NumberSubset {
	uint64 start;
	uint64 end;

	uint64 total;
};


TASK_ENTRY_POINT(AddNumberSubset) {
	NumberSubset *subset = (NumberSubset *)arg;

	subset->total = 0;

	while (subset->start != subset->end) {
		subset->total += subset->start;
		++subset->start;
	}

	subset->total += subset->end;
}


/**
 * Calculates the value of a triangle number by dividing the additions up into tasks
 *
 * A triangle number is defined as:
 *         Tn = 1 + 2 + 3 + ... + n
 *
 * The code is checked against the numerical solution which is:
 *         Tn = n * (n + 1) / 2
 *
 * TODO: Use gtest's 'Value Paramaterized Tests' to test multiple triangle numbers
 */
TEST(FunctionalTests, CalcTriangleNum) {
	FiberTaskingLib::TaskScheduler *taskScheduler = new FiberTaskingLib::TaskScheduler();
	taskScheduler->Initialize(400);

	FiberTaskingLib::TaggedHeap *taggedHeap = new FiberTaskingLib::TaggedHeap(2097152);
	FiberTaskingLib::TaggedHeapBackedLinearAllocator *allocator = new FiberTaskingLib::TaggedHeapBackedLinearAllocator();
	allocator->init(taggedHeap, 1234);

	// Define the constants to test
	const uint64 triangleNum = 47593243ull;
	const uint64 numAdditionsPerTask = 10000ull;
	const uint64 numTasks = (triangleNum + numAdditionsPerTask - 1ull) / numAdditionsPerTask;

	// Create the tasks
	FiberTaskingLib::Task *tasks = new FiberTaskingLib::Task[numTasks];
	// We have to declare this on the heap so other threads can access it
	NumberSubset *subsets = new NumberSubset[numTasks];
	uint64 nextNumber = 1ull;

	for (uint64 i = 0ull; i < numTasks; ++i) {
		NumberSubset *subset = &subsets[i];

		subset->start = nextNumber;
		subset->end = nextNumber + numAdditionsPerTask - 1ull;
		if (subset->end > triangleNum) {
			subset->end = triangleNum;
		}

		tasks[i] = {AddNumberSubset, subset};

		nextNumber = subset->end + 1;
	}

	// Schedule the tasks and wait for them to complete
	std::shared_ptr<FiberTaskingLib::AtomicCounter> counter = taskScheduler->AddTasks(numTasks, tasks);
	delete[] tasks;

	taskScheduler->WaitForCounter(counter, 0);


	// Add the results
	uint64 result = 0ull;
	for (uint64 i = 0; i < numTasks; ++i) {
		result += subsets[i].total;
	}

	// Test
	GTEST_ASSERT_EQ(triangleNum * (triangleNum + 1ull) / 2ull, result);

	taskScheduler->Quit();

	// Cleanup
	allocator->destroy();
	delete allocator;
	delete taggedHeap;
	delete taskScheduler;
}
