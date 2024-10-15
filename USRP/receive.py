#!/usr/bin/env python 
import uhd
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft, fftshift, fftfreq
from scipy import signal

usrp = uhd.usrp.MultiUSRP() #can put num_recv_frames = 1000 to recieve at a higher rate
num_samps = 50000
freq = 4218274940
sample_rate = 1e6
gain = 20

iq_data = usrp.recv_num_samps(num_samps, freq, sample_rate, [0], gain) # units: N, Hz, list of channel IDs, dB
np.savetxt("samples_output.txt", iq_data, delimiter=',')


def plot_iq_data_and_frequency(iq_data, sample_rate):

    # Compute the FFT of the complex I/Q data
    Fs=sample_rate
    N = len(iq_data)  # Number of samples
    X = np.fft.fftshift(np.fft.fft(iq_data))
    #X *= np.hamming(N)
    X_mag = 10*np.log10(np.abs(X)**2)
    f = np.arange(Fs/-2.0, Fs/2.0, Fs/N)/1e6 # plt in MHz
    plt.plot(f, X_mag)
    plt.title('Frequency Domain')
    plt.xlabel('Frequency [MHz]')
    plt.ylabel('Magnitude [dB]')
    plt.grid()

    plt.tight_layout()
    plt.show()



# Plot the time domain (I/Q) and frequency domain
plot_iq_data_and_frequency(iq_data, sample_rate)

