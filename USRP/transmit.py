
import uhd
import numpy as np
usrp = uhd.usrp.MultiUSRP()
sample_rate = 1e7
time = np.arange(0,1,1.0/10000)
msg_freq = 3000000 # 2MHz 
samples = np.sin(2.0 * np.pi * msg_freq * time) # create random signal
duration = 10 # seconds
center_freq = 428274940 # Hz
gain = 50 # [dB] start low then work your way up
usrp.send_waveform(samples, duration, center_freq, sample_rate, [0], gain)