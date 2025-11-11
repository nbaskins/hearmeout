#!/usr/bin/env python3

import matplotlib.pyplot as plt
import scipy.io.wavfile as wf
import scipy.signal as sps
import numpy as np

def main():
    rate, data = wf.read('sound.wav')
    print(data.shape)
    print(rate)
    data = [1500 * (int(i + 2**15) / 2**16 - 0.5) + 450 for i in data[:1000000, 0]]
    print(len(data))
    x = [i for i in range(len(data))]
    data_len = len(data)
    new_rate = 8000
    new_data_len = round(data_len * (float(new_rate) / rate))
    new_data = np.array(sps.resample(data, new_data_len), dtype=np.uint16)[:2**16-1]
    wf.write('new_audio.wav', new_rate, new_data.astype(np.int16))


    plt.plot(x, data)
    plt.show()

    with open('data.h', 'w') as f:
        f.write(f'#ifndef DATA_H\n')
        f.write(f'#define DATA_H\n')

        f.write(f'#include <stdint.h>\n')

        f.write(f'\n#define BUFFER_SIZE {len(new_data)}\n\n')


        f.write(f'volatile uint16_t buf[{len(new_data)}] = {{\n')
        for i in range(len(new_data) - 1):
            f.write(f'    {new_data[i]},\n')
        f.write(f'    {new_data[len(new_data) - 1]}\n')
        f.write(f'}};\n\n')


        f.write(f'#endif\n')

if __name__ == "__main__":
    main()
