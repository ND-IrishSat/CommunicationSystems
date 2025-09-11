import numpy as np
import matplotlib.pyplot as plt
from Signals.func import *
import random
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


freq = 418274940
sample_rate = 1e7

filename = './samples_output.txt'  # Replace with the actual filename

processor = SignalProcessing(fs=freq, sps=8, pulse_shape_length=8, pulse_shape='rrc', scheme='BPSK', alpha=0.5, show_graphs=False, data_length=104, fo=61250)

# Read the I/Q data from the file
iq_data = read_iq_data(filename)

# Plot the time domain (I/Q) and frequency domain
# plot_iq_data_and_frequency(iq_data, freq, sample_rate)

# iq_data = np.ravel(iq_data)
out = processor.decode(iq_data)
print("Tests")
for i in range(0,100):
    a = random.uniform(0.000001, 0.132) # 0.0000132
    p = SignalProcessing(fs=freq, sps=8, pulse_shape_length=8, pulse_shape='rrc', scheme='BPSK', alpha=0.5, show_graphs=False, data_length=104, fo=61250, costas_alpha=a, costas_beta=0.0000932)
    o = p.decode(iq_data)
    string = ""
    for i in range(0, 104, 8):
        binary = ''.join(str(bit) for bit in o[i:i+8])
        if (binary == ''):
            binary = '•'
        c = '•'
        try:
            c = chr(int(binary, 2))
        except:
            pass
        string += c
    print(f"Alpha: {a:.6f} ->| {string} |")
print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")

print(f"Receive length = {len(out)}")
print(out)
string = ""
for i in range(0, 104, 8):
    binary = ''.join(str(bit) for bit in out[i:i+8])
    if (binary == ''):
        binary = '•'
    c = '•'
    try:
        c = chr(int(binary, 2))
    except:
        pass
    string += c
        
print(f"Received ->| {string} |\n")