#include <iostream>

#define QUEUE_SIZE 5

u_int head = 0;
u_int tail = 0;

int data[QUEUE_SIZE] = {0xffffff};

bool enqueue(int value) {
  // compare the tail + 1 instead tail
  // This will reduce the QUEUE size effecvely by 1
  // and notify QUEUE is full before it is actually full.
  // This allow as to use head == tail as empty condition
  // and tail + 1 == head as full condition. 
  if ((tail + 1) % QUEUE_SIZE == head) {
    return false;  // queue is full
  }
  data[tail % QUEUE_SIZE] = value;
  tail++;
  return true;
}

bool dequeue(int &value) {
  if (head % QUEUE_SIZE == tail) {
    return false;  // queue is empty
  }
  value = data[head % QUEUE_SIZE];
  head = head + 1;
  return true;
}

int main() {
  for (int i = 0; i < QUEUE_SIZE + 1; i++) {
    auto success = enqueue(i);
    printf("enqueue status %d value %d\n", success, i);
  }

  for (size_t i = 0; i < QUEUE_SIZE; i++) {
    int value = 4321;
    auto success = dequeue(value);
    printf("dequeu status %d value %d\n", success, value);
  }

  return 0;
}