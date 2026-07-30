# XMEGA Polyphonic Wavetable Synthesizer

**▶ Demo:** [Audio Synthesizer for the ATxmega128A1U](https://youtu.be/gx8KwXzQoUY)

A real-time 3-voice wavetable synthesizer on a bare-metal ATxmega128A1U (8-bit AVR, 32 MHz),
playable from a PC keyboard over serial, with a live ASCII oscilloscope rendered back to the
terminal. Grew out of the EEL 4744 course platform and was extended into a standalone
instrument.

## Specs

| Parameter | Value |
|---|---|
| Sample rate / resolution | 20.0 kHz, 12-bit on-chip DAC |
| Polyphony | 3 voices (architecture parameterized to 8) |
| Waveforms | sine, triangle, saw, square (256-point tables in flash) |
| Note range | 88-note lookup table (full piano range), 8 octave-shift keys |
| Envelope | per-voice ADSR, 7 presets per stage (attack 13–819 ms, release 19–1200 ms) |
| Filter | per-voice 1-pole IIR low-pass, 5 cutoff presets, Q8 fixed point |
| Modulation | 5 Hz LFO vibrato, ±13 cents |
| Serial link | USART at 2 Mbaud, DMA circular RX buffer |
| Scope display | 128×18 characters, zero-crossing triggered, double-buffered, ~19.5 fps |
| Footprint | 8.6 KB flash; SRAM at 78% utilization |

## Architecture

- **Gapless audio.** Timer overflow events drive a DMA channel feeding the DAC through the
  XMEGA event system. The ISR swaps ping-pong buffer addresses while the main loop refills
  the inactive bank, so output never gaps.
- **Fixed-point mixing in inline assembly.** The mixer hot path uses a hand-written
  16×16→32 multiply with a fused `>>10`, decomposed into four 8×8 `mul` partial products.
  Mixing avoids runtime division entirely via a precomputed reciprocal table.
- **Voice stealing.** A note-ownership table prefers idle voices, then steals the releasing
  voice closest to silence.
- **Keyboard input.** Terminals only repeat one held key, so a host-side AutoHotkey script
  round-robins up to four physically held keys at 20 ms intervals to synthesize true
  polyphonic input.
- **Self-instrumented timing.** GPIO pins toggle around each pipeline stage (voice update,
  block fill, scope render) so a bench oscilloscope can profile real-time headroom.

## Source layout

```
synthesizer.c   DSP core: voice allocation, ADSR, mixing, inline-asm multiply
scope.c         ASCII oscilloscope renderer (VT100)
dma.c           DAC ping-pong + USART circular RX DMA channels
common.c/.h     wavetables, tuning/preset tables, config, compile-time guards
clock.c dac.c usart.c timer.c   peripheral drivers
keybuffer.ahk   host-side polyphonic key emulator
```

## Build

Atmel Studio 7 project (`synth.cproj`), avr-gcc `-Os`, EDBG debugger.
Connect a terminal (PuTTY) at 2 Mbaud to play; audio out on the DAC pin.
