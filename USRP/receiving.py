#!/usr/bin/env python

import uhd
import numpy as np
import matplotlib.pyplot as plt

usrp = uhd.usrp.MultiUSRP() #can put num_recv_frames = 1000 to recieve at a higher rate
samples = usrp.recv_num_samps(10000, 100e6, 1e6, [0], 50) # units: N, Hz, list of channel IDs, dB
print(samples[0:10])
# Convert the samples to a NumPy array (if not already)
samples_array = np.array(samples)

# Plot the real and imaginary parts of the samples
plt.figure()
plt.plot(np.real(samples_array), label="Real part")
plt.plot(np.imag(samples_array), label="Imaginary part")
plt.title('Received USRP Samples')
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.legend()
plt.show()
