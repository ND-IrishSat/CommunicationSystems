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

// Push a new data element to the back of the ring buffer
// Returns a pointer to the new element on success
// Returns a null pointer on failure
ringBufferDataElement* pushToRingBuffer(RingBuffer *ringBuff, ringBufferDataElement newData)
{
  // If ring buffer is full, dont push
  if (isRingBufferFull(ringBuff)) {return NULL;}

  // Else, push to the ring buffer and increment the tail
  ringBuff->bufferDataArray[ringBuff->tail] = newData;
  ringBuff->tail = (ringBuff->tail + 1) % (RING_BUFFER_SIZE + 1);
  ringBuff->size++;
  return &ringBuff->bufferDataArray[ringBuff->tail];
}

// Remove the top element from the ring buffer and advance the head
// Does nothing if the buffer is empty
void popFromRingBuffer(RingBuffer *ringBuff)
{
  // If the ringBuffer is empty, return a NULL poointer
  ringBufferDataElement poppedElement;
  if (isRingBufferEmpty(ringBuff)) 
  {
    return;
  }

  // Move the head pointer forward
  ringBuff->head = (ringBuff->head + 1) % (RING_BUFFER_SIZE + 1);
  ringBuff->size--;

  return;
}

// Return the top element of the ring buffer without removing it
// Returns a null pointer if buffer is empty
ringBufferDataElement* peekRingBuffer(RingBuffer *ringBuff)
{
  ringBufferDataElement* poppedElement = NULL;
  if (isRingBufferEmpty(ringBuff)) 
  {
    return NULL;
  }

  poppedElement = &ringBuff->bufferDataArray[ringBuff->head];
  return poppedElement;
}

// Return the element of the ring buffer at a certiain index
// Returns a null pointer if index is out of bounds
ringBufferDataElement* indexRingBuffer(RingBuffer *ringBuff, int bufferIndex)
{
  ringBufferDataElement *indexedElement = NULL;

  // Check if index exceeds the size
  if (bufferIndex >= ringBuff->size)
  {
    return NULL;
  }

  // Adjust the index based on where the head is
  int calculatedIndex = (ringBuff->head + bufferIndex) % (RING_BUFFER_SIZE + 1);
  indexedElement = &ringBuff->bufferDataArray[calculatedIndex];
  return indexedElement;
}