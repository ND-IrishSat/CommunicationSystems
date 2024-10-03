#!/usr/bin/env python 
import uhd
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft, fftshift, fftfreq
from scipy import signal

usrp = uhd.usrp.MultiUSRP() #can put num_recv_frames = 1000 to recieve at a higher rate
num_samps = 10000
freq = 4218274940
sample_rate = 1e6
gain = 20
iq_data = usrp.recv_num_samps(num_samps, freq, sample_rate, [0], gain) # units: N, Hz, list of channel IDs, dB
print("Samples: " + iq_data)

def plot_iq_data_and_frequency(iq_data, sampling_rate):
    # Extract real and imaginary parts
    real_parts = np.real(iq_data)
    imaginary_parts = np.imag(iq_data)

    # Create a figure with three subplots
    fig = plt.figure(figsize=(12, 8))

    # First subplot: Real vs Imaginary (2D Scatter Plot)
    ax1 = fig.add_subplot(221)
    ax1.scatter(real_parts, imaginary_parts, color='blue', s=1)
    ax1.set_title('I vs Q (Time Domain)')
    ax1.set_xlabel('Real Part (I)')
    ax1.set_ylabel('Imaginary Part (Q)')
    ax1.grid(True)

    # Second subplot: Real, Imaginary, and Index (3D Scatter Plot)
    ax2 = fig.add_subplot(222, projection='3d')
    indices = np.arange(len(iq_data))  # Indices along the x-axis
    ax2.scatter(indices, real_parts, imaginary_parts, color='red', s=1)
    ax2.set_title('I and Q vs Index')
    ax2.set_xlabel('Index')
    ax2.set_ylabel('Real Part (I)')
    ax2.set_zlabel('Imaginary Part (Q)')
    ax2.grid(True)

    # Compute the FFT of the complex I/Q data
    N = len(iq_data)  # Number of samples
    yf = fft(iq_data)  # Compute the FFT
    xf = fftfreq(N, 1 / sampling_rate)  # Generate frequency bins
    xf = fftshift(xf)  # Shift zero frequency to the center
    yf = fftshift(yf)  # Shift the FFT output accordingly

    # Third subplot: Frequency Domain (Magnitude Spectrum)
    ax3 = fig.add_subplot(223)
    ax3.plot(xf, np.abs(yf), color='green')  # Plot both negative and positive frequencies
    ax3.set_title('Frequency Domain (Magnitude Spectrum)')
    ax3.set_xlabel('Frequency (Hz)')
    ax3.set_ylabel('Magnitude')
    ax3.grid(True)

    plt.tight_layout()
    plt.show()

# Usage example
sampling_rate = 1e6  # Replace with the actual sampling rate in Hz


# Read the I/Q data from the file
sos = signal.butter(10, 1000, 'hp', fs=sampling_rate, output='sos')  # Cutoff frequency at 1 kHz
filtered_iq_data = signal.sosfilt(sos, iq_data)

# Plot the time domain (I/Q) and frequency domain
plot_iq_data_and_frequency(filtered_iq_data, sampling_rate)

