#!/Users/rylanpaul/uhd/uhd_venv/bin/python
# Demo Python QPSK
# Date: March 23, 2026
# By Rylan Paul

''' J. CHISUM says,
Rylan, if we want to use the USRP next week with Raytheon, could
you show us how to generate a 400 MHz QPSK signal that is externally
triggered. There is a coax port on the back of the x410 that is
"trig in". We want the qpsk signal to be sent every time the trigger
signal goes high. Activities, no trigger, just constantly looping is
fine too. By the way, this is a backup solution in case the Keysight
AWG doesn't work as we want.
'''

# ============ NOTES ============
# If you are running in a python venv,
# will need to run ./file.py with a
# shebang specifying the python venv.
# If you cannot ./file.py, run
# chmod +x file.py
# to add execution permissions


import numpy as np
import uhd
from qpsk import QPSK

channel = 0              # tx_channel
center_freq = 400e6      # f_c
sample_rate = 10e6       # f_s
tx_gain = 40             # ~ Approximately gain in dB (NOT OUTPUT POWER in dBm)
sps = 4                  # samples per symbol

def trigger() -> bool:
    ans = input("Enter to send QPSK data or Q to quit: ").rstrip().lstrip()
    if len(ans) != 0:
        if ans[0].lower() == 'q':
            return False
    return True


qpsk = QPSK() #QPSK Modulation object
usrp = uhd.usrp.MultiUSRP() # fetch USRP object

while True:
    num_bytes = 32
    data = np.random.bytes(num_bytes) # b'Hello World!'
    symbols = qpsk.modulate(data)
    signal = np.repeat(symbols, sps) * 0.7 # slight scale down to avoid clipping (may not be necessary)
    finish = trigger()
    if not finish:
        break
    duration = len(signal) / sample_rate
    usrp.send_waveform(signal, duration, center_freq, sample_rate, [channel], tx_gain) # signal, time to send, tune f_c, f_s DAC, channel 0, amp gain, if duration is longer than signal it will simply repeat it
print("\nFinished transmitting")

