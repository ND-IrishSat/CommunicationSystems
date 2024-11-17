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

void initializeRingBuffer(RingBuffer *ringBuff);

bool isRingBufferEmpty(RingBuffer *ringBuff);

bool isRingBufferFull(RingBuffer *ringBuff);

ringBufferDataElement* pushToRingBuffer(RingBuffer *ringBuff, ringBufferDataElement newData);

void popFromRingBuffer(RingBuffer *ringBuff);

ringBufferDataElement* peekRingBuffer(RingBuffer *ringBuff);

ringBufferDataElement* indexRingBuffer(RingBuffer *ringBuff, int bufferIndex);