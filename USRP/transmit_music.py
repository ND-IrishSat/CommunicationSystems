import uhd
import numpy as np
usrp = uhd.usrp.MultiUSRP()


duration = 5.0# seconds
center_freq = 418274940 # Hz
sample_rate = 1e6
gain = 90.0 # [dB] start low then work your way up

c4 = 261.63   # C4
c_sharp4 = 277.18  # C#4
d4 = 293.66   # D4
d_sharp4 = 311.13  # D#4
e4 = 329.63   # E4
f4 = 349.23   # F4
f_sharp4 = 369.99  # F#4
g4 = 392.00   # G4
g_sharp4 = 415.30  # G#4
a4 = 440.00   # A4
a_sharp4 = 466.16  # A#4
b4 = 493.88   # B4
c5 = 523.25   # C5 (octave up)

t = duration / 8.0
time = np.arange(0, t, (float(1) / float(sample_rate)))

# Create each note with fade in and fade out
def create_note(freq, note_duration, sample_rate):
    t = np.arange(0, note_duration, 1 / sample_rate)
    fade_in = np.linspace(0, 1, int(len(t) / 10))  # Fade in for first 10%
    fade_out = np.linspace(1, 0, int(len(t) / 10))  # Fade out for last 10%

    # Create the sine wave
    signal = 0.1 * np.sin(2 * np.pi * freq * t)

    # Apply fade in and fade out
    if len(signal) > len(fade_in):
        signal[:len(fade_in)] *= fade_in  # Apply fade in
        signal[-len(fade_out):] *= fade_out  # Apply fade out

    return signal

n1 = create_note(c_sharp4, t, sample_rate)
n2 = create_note(e4, t, sample_rate)
n3 = create_note(f_sharp4, t, sample_rate)
n4 = create_note(g_sharp4, t, sample_rate)
n5 = create_note(c_sharp4, t, sample_rate)
n6 = create_note(e4, t, sample_rate)
n7 = create_note(f_sharp4, t, sample_rate)
n8 = create_note(g_sharp4, t, sample_rate)
samples = np.concatenate((n1,n2,n3,n4,n5,n6,n7,n8))

usrp.send_waveform(samples, duration, center_freq, sample_rate, [0], gain)


