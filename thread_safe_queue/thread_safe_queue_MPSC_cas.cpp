#include <iostream>
#include <atomic>
#include <vector>
#include <thread>

#define QUEUE_SIZE 10

std::atomic<u_int> head {0};
std::atomic<u_int> tail {0};

struct Node
{
  std::atomic<int> ready; // 0 = Empty, 1 = full
  int data;
};


Node data[QUEUE_SIZE];

bool enqueue(int value) {
  // compare the tail + 1 instead tail
  // This will reduce the QUEUE size effecvely by 1
  // and notify QUEUE is full before it is actually full.
  // This allow as to use head == tail as empty condition
  // and tail + 1 == head as full condition.
  u_int current_tail, next_tail;
  do {
    current_tail = tail.load(std::memory_order_relaxed);
    next_tail = (current_tail + 1) % QUEUE_SIZE;
    if (next_tail == head.load(std::memory_order_acquire)) {
      return false;  // queue is full
    }
  } while (!tail.compare_exchange_weak(current_tail, next_tail, std::memory_order_acq_rel));
  data[next_tail].data = value;
  data[next_tail].ready.store(1, std::memory_order_release);
  return true;
}

bool dequeue(int &value) {
  u_int currentHead = head.load(std::memory_order_relaxed) % QUEUE_SIZE;
  if (currentHead == tail.load(std::memory_order_acquire)) {
    return false;  // queue is empty
  }
  // if (!data[currentHead].ready.load(std::memory_order_acquire)) {
  //   return false;
  // }
  value = data[currentHead].data;
  data[currentHead].ready.store(0, std::memory_order_release);
  head.store((currentHead + 1) % QUEUE_SIZE, std::memory_order_release);
  return true;
}

#define TEST_SIZE 20
#define NUMBER_OF_PRODUCERS 4

bool test_mpsc_queue() {
  bool passed = true;
  std::vector<std::thread> producers;
  for (size_t i = 0; i < NUMBER_OF_PRODUCERS; i++)
  {
    producers.emplace_back([&] () {
      for (size_t i = 1; i < TEST_SIZE; i++) {
        while(!enqueue(i)) {
          std::this_thread::yield();
        }
      }
      
    });    
  }
  


  std::unordered_map<int, int> results;
  std::thread consumer_thread([&] {
    for (size_t i = 1; i < TEST_SIZE - 1; i++) {
      int value = 123456789;
      while(!dequeue(value)) {
        std::this_thread::yield();
      }
      auto it = results.find(value);
      if (it == results.end()) {
        results[value] = 1;
      } else {
        results[value]++;
      }
    }
  });

  for (auto &p : producers) {
    p.join();
  }
  consumer_thread.join();
  for (size_t i = 1; i < TEST_SIZE - 1; i++) {
    if (results[i] != NUMBER_OF_PRODUCERS) {
      passed = false;
      std::cout << "Failed: test value " << i << " count " << results[i] << "\n";
      break;
    }
  }
  if (passed) {
    std::cout << "Success\n";
  }
  return passed;
}

int main() {
  auto start_time = std::chrono::steady_clock::now();
  auto end_time = start_time + std::chrono::hours(1);

  int test_count = 0;
  // while (std::chrono::steady_clock::now() < end_time) {
      if (!test_mpsc_queue()) {
        std::cout << "test " << test_count << " failed\n";
        return -1;
      }
      test_count++;
  // }

  std::cout << "Completed " << test_count << " tests in 1 hour." << std::endl;
  return 0;
}