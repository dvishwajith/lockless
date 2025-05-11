#include <iostream>
#include <atomic>
#include <vector>
#include <thread>
#include <unordered_map>

#include <thread>
#include <pthread.h>
#include <sched.h>
#include <iostream>

void pin_thread_to_cpu(std::thread& t, int cpu_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_id, &cpuset);

  int rc = pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
      std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
  }
}


// // incase we need to test all threads on same core
// #include <mach/mach.h>
// void set_thread_affinity(int tag) {
//     thread_port_t thread = pthread_mach_thread_np(pthread_self());
//     thread_affinity_policy_data_t policy = {tag};  // Set affinity tag
//     thread_policy_set(thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
// }



#define QUEUE_SIZE 20

std::atomic<u_int> head {0};
std::atomic<u_int> tail {0};

struct Node
{
  std::atomic<bool> ready; // 0 = Empty, 1 = full
  std::atomic<int> data; // Make data atomic to avoid race condition
};


Node data[QUEUE_SIZE] = {};

bool enqueue(int value) {
  // compare the tail + 1 instead tail
  // This will reduce the QUEUE size effecvely by 1
  // and notify QUEUE is full before it is actually full.
  // This allow as to use head == tail as empty condition
  // and tail + 1 == head as full condition.
  u_int current_tail = 0, next_tail = 0;
  bool ready = false, current_ready = false;
  do {
    current_tail = tail.load(std::memory_order_seq_cst);
    current_ready = data[current_tail].ready.load(std::memory_order_seq_cst);
    // if (current_ready) {
    //   // std::cout << "Queue might be full, current ready " << current_tail << "\n";
    //   return false;  // queue is full
    // }
    next_tail = (current_tail + 1) % QUEUE_SIZE;
    std::atomic_thread_fence(std::memory_order_seq_cst); // Full memory barrier
    if ((next_tail + 5) % QUEUE_SIZE == head.load(std::memory_order_seq_cst) || current_ready) {
      return false;  // queue is full
    }
    std::atomic_thread_fence(std::memory_order_seq_cst); // Full memory barrier
  } while (!tail.compare_exchange_weak(current_tail, next_tail));
  data[current_tail].data.store(value, std::memory_order_seq_cst);
  std::atomic_thread_fence(std::memory_order_seq_cst); // Full memory barrier
  data[current_tail].ready.compare_exchange_strong(ready, true, std::memory_order_seq_cst);
  // data[current_tail].ready.store(1, std::memory_order_release);
  return true;
}

int dequeue(int &value) {
  u_int currentHead = head.load(std::memory_order_seq_cst);
  std::atomic_thread_fence(std::memory_order_seq_cst); // Full memory barrier
  u_int currentTail = tail.load(std::memory_order_seq_cst);
  if (currentHead == currentTail) {
    return -1;  // queue is empty
  }
  if (!data[currentHead].ready.load(std::memory_order_seq_cst)) {
    return -2;
  }
  std::atomic_thread_fence(std::memory_order_seq_cst); // Full memory barrier
  value = data[currentHead].data;

  bool ready = true;
  std::atomic_thread_fence(std::memory_order_seq_cst); // Full memory barrier
  data[currentHead].ready.compare_exchange_strong(ready, false, std::memory_order_seq_cst);

  u_int next_head = (currentHead + 1) % QUEUE_SIZE;
  std::atomic_thread_fence(std::memory_order_seq_cst); // Full memory barrier
  // data[currentHead].ready.store(0, std::memory_order_release);
  head.store(next_head, std::memory_order_seq_cst);
  return 0;
}

#define TEST_SIZE 50
#define NUMBER_OF_PRODUCERS 4

bool test_mpsc_queue() {
  bool passed = true;
  std::vector<std::thread> producers;
  for (int producer_index = 0; producer_index < NUMBER_OF_PRODUCERS; producer_index++)
  {
    producers.emplace_back([producer_index] () {
      for (int i = 0; i < TEST_SIZE; i++) {
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + std::chrono::seconds(20);
        // printf("producer %d writing index %zu encoded_value %d\n", producer_index, i, producer_index*TEST_SIZE+i);
        while(!enqueue(producer_index*TEST_SIZE+i)) {
           std::this_thread::yield();
          //std::this_thread::sleep_for(std::chrono::nanoseconds(5));
          if (std::chrono::steady_clock::now() > end_time) {
            std::cout << "producer wait exceeded when writing index " << i << " true index " << producer_index << "\n";
            printf("head %u tail %u\n", head.load(), tail.load());
            for (int j = 0; j < QUEUE_SIZE; j++) {
              printf("data[%zu] %d %d\n", j, data[j].ready.load(), data[j].data.load());
            }
            break;
          }
        }
      }
    });    
  }
  


  std::unordered_map<int, int> results;
  std::thread consumer_thread([&] {

    for (size_t i = 0; i < TEST_SIZE*NUMBER_OF_PRODUCERS; i++) {
      auto start_time = std::chrono::steady_clock::now();
      auto end_time = start_time + std::chrono::seconds(5);
      int value = 123456789;
      int ret = -10;
      uint empty_count = 0;
      uint not_ready_count = 0;
      ret = dequeue(value);
      while(ret != 0) {
        if (ret == -1) {
          empty_count++;
        } else if (ret == -2) {
          not_ready_count++;
        }
        if (std::chrono::steady_clock::now() > end_time) {
          std::cout << "consumer wait exceeded when reading index " << i << " true index " << i/NUMBER_OF_PRODUCERS << " empty_count " << empty_count << " not_ready_count " << not_ready_count << "\n";
          printf("head %u tail %u\n", head.load(), tail.load());
          for (size_t j = 0; j < QUEUE_SIZE; j++) {
            printf("data[%zu] %d %d\n", j, data[j].ready.load(), data[j].data.load());
          }
          break;
        }
        std::this_thread::yield();
        // std::this_thread::sleep_for(std::chrono::nanoseconds(5));
        ret = dequeue(value);
      }
      if (std::chrono::steady_clock::now() > end_time) {
        break;
      }
      // std::cout << "dequeu val " << value << "\n";
      value = value % TEST_SIZE;
      auto it = results.find(value);
      if (it == results.end()) {
        results[value] = 1;
      } else {
        results[value]++;
      }
    }
  });

  int cpu = 0;
  pin_thread_to_cpu(consumer_thread, cpu++);
  consumer_thread.join();

  for (auto &p : producers) {
    pin_thread_to_cpu(p, cpu++);
    p.join();
  }
  
  for (size_t i = 0; i < TEST_SIZE ; i++) {
    if (results[i] != NUMBER_OF_PRODUCERS) {
      passed = false;
      std::cout << "Failed: test value " << i << " count " << results[i] << "size of result vector " << results.size() << "\n";
      break;
    } else {
      // std::cout << "        test value " << i << " count " << results[i] << "\n";
    }
  }
  if (passed) {
    std::cout << "Success\n";
  }
  return passed;
}

int main() {
  auto start_time = std::chrono::steady_clock::now();
  auto end_time = start_time + std::chrono::minutes(10);

  int test_count = 0;
  while (std::chrono::steady_clock::now() < end_time) {
      if (!test_mpsc_queue()) {
        std::cout << "test " << test_count << " failed\n";
        return -1;
      }
      test_count++;
  }

  std::cout << "Completed " << test_count << " tests in 1 hour." << std::endl;
  return 0;
}