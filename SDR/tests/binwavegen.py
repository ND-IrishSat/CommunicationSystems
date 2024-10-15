
import numpy as np
import struct

# Parameters
frequency = 155000  # Frequency of the cosine wave in Hz
sample_rate = 1000000  # Sample rate in Hz
duration = 10  # Duration in seconds
num_samples = int(sample_rate * duration)

# Generate time array and cosine wave
t = np.arange(num_samples) / sample_rate
cos_wave = 0.8 * np.cos(2 * np.pi * frequency * t)  # Amplitude scaled for 16-bit

# Quantize the cosine wave and create the binary data
q_wave = np.zeros_like(cos_wave)  # Q channel is zero for a real wave
i_samples = (cos_wave * (2**15 - 1)).astype(np.int16)
q_samples = (q_wave * (2**15 - 1)).astype(np.int16)

# Interleave I and Q samples and write to file
with open('cos_wave_' + str(frequency) + 'Hz_1MHz.bin', 'wb') as f:
    for i, q in zip(i_samples, q_samples):
        f.write(struct.pack('<hh', i, q))
