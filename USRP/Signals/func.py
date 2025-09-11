# each take in iqdata, noise spit out more iqdata, decode should spit out just an array(ints), encode 

from .lib import pulse_shaping
from .lib import symbol_demod_QAM as demod
import numpy as np
from scipy import signal
import matplotlib.pyplot as plt
from .lib import CRC
from .lib.IQ_Imbalance_correction import ShowConstellationPlot, PlotWave, IQ_Imbalance_Correct


class SignalProcessing:
    def __init__(self, fs=2.4e9, sps=8, pulse_shape_length=8, pulse_shape='rrc', scheme='BPSK', alpha=0.5, show_graphs=False, data_length=256, fo=61250, costas_alpha=0.0000132, costas_beta=0.0000932):
        # Declare Variables
        self.fs = fs                                    # carrier frequency 
        self.Ts = 1.0 / fs                              # sampling period in seconds
        self.sps = sps                                  # oversampling factor
        self.pulse_shape_length = pulse_shape_length    # L: pulse shaping filter length (Is this the ideal length?)
        self.pulse_shape = pulse_shape                  # type of pulse shaping, also "rect"
        self.scheme = scheme                            # Modulation scheme 'OOK', 'QPSK', or 'QAM'
        self.alpha = alpha                              # roll-off factor of the RRC pulse-shaping filter
        self.show_graphs = show_graphs
        self.preamble = np.array([0,1,0,0,0,0,1,1,0,0,0,1,0,1,0,0,1,1,1,1,0,1,0,0,0,1,1,1,0,0,1,0,0,1,0,1,1,0,1,1,1,0,1,1,0,0,1,1,0,1,0,1,0,1,1,1,1,1,1,0]).astype(float) # optimal periodic binary code for N = 63 https://ntrs.nasa.gov/citations/19800017860
        self.CRC_key = np.array([1,0,0,1,1,0,0,0,0,1,1,1]) # Best CRC polynomials: https://users.ece.cmu.edu/~koopman/crc/
        self.matched_filter_coef = np.flip(self.preamble)
        
        
        self.fo = fo # Simulated frequency offset
        self.data_length = data_length

        self.costas_alpha = costas_alpha # how fast phase updates
        self.costas_beta = costas_beta # how fast freq updates

    def encode(self, data):
        self.data_length = len(data)
            
        print("Data length: " + str(len(data)))
        data_encoded = CRC.encodeData(data, self.CRC_key)

        bits = np.append(self.preamble,data_encoded)

        # pulse train and BPSK modulation
        pulse_train = np.array([])
        for bit in bits:
            pulse = np.zeros(self.sps)
            pulse[0] = bit*2-1 # set the first value to either a 1 or -1
            pulse_train = np.concatenate((pulse_train, pulse)) # add the 8 samples to the signal
        if (self.show_graphs):
            try:
                plt.stem(pulse_train, label="Pulse Train")
                plt.stem(bits, 'ro', label="Original Bits")
                plt.legend(loc="upper right")
                plt.title("Pulse Train")
                plt.show()
            except:
                pass
        iq_data = pulse_shaping.pulse_shaping(pulse_train, self.sps, fs=self.fs, pulse_shape=self.pulse_shape, alpha=self.alpha, L=self.pulse_shape_length)
        if (self.show_graphs):
            try:
                plt.stem(iq_data, 'ko')
                plt.title("After pulse shaping")
                plt.show()
            except:
                pass

        return iq_data

    def noise(self, iq_data, noise_power=10, phase_noise_strength=0.1):
        # Parameters for AWGN
        num_samples = len(iq_data)
        awgn_complex_samples = (np.random.randn(num_samples) + 1j*np.random.randn(num_samples)) / np.sqrt(2)
        awgn_complex_samples /= np.sqrt(noise_power)
        phase_noise_samples = np.exp(1j * (np.random.randn(num_samples)*phase_noise_strength)) # adds random imaginary phase noise
        iq_data = np.add(iq_data, awgn_complex_samples)
        iq_data_noise = np.multiply(iq_data, phase_noise_samples)
        return iq_data_noise

    def decode(self, testpacket):
        # Add fractional delay
        delay = 0.4 # fractional delay, in samples
        N = 21 # number of taps
        n = np.arange(-N//2, N//2) # ...-3,-2,-1,0,1,2,3...
        h = np.sinc(n - delay) # calc filter taps
        h *= np.hamming(N) # window the filter to make sure it decays to 0 on both sides
        h /= np.sum(h) # normalize to get unity gain, we don't want to change the amplitude/power
        testpacket = np.convolve(testpacket, h) # apply filter
        ###################################
        # Add frequency offset - NOTE: Frequency offset of > 1% will result in inability of crosscorrelation operation to detect frame start

        # apply a freq offset
        t = np.arange(0, self.Ts*(len(testpacket)), self.Ts) # create time vector
        testpacket = testpacket * np.exp(1j * 2 * np.pi * self.fo * t) # perform freq shift
        if (self.show_graphs):
            try:
                # plt.stem(symbols_I, label="Original Pulse Shaped Waveform")
                plt.stem(np.real(testpacket), 'ro', label="Non-ideal Waveform Real")
                plt.stem(np.imag(testpacket), 'mo', label="Non-ideal Waveform Imaginary")
                plt.title("After fractional delay and frequency offset")
                plt.legend(loc="upper left")
                plt.show()
            except:
                pass
        """
        Muller and Mueller Clock Recovery
        """
        samples_interpolated = signal.resample_poly(testpacket, 16, 1)

        mu = 0 # initial estimate of phase of sample
        out = np.zeros(len(testpacket) + 10, dtype=complex)
        out_rail = np.zeros(len(testpacket) + 10, dtype=complex) # stores values, each iteration we need the previous 2 values plus current value
        i_in = 0 # input samples index
        i_out = 2 # output index (let first two outputs be 0)

        while i_out < len(testpacket) and i_in+16 < len(testpacket):
            out[i_out] = samples_interpolated[i_in*16 + int(mu*16)]
            out_rail[i_out] = int(np.real(out[i_out]) > 0) + 1j*int(np.imag(out[i_out]) > 0)
            x = (out_rail[i_out] - out_rail[i_out-2]) * np.conj(out[i_out-1])
            y = (out[i_out] - out[i_out-2]) * np.conj(out_rail[i_out-1])
            mm_val = np.real(y - x)
            mu += self.sps + 0.3*mm_val
            i_in += int(np.floor(mu)) # round down to nearest int since we are using it as an index
            mu = mu - np.floor(mu) # remove the integer part of mu
            i_out += 1 # increment output index
        out = out[2:i_out] # remove the first two, and anything after i_out (that was never filled out)

        testpacket = out # only include this line if you want to connect this code snippet with the Costas Loop later on
        if (self.show_graphs):
            plt.stem(np.arange(len(np.real(testpacket))), np.real(testpacket), 'ro', label="Real Part of Clock Recovered Waveform")
            plt.stem(np.arange(len(np.imag(testpacket))), np.imag(testpacket), 'mo', label="Imaginary Part of Clock Recovered Waveform")
            plt.title("After clock recovery")
            plt.legend(loc="upper left")
            plt.show()

        """
        Coarse Frequency Detection and Correction
        """
        fft_samples = testpacket**2

        psd = np.fft.fftshift(np.abs(np.fft.fft(fft_samples)))
        f = np.linspace(-self.fs/2.0, self.fs/2.0, len(psd))

        max_freq = f[np.argmax(psd)]
        t = np.arange(0, self.Ts*len(testpacket), self.Ts) # create time vector
        testpacket = testpacket * np.exp(-1j * 2 * np.pi * max_freq * t / 2.0) # multiply by negative complex sinusoid at offset frequency

        if (self.show_graphs):
            try:
                plt.stem(np.real(testpacket), label="Real Part of Recovered Waveform")
                plt.stem(np.imag(testpacket), 'ro', label="Imaginary Part of Recovered Waveform")
                plt.legend(loc="upper left")
                plt.title("After coarse frequency correction")
                plt.show()
            except:
                pass

        """
        Costas Loop Fine Freq Correction
        """

        N = len(testpacket)
        phase = 0
        freq = 0
        # These next two params are what to adjust, to make the feedback loop faster or slower (which impacts stability)
        out = np.zeros(N, dtype=complex)
        freq_log = []
        for i in range(N):
            out[i] = testpacket[i] * np.exp(-1j*phase) # adjust the input sample by the inverse of the estimated phase offset
            error = np.real(out[i]) * np.imag(out[i]) # This is the error formula for 2nd order Costas Loop (e.g. for BPSK)

            # Advance the loop (recalc phase and freq offset)
            freq += (self.costas_beta * error)
            freq_log.append(freq * self.fs / (2*np.pi)) # convert from angular velocity to Hz for logging
            phase += freq + (self.costas_alpha * error)

            # Optional: Adjust phase so its always between 0 and 2pi, recall that phase wraps around every 2pi
            while phase >= 2*np.pi:
                phase -= 2*np.pi
            while phase < 0:
                phase += 2*np.pi

        # Plot freq over time to see how long it takes to hit the right offset
        if (self.show_graphs):
            try:
                plt.plot(freq_log,'.-')
                plt.title("Estimated frequency offset of Costas Loop vs Sample index")
                plt.show()
            except:
                pass

        testpacket = out

        if (self.show_graphs):
            try:
                plt.stem(signal.upfirdn([1],pulse_train,1, self.sps), 'ko', label="Original Pulse Train")
                plt.stem(np.real(testpacket), label="Real Part of Recovered Waveform")
                plt.stem(np.imag(testpacket), 'ro', label="Imaginary Part of Recovered Waveform")
                plt.legend(loc="upper left")
                plt.title("After fine frequency correction")
                plt.show()
            except:
                pass

        # Testing frequency correction
        fft_samples = testpacket**2
        psd = np.fft.fftshift(np.abs(np.fft.fft(fft_samples)))
        f = np.linspace(-self.fs / 2.0, self.fs / 2.0, len(psd))

        if (self.show_graphs):
            try:
                plt.plot(f, psd)
                plt.title("Frequency offset after correction")
                plt.show()
            except:
                pass

        """
        IQ Imbalance Correction ------
        """
        if (self.show_graphs):
            try:
                ShowConstellationPlot(testpacket, same_magnitude=False, title="Before IQ Imbalance Correction")
            except:
                pass
        testpacket = IQ_Imbalance_Correct(testpacket, mean_period=(len(testpacket) // 10)) # mean_period adjusts how many values it should take the mean of in each direction of an array for a given value.
        if (self.show_graphs):
            try:
                ShowConstellationPlot(testpacket, same_magnitude=False, title="After IQ Imbalance Correction")
            except:
                pass
        #------------------------------------------------

        """
        Frame Sync
        """
        scale = np.mean(np.abs(testpacket))
        out = np.array([])

        for symbol in testpacket:
            symbol = (symbol + scale)/2*scale
            out = np.append(out, symbol)

        crosscorr = signal.fftconvolve(out, self.matched_filter_coef)
        if (self.show_graphs):
            try:
                plt.stem(np.real(testpacket), label="Recovered Waveform")
                plt.stem(self.preamble, 'r', label="Preamble Sequence")
                plt.stem(np.real(crosscorr), 'g', label="Crosscorrelation")
                plt.title("Frame Synchronization: Crosscorrelation")
                plt.legend(loc="upper right")
                plt.show()
            except:
                pass

        idx = np.array(crosscorr).argmax() 
        data_encoded_length = self.data_length + len(self.CRC_key) - 1 # len(data_encoded)
        recoveredPayload = testpacket[idx - len(self.preamble) + 1 : idx + data_encoded_length + 1] # Reconstruct original packet minus preamble
        recoveredData = recoveredPayload[len(self.preamble):]

        if (self.show_graphs):
            try:
                plt.stem(np.real(recoveredPayload), label="Recovered Payload Real")
                plt.stem(np.imag(recoveredPayload), 'ro', label="Recovered Payload Imaginary")
                plt.stem(signal.upfirdn([1],pulse_train,1, self.sps), 'ko', label="Original Pulse Train")
                plt.title("Data recovery")
                plt.legend(loc="upper right")
                plt.show()
            except:
                pass



        demod_bits = demod.symbol_demod(recoveredData, self.scheme, 1, len(self.preamble)) # gain has to be set to 1
        error = CRC.CRCcheck(demod_bits, self.CRC_key)
        if (self.show_graphs):
            print("CRC error: " + str(error) + ("" if error == 0 else " -> CRC check failed and data integrity is compromised!\n") )

        return demod_bits
        # # This just calculates how many bits were correctly received
        # num_correct = 0
        # num = 0
        # for index in range(0,len(data)):
        #     num += 1
        #     value = "NaN"
        #     if (index <= len(demod_bits)-1):
        #         value = demod_bits[index]
        #         if (value == data[index]):
        #             num_correct += 1
        # print(f"Received: {num_correct} / {num} bits   |   {round(num_correct / num * 100)}%")

        # # Plot
        # plt.stem(np.real(demod_bits), label="Recovered Data")
        # plt.stem(0.5*np.real(data), 'ro', label="Original Data")
        # plt.title("Demodulated bit comparison")
        # plt.legend(loc="upper right")
        # plt.show()
