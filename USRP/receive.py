#!/usr/bin/env python 
import uhd
import numpy as np

usrp = uhd.usrp.MultiUSRP() #can put num_recv_frames = 1000 to recieve at a higher rate
num_samps = 10000
freq = 4218274940
sample_rate = 1e6
gain = 20
samples = usrp.recv_num_samps(num_samps, freq, sample_rate, [0], gain) # units: N, Hz, list of channel IDs, dB
print(samples[0:10])

