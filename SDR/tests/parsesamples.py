import numpy as np
import matplotlib.pyplot as plt

ax2 = None
live_real = []
live_imaj = []
live_indices = []


def fft(x):
    N = len(x)
    if N == 1:
        return x
    twiddle_factors = np.exp(-2j * np.pi * np.arange(N//2) / N)
    x_even = fft(x[::2]) # yay recursion!
    x_odd = fft(x[1::2])
    return np.concatenate([x_even + twiddle_factors * x_odd,
                           x_even - twiddle_factors * x_odd])

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

def plot_iq_data_and_frequency(iq_data, sample_rate):
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
    t = np.arange(N)
    S = np.fft.fft(iq_data)
    S_mag = np.abs(S)
    S_phase = np.angle(S)

    # Third subplot: Frequency Domain (Magnitude Spectrum)
    ax3 = fig.add_subplot(223)
    ax3.plot(t,S_mag,'.-')
    ax3.plot(t,S_phase,'.-')
    # ax3.plot(f, X_mag, color='green')  # Plot both negative and positive frequencies
    ax3.set_title('Frequency Domain (Magnitude Spectrum)')
    ax3.set_xlabel('Frequency [MHz]')
    ax3.set_ylabel('Magnitude [dB]')
    ax3.grid(True)

    plt.tight_layout()
    plt.show()

# Usage example
filename = 'output.a1'  # Replace with the actual filename
sampling_rate = 1000000000  # Replace with the actual sampling rate in Hz
freq = 4218274940


# Read the I/Q data from the file
iq_data = read_iq_data(filename)

# Plot the time domain (I/Q) and frequency domain
plot_iq_data_and_frequency(iq_data, sampling_rate)
