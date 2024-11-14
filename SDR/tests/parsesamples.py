import numpy as np
import matplotlib.pyplot as plt

def read_iq_data(filename):
    # Open the file in binary mode
    with open(filename, 'rb') as f:
        # Read the binary data into a numpy array (16-bit signed integers)
        raw_data = np.fromfile(f, dtype=np.int16)

    # Ensure the data has an even number of elements
    if len(raw_data) % 2 != 0:
        raise ValueError("File contains an odd number of 16-bit integers, which doesn't match I/Q pair format.")

    # Split the data into Q (even indices) and I (odd indices)
    #[start:stop:step]
    q_samples = raw_data[::2]  # Q samples (first in each pair)
    i_samples = raw_data[1::2]  # I samples (second in each pair)

    # Combine I and Q into a complex array: complex_value = I + jQ
    iq_data = i_samples + 1j * q_samples
    print("IQ Data: (" + str(len(iq_data)) + "): " + str(iq_data))
    return iq_data

def plot_iq_data_and_frequency(iq_data, center_freq=4218274940, sample_rate=1000000):

    # Compute the FFT of the complex I/Q data
    Fs=sample_rate
    N = len(iq_data)  # Number of samples
    X = np.fft.fftshift(np.fft.fft(iq_data))
    #X *= np.hamming(N)
    X_mag = 10*np.log10(np.abs(X)**2)
    f = np.arange(Fs/-2.0, Fs/2.0, Fs/N)/1e6 # plt in MHz
    #f += center_freq

    # Third subplot: Frequency Domain (Magnitude Spectrum)
    plt.plot(f, X_mag)
    plt.title('Frequency Domain')
    plt.xlabel('Frequency [MHz]')
    plt.ylabel('Magnitude [dB]')
    plt.grid()

    plt.tight_layout()
    plt.show()

    avg_pwr = np.mean(np.abs(iq_data)**2)

    plt.plot(np.arange(0,len(iq_data),1), np.real(iq_data))
    plt.title('Time Domain')
    plt.xlabel('Index [MHz]')
    plt.ylabel('Real [dB]')
    plt.grid()

    plt.tight_layout()
    plt.show()

# Usage example
filename = 'SDR/tests/out.a1'  # Replace with the actual filename
sampling_rate = 10000000  # Replace with the actual sampling rate in Hz
freq = 418274940


# Read the I/Q data from the file
iq_data = read_iq_data(filename)

# Plot the time domain (I/Q) and frequency domain
plot_iq_data_and_frequency(iq_data, freq, sampling_rate)
