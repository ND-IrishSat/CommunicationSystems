#include <stdbool.h>

// Define the size N of the ring buffer
#define RING_BUFFER_SIZE 20
// Defines the size of the data in each element in the ring buffer
#define RING_BUFFER_DATA_ARRAY_SIZE 255

/** Union to determine possible data types that can go into the array */
typedef struct
{
  double real;
  double imaginary;
} Complex;

/** Define the array elements as a fixed size array of complex numbers */
typedef struct ringBufferDataElement 
{
  Complex data[RING_BUFFER_DATA_ARRAY_SIZE];
} ringBufferDataElement;

typedef struct RingBuffer{
  // Create an array of N + 1 elements for the ring buffer
  // We add an extra element to simplify the math of the ring buffer operations
  ringBufferDataElement bufferDataArray[RING_BUFFER_SIZE + 1];

  int head; // Index to first element in data array
  int tail; // Index to last element in data array
  int size; // Number of elements in data array
} RingBuffer;  

// Allocates a stack based ring buffer to avoid heap fragmentation
void initializeRingBuffer(RingBuffer *ringBuff);

// Returns:
//  True: if empty
//  False: otherwise
bool isRingBufferEmpty(RingBuffer *ringBuff);

// Returns:
//  True: if full
//  False: otherwise
bool isRingBufferFull(RingBuffer *ringBuff);

// Getting the current number of elements will be useful for knowing indexing bounds
bool getRingBufferSize(RingBuffer *ringBuff);

// Pushes an input element onto the end of the ring buffer.
// Returns:
//  0: push was successful
//  1: push was unsuccessful, ring buffer is full
int pushToRingBuffer(RingBuffer *ringBuff, ringBufferDataElement newData);

// Removes the front element from the ring buffer and copies the data to the passed output element
// Returns:
//  0: pop was successful
//  1: pop was unsuccessful, no element to pop
int popFromRingBuffer(RingBuffer *ringBuff, ringBufferDataElement* outputElement);

// Copies the data of the front element into the passed output element without popping it
// Returns:
//  0: peek was successful
//  1: peek was unsuccessful, no element to peek
int peekRingBuffer(RingBuffer *ringBuff, ringBufferDataElement* outputElement);

// Copies the data of the indexed element into the passed output element without popping it
// Returns:
//  0: indexing was successful
//  1: indexing was unsuccessful, no element at index
//  2: indexing was unsuccessful, index out of bounds
int indexRingBuffer(RingBuffer *ringBuff, ringBufferDataElement* outputElement, int bufferIndex);