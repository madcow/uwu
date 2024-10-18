## uwu - Useful Wireless Utilities

### What's this?

uwu a-are usefuw wiwewess utiwities fow encoding, sending
a-and weceiving data o-over ampwitude-shift moduwated wadio
fwequencies. cuwwentwy it onwy suppowts manchester (binawy
phase) wine e-encoding〜☆  f-fowwawd ewwow cowwection will
be d-done using hamming codes but i'm still wowking on that.

Sorry...

### ASK Module

| Attribute     |  Value                       |
| ------------- | ---------------------------- |
| UHF Band Name | LPD433, SRD860 (ISM)         |
| Carrier Freq. | 433 MHz, 868 MHz             |
| Bandwidth LPD | 1.7 MHz (433.05 – 434.79)    |
| Modulation    | ASK (Amplitude-shift keying) |
| Data Rate     | 4 KHz, maybe more?           |

#### General Notes

- Generic 433 MHz RX/TX chips with ASK.
- Transmitter is on if data pin is high. Receivers have
  automatic gain control. Will output random noise when
  there is no signal. Requires an error checking protocol.
- Use manchester encoding and training preamble for receiver
  and transmitter to sync up their clock phases.
- Encrypted serial connection over 433 MHz?

### Planned Modules

- Frequency-shift keying (FSK)
- Phase-shift keying (PSK)
- Chirp spread spectrum (CSS)

### Further Links

- [Line code](https://en.wikipedia.org/wiki/Line_code)
- [Clock recovery](https://en.wikipedia.org/wiki/Clock_recovery)
- [Manchester code](https://en.wikipedia.org/wiki/Manchester_code)
- [Error correction code](https://en.wikipedia.org/wiki/Error_correction_code)
- [Noisy-channel coding theorem](https://en.wikipedia.org/wiki/Noisy-channel_coding_theorem)
- [Nyquist–Shannon sampling theorem](https://en.wikipedia.org/wiki/Nyquist%E2%80%93Shannon_sampling_theorem)
- [Short range device frequencies](https://de.wikipedia.org/wiki/Short_Range_Device#Frequenzen)
