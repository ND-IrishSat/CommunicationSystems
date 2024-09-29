import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft, fftshift, fftfreq

ax2 = None
live_real = []
live_imaj = []
live_indices = []

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
    print(iq_data)
    return iq_data

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
filename = 'output.a1'  # Replace with the actual filename
sampling_rate = 850000000  # Replace with the actual sampling rate in Hz


# Read the I/Q data from the file
iq_data = read_iq_data(filename)

# Plot the time domain (I/Q) and frequency domain
plot_iq_data_and_frequency(iq_data, sampling_rate)

fig = plt.figure(figsize=(12, 8))
graph = fig.add_subplot(221, projection='3d')

real_parts = np.real(iq_data)
imaginary_parts = np.imag(iq_data)
indices = np.arange(len(iq_data))

live_real = real_parts[0] # real_parts
live_imaj = imaginary_parts[0] # imaginary_parts
live_indices = indices[0] # indices

# Second subplot: Real, Imaginary, and Index (3D Scatter Plot)
graph = plt.scatter(live_indices, live_real, live_imaj, color='red')
plt.pause(1)
indices = np.arange(len(iq_data))  # Indices along the x-axis

#graph.set_title('I and Q vs Index')
# graph.set_xlabel('Index')
# graph.set_ylabel('Real Part (I)')
# graph.set_zlabel('Imaginary Part (Q)')
# graph.grid(True)


index = 0
while(True):
    live_real = real_parts[:index] # real_parts
    live_imaj = imaginary_parts[:index] # imaginary_parts
    live_indices = indices[:index] # indices
    graph.remove()
    graph = plt.scatter(live_indices, live_real, live_imaj, color='red')
    # graph.xlim(live_indices[0], live_indices[index])
    # graph.ylim(live_real[0], live_real[index])
    # graph.zlim(live_imaj[0], live_imaj[index])
    index += 10000
    if (index >= len(indices)):
        index = len(indices) - 1
    plt.pause(0.001)
