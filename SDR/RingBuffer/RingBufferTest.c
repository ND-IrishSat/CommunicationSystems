#include "RingBuffer.h"
#include "stdio.h"

int main()
{
  RingBuffer testRingBuffer;
  int testing = 0;
  ringBufferDataElement inputElement = {0};
  ringBufferDataElement* outputElement = NULL;

  initializeRingBuffer(&testRingBuffer);

  /** Show that peek on an empty buffer returns null */
  outputElement = peekRingBuffer(&testRingBuffer);
  if (outputElement == NULL) 
    printf("Peek On Null Success!\n");
  else 
    printf("Peek On Null Failure!\n");


  /** Show that a single push works */
  for (int i = 0; i < RING_BUFFER_DATA_ARRAY_SIZE; i++)
  {
    // Set the value of each index to 10 times itself
    inputElement.data[i].real = i * 10;
    inputElement.data[i].imaginary = i * 30;
  }
  pushToRingBuffer(&testRingBuffer, inputElement);
  outputElement = peekRingBuffer(&testRingBuffer);
  for (int i = 0; i < RING_BUFFER_DATA_ARRAY_SIZE; i++)
  {
    if (outputElement == NULL)
    {
      printf("ERROR: null pointer");
      continue;
    }

    // Set the value of each index to 10 times itself
    if (inputElement.data[i].real != i * 10)
      printf("Pushed incorrect real data to ring buffer!");
    if (inputElement.data[i].imaginary != i * 30)
      printf("Pushed incorrect imaginary data to ring buffer!");
  }
  popFromRingBuffer(&testRingBuffer);

  /** Push to fill up the buffer, checking the size */
  for (int buffIdx = 0; buffIdx < RING_BUFFER_SIZE; buffIdx++)
  {
    // Fill data and push
    for (int i = 0; i < RING_BUFFER_DATA_ARRAY_SIZE; i++)
    {
      inputElement.data[i].real = i * 2;
      inputElement.data[i].imaginary = i * 7;
    }
    pushToRingBuffer(&testRingBuffer, inputElement);

    // Validate updated buffer size
    if (testRingBuffer.size != buffIdx + 1)
    {
      printf("ERROR: Size did not update correctly on push number %d", buffIdx+1);
      printf("          Size is: %d   | Size should be: %d", testRingBuffer.size, buffIdx+1);
    }

    outputElement = indexRingBuffer(&testRingBuffer, buffIdx);
    for (int i = 0; i < RING_BUFFER_DATA_ARRAY_SIZE; i++)
    {
      if (outputElement == NULL)
      {
        printf("ERROR: null pointer");
        continue;
      }

      Complex complexData = inputElement.data[i];
      if (complexData.real != (buffIdx * i * 2))
      {
        printf("Pushed incorrect real data");
      }
      if (complexData.imaginary != (buffIdx * i * 7))
      {
        printf("Pushed incorrect imaginary data");
      }
    }
  }

  // Clear out the ring buffer
  for (int i = 0; i < RING_BUFFER_SIZE; i++)
    popFromRingBuffer(&testRingBuffer);
  // Confirm that ring buffer is now empty
  if (!isRingBufferEmpty(&testRingBuffer))
    printf("ERROR: Size is not 0 for empty buffer");
}