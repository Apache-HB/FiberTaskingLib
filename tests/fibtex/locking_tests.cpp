/**
 * FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2018
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ftl/fibtex.h"
#include "ftl/task_scheduler.h"

#include <atomic>
#include <gtest/gtest.h>

struct MutexData {
	MutexData(ftl::TaskScheduler *scheduler, std::size_t const startingNumber)
	        : CommonMutex(scheduler, 12), SecondMutex(scheduler, 12), Counter(startingNumber) {
	}

	ftl::Fibtex CommonMutex;
	ftl::Fibtex SecondMutex;
	std::atomic<std::size_t> Counter;
};

void LockGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	ftl::LockGuard<ftl::Fibtex> lg(data->CommonMutex);

	// Intentional non-atomic increment
	std::size_t value = data->Counter.load(std::memory_order_acquire);
	value += 1;
	data->Counter.store(value, std::memory_order_release);
}

void SpinLockGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	ftl::SpinLockGuard<ftl::Fibtex> lg(data->CommonMutex);

	// Intentional non-atomic increment
	std::size_t value = data->Counter.load(std::memory_order_acquire);
	value += 1;
	data->Counter.store(value, std::memory_order_release);
}

void InfiniteSpinLockGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	ftl::InfiniteSpinLockGuard<ftl::Fibtex> lg(data->CommonMutex);

	// Intentional non-atomic increment
	std::size_t value = data->Counter.load(std::memory_order_acquire);
	value += 1;
	data->Counter.store(value, std::memory_order_release);
}

void UniqueLockGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	ftl::UniqueLock<ftl::Fibtex> lock(data->CommonMutex, ftl::defer_lock);

	lock.lock();
	// Intentional non-atomic increment
	std::size_t value = data->Counter.load(std::memory_order_acquire);
	value += 1;
	data->Counter.store(value, std::memory_order_release);
	lock.unlock();

	lock.lock_spin();
	// Intentional non-atomic increment
	value = data->Counter.load(std::memory_order_acquire);
	value += 1;
	data->Counter.store(value, std::memory_order_release);
	lock.unlock();

	lock.lock_spin_infinite();
	// Intentional non-atomic increment
	value = data->Counter.load(std::memory_order_acquire);
	value += 1;
	data->Counter.store(value, std::memory_order_release);
	lock.unlock();
}

void ScopeGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	ftl::ScopedLock<ftl::Fibtex, ftl::Fibtex> scopedLock(false, data->CommonMutex, data->SecondMutex);

	// Intentional non-atomic increment
	std::size_t value = data->Counter.load(std::memory_order_acquire);
	value += 1;
	data->Counter.store(value, std::memory_order_release);
}

void FutexMainTask(ftl::TaskScheduler *taskScheduler, void *arg) {
	auto &md = *reinterpret_cast<MutexData *>(arg);

	ftl::AtomicCounter c(taskScheduler);

	constexpr std::size_t iterations = 2000;
	for (std::size_t i = 0; i < iterations; ++i) {
		taskScheduler->AddTask(ftl::Task{LockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{LockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{SpinLockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{SpinLockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{InfiniteSpinLockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{InfiniteSpinLockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{UniqueLockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{UniqueLockGuardTest, &md}, &c);
		//		taskScheduler->AddTask(ftl::Task{ScopeGuardTest, &md}, &c);
		//		taskScheduler->AddTask(ftl::Task{ScopeGuardTest, &md}, &c);

		taskScheduler->WaitForCounter(&c, 0);
	}

	GTEST_ASSERT_EQ(md.Counter.load(std::memory_order_acquire), 6 * 2 * iterations);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FunctionalTests, LockingTests) {
	ftl::TaskScheduler taskScheduler;
	MutexData md(&taskScheduler, 0);
	taskScheduler.Run(400, FutexMainTask, &md, 4, ftl::EmptyQueueBehavior::Yield);
}
