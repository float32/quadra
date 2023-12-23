import scipy.signal as signal

NUM_SECTIONS = 2

def gen_filter(Tsymbol):
    wc = 1 / Tsymbol
    N = NUM_SECTIONS * 2
    return signal.bessel(N, wc, output='sos', fs=1)

def generate():
    for Tsym in (5, 6, 8, 10, 12, 16):
        yield Tsym, gen_filter(Tsym)

if __name__ == '__main__':
    import matplotlib.pyplot as plt
    import numpy as np

    for Tsym, sos in generate():
        fpass = 1 / Tsym
        fstop = 2 * fpass
        f = np.geomspace(1 / 32, 1 / 2, num=1000)
        w, h = signal.sosfreqz(sos, worN=f, fs=1)
        _, gd = signal.group_delay(signal.sos2tf(sos), w=f, fs=1)

        fix, ax2 = plt.subplots()
        ax1 = ax2.twinx()

        ax1.set_xlabel('Frequency')
        ax1.set_ylabel('Amplitude [dB]', color='tab:blue')
        ax1.semilogx(w, 20 * np.log10(abs(h)), base=2, color='tab:blue')
        ax1.set_ylim(-60, 3)

        ax2.set_ylabel('Group delay', color='tab:orange')
        ax2.semilogx(w, gd, base=2, color='tab:orange')

        plt.title('Frequency response (Tsymbol={})'.format(Tsym))
        plt.margins(0, 0.1)
        ax1.grid(which='both', axis='both')
        plt.axvline(fpass, color='tab:green')
        plt.axvline(fstop, color='tab:red')
        plt.show()
