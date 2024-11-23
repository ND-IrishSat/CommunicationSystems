#include "RingBuffer.h"
#include "stdio.h"

int main()
{
  RingBuffer testRingBuffer;
  int returnCode = 0;
  ringBufferDataElement inputElement = {0};
  ringBufferDataElement outputElement = {0};

  initializeRingBuffer(&testRingBuffer);

  /** Show that peek on an empty buffer returns null */
  returnCode = peekRingBuffer(&testRingBuffer, &outputElement);
  if (returnCode == 1) 
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
  returnCode = peekRingBuffer(&testRingBuffer, &outputElement);
  for (int i = 0; i < RING_BUFFER_DATA_ARRAY_SIZE; i++)
  {
    if (returnCode == 1)
    {
      printf("ERROR: peek did not return data\n");
      continue;
    }

    // Set the value of each index to 10 times itself
    if (inputElement.data[i].real != i * 10)
      printf("Pushed incorrect real data to ring buffer!\n");
    if (inputElement.data[i].imaginary != i * 30)
      printf("Pushed incorrect imaginary data to ring buffer!\n");
  }
  popFromRingBuffer(&testRingBuffer, &outputElement);

  /** Push to fill up the buffer, checking the size */
  for (int buffIdx = 0; buffIdx < RING_BUFFER_SIZE; buffIdx++)
  {
    // Fill data and push
    for (int i = 0; i < RING_BUFFER_DATA_ARRAY_SIZE; i++)
    {
      inputElement.data[i].real = buffIdx * i * 2;
      inputElement.data[i].imaginary = buffIdx * i * 7;
    }
    returnCode = pushToRingBuffer(&testRingBuffer, inputElement);

    // Validate updated buffer size
    if (testRingBuffer.size != buffIdx + 1)
    {
      printf("ERROR: Size did not update correctly on push number %d\n", buffIdx+1);
      printf("          Size is: %d   | Size should be: %d\n", testRingBuffer.size, buffIdx+1);
    }

    // Check if indexing functions properly
    returnCode = indexRingBuffer(&testRingBuffer, &outputElement, buffIdx);
    if (returnCode == 2)
    {
      printf("ERROR: index was out of bounds of the buffer\n");
      continue;
    }

    if (returnCode == 1)
    {
      printf("ERROR: no element found at the given index\n");
      continue;
    }

    for (int i = 0; i < RING_BUFFER_DATA_ARRAY_SIZE; i++)
    {
      Complex complexData = outputElement.data[i];
      if (complexData.real != (buffIdx * i * 2))
      {
        printf("ERROR: Pushed incorrect real data\n");
      }
      if (complexData.imaginary != (buffIdx * i * 7))
      {
        printf("ERROR: Pushed incorrect imaginary data\n");
      }
    }
  }

  // Clear out the ring buffer
  for (int i = 0; i < RING_BUFFER_SIZE; i++)
    returnCode = popFromRingBuffer(&testRingBuffer, &outputElement);

  // Confirm that ring buffer is now empty
  if (!isRingBufferEmpty(&testRingBuffer))
    printf("ERROR: Size is not 0 for empty buffer\n");
}