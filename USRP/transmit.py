import uhd
import numpy as np
usrp = uhd.usrp.MultiUSRP()
time = np.arange(0, 1, (float(1) / float(10000)))
samples = 0.1*np.sin(time) + 0 # create random signal
duration = 10 # seconds
center_freq = 418274940 # Hz
sample_rate = 1e6
gain = 20 # [dB] start low then work your way up
usrp.send_waveform(samples, duration, center_freq, sample_rate, [0], gain)
