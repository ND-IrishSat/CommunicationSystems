#include <stdio.h>
#include "RingBuffer.h"

void initializeRingBuffer(RingBuffer *ringBuff)
{
  // Set the head and tail to the first element
  ringBuff->head = 0;
  ringBuff->tail = 0;
  ringBuff->size = 0;
}

bool isRingBufferEmpty(RingBuffer *ringBuff)
{
  return (ringBuff->head == ringBuff->tail);
}

bool isRingBufferFull(RingBuffer *ringBuff)
{
  return ( ((ringBuff->tail + 1) % (RING_BUFFER_SIZE + 1)) == ringBuff->head );
}

bool getRingBufferSize(RingBuffer *ringBuff)
{
  return ringBuff->size;
}

int pushToRingBuffer(RingBuffer *ringBuff, ringBufferDataElement newData)
{
  // If ring buffer is full, dont push
  if (isRingBufferFull(ringBuff)) {return 1;}

  // Else, push to the ring buffer and increment the tail and size to reflect new element
  ringBuff->bufferDataArray[ringBuff->tail] = newData;
  ringBuff->tail = (ringBuff->tail + 1) % (RING_BUFFER_SIZE + 1);
  ringBuff->size++;
  return 0;
}

// Remove the top element from the ring buffer and advance the head
// Does nothing if the buffer is empty
int popFromRingBuffer(RingBuffer *ringBuff, ringBufferDataElement* outputElement)
{
  // If the ringBuffer is empty, return error
  if (isRingBufferEmpty(ringBuff)) {return 1;}

  // Move the head pointer forward and copy the data to the output element
  *outputElement = ringBuff->bufferDataArray[ringBuff->head];
  ringBuff->head = (ringBuff->head + 1) % (RING_BUFFER_SIZE + 1);
  ringBuff->size--;
  return 0;
}

// Copies the data of the top element of the ring buffer without removing it
int peekRingBuffer(RingBuffer *ringBuff, ringBufferDataElement* outputElement)
{
  // If the ringBuffer is empty, return error
  if (isRingBufferEmpty(ringBuff)) {return 1;}

  *outputElement = ringBuff->bufferDataArray[ringBuff->head];
  return 0;
}

// Copies the data of the element of the ring buffer at a certain index
// Returns a null pointer if index is out of bounds
int indexRingBuffer(RingBuffer *ringBuff, ringBufferDataElement* outputElement, int bufferIndex)
{
  // Check if index exceeds the capacity
  if (bufferIndex >= RING_BUFFER_SIZE) {return 2;}

  // Check if index exceeds the size
  if (bufferIndex >= ringBuff->size) {return 1;}

  // Adjust the index based on where the head is and access
  int calculatedIndex = (ringBuff->head + bufferIndex) % (RING_BUFFER_SIZE + 1);
  *outputElement = ringBuff->bufferDataArray[calculatedIndex];
  return 0;
}