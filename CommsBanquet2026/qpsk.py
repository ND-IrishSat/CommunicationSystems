import numpy as np
class QPSK:
    def __init__(self, mapping={0:-1-1j,1:-1+1j,2:1-1j,3:1+1j}, normalize=True):
        if normalize:
            max_magnitude = -1
            for key in mapping.keys():
                mag = np.abs(mapping[key])
                if mag > max_magnitude:
                    max_magnitude = mag
            for key in mapping.keys():
                mapping[key] = mapping[key] / max_magnitude
        self.mapping = mapping
    def modulate(self, data: bytes) -> np.complex64:
        # Convert bytes to bit array
        bits = np.unpackbits(np.frombuffer(data, dtype=np.uint8))

        # Pad with 0 if odd length
        if len(bits) % 2 != 0:
            bits = np.append(bits, 0)

        # Iterate in steps of 2 bits
        symbols = []
        for i in range(0, len(bits), 2):
            symbols.append(self.mapping[(bits[i] << 1) | bits[i+1]])

        return np.array(symbols)
    def demodulate(self, samples: np.complex64) -> bytes:
        const_points = np.array(list(self.mapping.values()))
        const_indices = np.array(list(self.mapping.keys()))

        # Compute all distances at once
        dists = np.abs(samples[:, None] - const_points[None, :])

        nearest = np.argmin(dists, axis=1)
        detected_indices = const_indices[nearest]

        bits = np.zeros(len(detected_indices) * 2, dtype=np.uint8)
        bits[0::2] = (detected_indices >> 1) & 1
        bits[1::2] = detected_indices & 1

        return np.packbits(bits).tobytes()
    

if __name__ == "__main__":
    qpsk = QPSK()
    data = b"Hello World!"
    print("Data Bytes : ", data)

    tx_symbols = qpsk.modulate(data) # modulate

    # simulate noise  (just white noise, no phase offsets)
    rx_symbols = tx_symbols + (np.random.randn(len(tx_symbols)) + 1j*np.random.randn(len(tx_symbols))) * 0.1

    rx_data = qpsk.demodulate(rx_symbols) # demodulate
    print("  RX Bytes : ", rx_data)


    num_errors = sum(a != b for a, b in zip(data, rx_data))
    print("       BER : ", round(num_errors / len(data), 2))


    print("     Match : ", data == rx_data[:len(data)])