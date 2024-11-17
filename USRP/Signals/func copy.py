# each take in iqdata, noise spit out more iqdata, decode should spit out just an array(ints), encode 

from lib import pulse_shaping
from lib import symbol_demod_QAM as demod
import numpy as np
from scipy import signal
import matplotlib.pyplot as plt
from lib import CRC
from lib.IQ_Imbalance_correction import ShowConstellationPlot, PlotWave, IQ_Imbalance_Correct


class SignalProcessing:
    def __init__(self, fs=2.4e9, sps=8, pulse_shape_length=8, pulse_shape='rrc', scheme='BPSK', alpha=0.5, show_graphs=False):
        # Declare Variables
        self.fs = fs                                    # carrier frequency 
        self.Ts = 1.0 / self.fs                         # sampling period in seconds
        self.sps = sps                                  # oversampling factor
        self.pulse_shape_length = pulse_shape_length    # L: pulse shaping filter length (Is this the ideal length?)
        self.pulse_shape = pulse_shape                  # type of pulse shaping, also "rect"
        self.scheme = scheme                            # Modulation scheme 'OOK', 'QPSK', or 'QAM'
        self.alpha = alpha                              # roll-off factor of the RRC pulse-shaping filter
        self.show_graphs = show_graphs
        self.preamble = np.array([0,1,0,0,0,0,1,1,0,0,0,1,0,1,0,0,1,1,1,1,0,1,0,0,0,1,1,1,0,0,1,0,0,1,0,1,1,0,1,1,1,0,1,1,0,0,1,1,0,1,0,1,0,1,1,1,1,1,1,0]).astype(float) # optimal periodic binary code for N = 63 https://ntrs.nasa.gov/citations/19800017860
        self.CRC_key = np.array([1,0,0,1,1,0,0,0,0,1,1,1]) # Best CRC polynomials: https://users.ece.cmu.edu/~koopman/crc/


    def encode(self, data, random_data=False):
        if (random_data):
            data_size = 256
            data = np.random.randint(2, size=data_size).astype(float)
        data_encoded = CRC.encodeData(data, self.CRC_key)
        bits = np.append(self.preamble,data_encoded)

        # pulse train and BPSK modulation
        pulse_train = np.array([(bit * 2 - 1) if i == 0 else 0 for bit in bits for i in range(self.sps)])
        iq_data = pulse_shaping.pulse_shaping(pulse_train, self.sps, fs=self.fs, pulse_shape=self.pulse_shape, alpha=self.alpha, L=self.pulse_shape_length)

        if (self.show_graphs):
            plt.stem(pulse_train, label="Pulse Train")
            plt.stem(bits, 'ro', label="Original Bits")
            plt.legend(loc="upper right")
            plt.title("Pulse Train")
            plt.show()
            plt.stem(pulse_train, 'ko')
            plt.title("After pulse shaping")
            plt.show()

        return iq_data

    def noise(self, iq_data, noise_power=10, phase_noise_strength=0.1):
        # Parameters for AWGN
        num_samples = len(iq_data)
        awgn_complex_samples = (np.random.randn(num_samples) + 1j*np.random.randn(num_samples)) / np.sqrt(2)
        noise_power = 10
        awgn_complex_samples /= np.sqrt(noise_power)
        phase_noise_strength = 0.1
        phase_noise_samples = np.exp(1j * (np.random.randn(num_samples)*phase_noise_strength)) # adds random imaginary phase noise
        iq_data = np.add(iq_data, awgn_complex_samples)
        iq_data_noise = np.multiply(iq_data, phase_noise_samples)
        return iq_data_noise

    def decode(self, iq_data):