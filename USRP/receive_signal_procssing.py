#!/usr/bin/env python 
import uhd
import numpy as np
import matplotlib.pyplot as plt
from Signals.func import *

usrp = uhd.usrp.MultiUSRP() #can put num_recv_frames = 1000 to recieve at a higher rate
num_samps = 10000
freq = 418274940
sample_rate = 1e7
gain = 20


processor = SignalProcessing(data_length=104)

iq_data = usrp.recv_num_samps(num_samps, freq, sample_rate, [0], gain) # units: N, Hz, list of channel IDs, dB
np.savetxt("samples_output.txt", iq_data, delimiter=',')

iq_data = np.ravel(iq_data)
processor.data_length = len(iq_data)
print(processor.decode(iq_data))

