import uhd
import numpy as np
usrp = uhd.usrp.MultiUSRP()
samples = 0.1*np.random.randn(10000) + 0.1j*np.random.randn(10000) # create random signal
duration = 10 # seconds
center_freq = 418274940 # Hz
sample_rate = 1e6
gain = 20 # [dB] start low then work your way up
usrp.send_waveform(samples, duration, center_freq, sample_rate, [0], gain)
