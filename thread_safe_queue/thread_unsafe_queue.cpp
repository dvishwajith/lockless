#include <iostream>
#include <atomic>
#include <vector>
#include <thread>

#define QUEUE_SIZE 5

uint head {0};
uint tail {0};

int data[QUEUE_SIZE] = {0xffffff};

bool enqueue(int value) {
  // compare the tail + 1 instead tail
  // This will reduce the QUEUE size effecvely by 1
  // and notify QUEUE is full before it is actually full.
  // This allow as to use head == tail as empty condition
  // and tail + 1 == head as full condition.
  u_int next_tail = (tail + 1) % QUEUE_SIZE;
  if (next_tail == head) {
    return false;  // queue is full
  }
  data[tail % QUEUE_SIZE] = value;
  tail = next_tail;
  return true;
}

bool dequeue(int &value) {
  if (head % QUEUE_SIZE == tail) {
    return false;  // queue is empty
  }
  value = data[head % QUEUE_SIZE];
  head = (head + 1) % QUEUE_SIZE;
  return true;
}

#define TEST_SIZE 20000000

bool test_mpsc_queue() {
  bool passed = true;
  std::vector<int> test_values;
  std::thread producer_thread([&] () {
    for (size_t i = 0; i < TEST_SIZE; i++)
    {
      while(!enqueue(i)) {
        std::this_thread::yield();
      }
      test_values.push_back(i);
    }
    
  });

  std::vector<int> results;
  std::thread consumer_thread([&] {
    for (size_t i = 0; i < TEST_SIZE; i++)
    {
      int value = 123456789;
      while(!dequeue(value)) {
        std::this_thread::yield();
      }
      results.push_back(value);
    }
  });
  
  producer_thread.join();
  consumer_thread.join();
  if (test_values == results) {
    std::cout << "Success\n";
  } else {
    passed = false;
    std::cout << "Input and output vector mismatch\n";
    std::cout << "inputs:\n";
    if (test_values.size() < 50) {
      for (auto &d : test_values) {
        std::cout << d << " ";
      }
      std::cout << "\nresults:\n";
      for (auto &d : results) {
        std::cout << d << " ";
      }
    }
  }
  return passed;
}

int main() {
  auto start_time = std::chrono::steady_clock::now();
  auto end_time = start_time + std::chrono::hours(1);

  int test_count = 0;
  while (std::chrono::steady_clock::now() < end_time) {
      if (!test_mpsc_queue()) {
        std::cout << "test " << test_count << "failed\n";
        return -1;
      }
      test_count++;
  }

  std::cout << "Completed " << test_count << " tests in 1 hour." << std::endl;
  return 0;
}