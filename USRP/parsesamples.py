import numpy as np
import matplotlib.pyplot as plt

ax2 = None
live_real = []
live_imaj = []
live_indices = []

def read_iq_data(filename):
    # Open the file in binary mode
    iq_data = np.genfromtxt(filename, dtype=None, delimiter=',')
    return iq_data
def plot_iq_data_and_frequency(iq_data, center_freq=418274940, sample_rate=1000000):

    # Compute the FFT of the complex I/Q data
    Fs=sample_rate
    N = len(iq_data)  # Number of samples
    X = np.fft.fftshift(np.fft.fft(iq_data))
    # X *= np.hamming(N)
    X_mag = 10*np.log10(np.abs(X)**2)
    f = np.arange(Fs/-2.0, Fs/2.0, Fs/N)/1e6 # plt in MHz
    # f += center_freq

    # Third subplot: Frequency Domain (Magnitude Spectrum)
    plt.plot(f, X_mag)
    plt.title('Frequency Domain USRP')
    plt.xlabel('Frequency [MHz]')
    plt.ylabel('Magnitude [dB]')
    plt.grid()

    plt.tight_layout()
    plt.show()

    avg_pwr = np.mean(np.abs(iq_data)**2)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 6))
    ax1.plot(np.arange(0, len(iq_data), 1), np.real(iq_data))
    ax1.set_title('Time Domain')
    ax1.set_xlabel('Index')
    ax1.set_ylabel('Real')
    ax1.grid()

    ax2.plot(np.arange(0, len(iq_data), 1), np.imag(iq_data))
    ax2.set_xlabel('Index')
    ax2.set_ylabel('Imaginary')
    ax2.grid()

    plt.tight_layout()
    plt.show()

# Usage example
filename = 'USRP/samples_output.txt'  # Replace with the actual filename
sampling_rate = 1e7  # Replace with the actual sampling rate in Hz
freq = 418274940


# Read the I/Q data from the file
iq_data = read_iq_data(filename)

# Plot the time domain (I/Q) and frequency domain
plot_iq_data_and_frequency(iq_data, freq, sampling_rate)
