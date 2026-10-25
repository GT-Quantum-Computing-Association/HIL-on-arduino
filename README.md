# LOQC Control Electronics Onboarding: HIL Coincidence Detector

Hardware-in-the-loop (HIL) simulation of an SPDC photon-pair experiment using three Arduino Uno-compatible boards (Elegoo R3 works).

> **Status:** The reference code was traced by hand but not yet run on real boards. Expect a short bring-up pass before the session.

## Contents

1. [What you are building](#1-what-you-are-building)
2. [Arduino IDE setup](#2-arduino-ide-setup)
3. [Wiring](#3-wiring)
4. [Register contract](#4-register-contract)
5. [SPI protocol](#5-spi-protocol)
6. [Decode math](#6-decode-math-dut-side)
7. [Golden packet](#7-golden-packet)
8. [DUT team guide](#8-dut-team-guide)
9. [Plant team guide](#9-plant-team-guide)
10. [Plant serial commands](#10-plant-serial-commands)
11. [Log formats](#11-log-formats)
12. [Checkpoints](#12-checkpoints)
13. [Debugging tips](#13-debugging-tips)
14. [Known simplifications](#14-known-simplifications)
15. [References](#15-references)

---

## 1. What you are building

| Board | Role | Description |
|---|---|---|
| 1x **DUT** | SPI controller | Starts measurements, reads both plants, converts counts to ns, finds coincidences. |
| 2x **Plant A / Plant B** | SPI peripherals | Each acts like a TDC7200 for one detector arm. Simulates photons, detectors and the TDC, then hands the DUT a result packet. |

### Control loop (one measurement)

1. DUT writes a measurement number to both plants (`REG_SEQ`).
2. DUT writes `START_MEAS` to both plants.
3. Each plant simulates one 500 ns window, fills its registers, then pulls its INTB line **low**.
4. DUT sees INTB fall, reads `INT_STATUS`, then reads the 13 result registers over SPI.
5. DUT converts counts to ns and checks for a coincidence between the arms.

### Teams

| Team | Files you write |
|---|---|
| **DUT team** | `spi_controller.cpp`, `decode.cpp`, and TODO 1 and TODO 2 in `student_dut.ino` |
| **Plant team** | `spi_peripheral.cpp`, `sim.cpp` |

Each team tests against the instructor's **reference** version of the other team's half, so nobody waits. When both halves work, we swap in your code on both sides. Any failure then is a real SPI interop bug, and we debug it together.

---

## 2. Arduino IDE setup

- The sketch folder and the main `.ino` must have **the same name**: folder `student_dut` needs `student_dut.ino`, folder `student_plant` needs `student_plant.ino`.
- To add a file: click the small arrow at the top right of the editor, choose **New Tab**, and type the **full name with extension** (for example `spi_controller.cpp`). The IDE saves it in the sketch folder.
- **Do not edit `tdc_contract.h`.** It is the contract both teams share. If you think it is wrong, tell an instructor.
- Select **Tools > Board > Arduino Uno** and the right **Port**.
- Serial Monitor: **115200 baud**, line ending **Newline**.
- The sketch will not link until all your `.cpp` files exist, even as empty stubs.

> **Warning:** Opening the Serial Monitor on an Uno **resets** that board. A reset plant forgets every serial setting (`r`, `d`, `k`, `f`, `g`). Set each plant up, then leave its monitor alone. Reopening the DUT's monitor is fine.

---

## 3. Wiring

All boards 5 V, **all grounds tied together**.

| Signal | DUT | Plant A | Plant B |
|---|---|---|---|
| SCK | D13 | D13 | D13 |
| COPI (was "MOSI") | D11 | D11 | D11 |
| CIPO (was "MISO") | D12 | D12 | D12 |
| CS | D10 → Plant A CS, D9 → Plant B CS | D10 | D10 |
| INTB | D2 ← Plant A D8, D3 ← Plant B D8 | D8 | D8 |

- SCK, COPI and CIPO are **one shared bus** across all three boards.
- Each plant has its own CS line. INTB lines are separate.
- Plants drive INTB open-drain style (low or floating, never driven high). The DUT uses `INPUT_PULLUP` on D2 and D3.
- Tie the grounds together even when each board has its own USB cable.

---

## 4. Register contract

`tdc_contract.h` is the source of truth. Summary:

| Address | Name | Notes |
|---|---|---|
| `0x00` | CONFIG1 | bit0 `START_MEAS` (self-clearing), bit7 `FORCE_CAL` |
| `0x01` | CONFIG2 | default `0x44`: 10 cal periods, 5 stops (bits 7:6 = CAL2 periods, bits 2:0 = NUM_STOP-1) |
| `0x02` | INT_STATUS | read-only. `0x19` = done, `0x1A` = timeout |
| `0x10`..`0x1C` | Results | 13 registers x 24 bits, read-only |
| `0x20`..`0x23` | REG_SEQ | 4 bytes, big endian. **Not** a real TDC7200 register. It lets both plants build the same photon pairs. |

Result registers in address order:

| Address | Register | Array index |
|---|---|---|
| 0x10 | TIME1 | 0 |
| 0x11 | CLOCK_COUNT1 | 1 |
| 0x12 | TIME2 | 2 |
| 0x13 | CLOCK_COUNT2 | 3 |
| 0x14 | TIME3 | 4 |
| 0x15 | CLOCK_COUNT3 | 5 |
| 0x16 | TIME4 | 6 |
| 0x17 | CLOCK_COUNT4 | 7 |
| 0x18 | TIME5 | 8 |
| 0x19 | CLOCK_COUNT5 | 9 |
| 0x1A | TIME6 | 10 |
| 0x1B | CALIBRATION1 | 11 |
| 0x1C | CALIBRATION2 | 12 |

`CLOCK_COUNTn` are always 0 (Mode 1 only).

**INT_STATUS bits:** bit4 complete, bit3 started, bit1 coarse overflow, bit0 new measurement.

- `0x19` = `1 1001` = complete + started + new_meas (all stops arrived)
- `0x1A` = `1 1010` = complete + started + overflow (window ended early)

Each result register is 24 bits, sent MSB first. Bit 23 is parity (always 0 here) and the value is in bits 22:0. The DUT should mask bit 23 off.

---

## 5. SPI protocol

SPI **mode 0** (clock idle low, sample on rising edge), **MSB first**, **1 MHz**.

The first byte after CS falls is the **command byte**:

| Bit | Meaning |
|---|---|
| 7 | auto-increment (1 = address moves to the next register) |
| 6 | 1 = write, 0 = read |
| 5:0 | register address |

`tdc_contract.h` builds it for you: `CMD_READ(a) = 0x80 | a`, `CMD_WRITE(a) = 0xC0 | a`.

The byte clocked back while the command is sent is **don't-care**.

### Example: read TIME1..CAL2 in one transaction

```
COPI (DUT -> plant):  0x90, then 39 bytes of 0x00     (40 bytes total)
CIPO (plant -> DUT):  1 ignored byte, then 39 bytes   (13 registers x 3 bytes)
```

SPI is full duplex: to **read** a byte, the DUT must **send** a byte (a dummy `0x00`).

### Inter-byte pause (important)

The DUT waits `INTERBYTE_US` (20 us) between bytes. An AVR peripheral reloads `SPDR` inside an interrupt and cannot keep up with back-to-back bytes. This is why SPI clock speed barely matters here.

```
begin transaction -> CS low -> wait 20 us
byte, wait 20 us, byte, wait 20 us, ...
wait 20 us -> CS high -> end transaction
```

---

## 6. Decode math (DUT side)

```
calCount = (CAL2 - CAL1) / (cal2periods - 1)
normLSB  = clkPeriodNs / calCount
TOF_ns   = TIME * normLSB
```

- **Check 1 (datasheet example):** `TIME1=4175, CAL1=2110, CAL2=21121`, 10 periods, 8 MHz (125 ns) gives **247.061 ns**. The DUT prints this once at boot.
- **Check 2 (our example, 16 MHz = 62.5 ns):** `TIME1=860, TIME2=3865, CAL1=1136, CAL2=11364` gives **47.30 ns** and **212.56 ns**.

---

## 7. Golden packet

Plant A sees two stops, at 47.3 ns and 212.6 ns. The DUT reads TIME1..CAL2:

| Reply bytes | Register | Value | Bytes (hex) |
|---|---|---|---|
| 1..3 | TIME1 (0x10) | 860 | `00 03 5C` |
| 4..6 | CLOCK_COUNT1 | 0 | `00 00 00` |
| 7..9 | TIME2 (0x12) | 3865 | `00 0F 19` |
| 10..12 | CLOCK_COUNT2 | 0 | `00 00 00` |
| 13..30 | TIME3 .. CLOCK_COUNT5 | 0 | all `00` |
| 31..33 | TIME6 | 0 | `00 00 00` |
| 34..36 | CALIBRATION1 | 1136 | `00 04 70` |
| 37..39 | CALIBRATION2 | 11364 | `00 2C 64` |

`INT_STATUS` reads `0x1A` (2 of 3 stops arrived, window ended).

---

## 8. DUT team guide

**Files you write:** `spi_controller.cpp` (declared in `spi_controller.h`), `decode.cpp` (declared in `decode.h`), and TODO 1 / TODO 2 in `student_dut.ino`.

### Suggested order

1. `decodeNs()` in `decode.cpp`. Check the boot message prints `247.061`.
2. Private helpers in `spi_controller.cpp`: begin a transaction (CS low, pause), end a transaction (pause, CS high), transfer one byte followed by the pause. Everything else uses these.
3. `writeReg`, `writeBurst`, `readReg`.
4. `readResult`: 1 command byte + 39 dummy bytes. Skip the first reply byte. Unpack 13 x 3 bytes into `out[]`, masking off bit 23.
5. **TODO 1 (`readArm`):** read `INT_STATUS` first (this also releases INTB), then `readResult`, then decode TIME1, TIME2, ... A TIME of 0 means "no stop here": stop collecting.
6. **TODO 2 (`countCoincidences`):** count pairs `(a[i], b[j])` with `|(a[i] - b[j]) - PEAK_NS| <= COINC_HALF_NS`.

### Things to watch

- Send the dummy byte when you want to read.
- `readReg` returns the byte clocked back **with the dummy**, not with the command.
- If a plant never pulls INTB low (Test 7), `readArm` must return `-1` and the program must keep running. The skeleton loop already has a 30 ms timeout.

---

## 9. Plant team guide

**Files you write:** `spi_peripheral.cpp` (declared in `spi_peripheral.h`) and `sim.cpp` (declared in `sim.h`). `student_plant.ino` is provided complete. Set `MY_ARM` to `'A'` or `'B'` before flashing each board.

### spi_peripheral.cpp (most of the difficulty)

- You are an SPI **peripheral**. Do **not** call `SPI.begin()` (that makes you a controller). Set up the hardware registers directly: `SPCR = (1<<SPE) | (1<<SPIE);` and write `ISR(SPI_STC_vect)` to run once per byte received.
- **Byte state machine in the ISR:**
  - *State 0:* this byte is the command. Decode write/read, auto-increment and address. Preload `SPDR` with the first reply byte.
  - *State 1:* data bytes. Read: load `SPDR` with the next register byte (result registers are 3 bytes each, MSB first). Write: store the byte. With auto-increment, advance to the next register after each register's last byte.
  - Load `SPDR` for the **next** byte inside the ISR, before it arrives.
- **CS rising resets the state machine** to "expect command". Use a pin-change interrupt on D10: `PCICR |= (1<<PCIE0)`, `PCMSK0 |= (1<<PCINT2)`, and `ISR(PCINT0_vect)`.
- **CIPO sharing:** two plants share D12. Make D12 an **input** (high-Z) while CS is high and an **output** while CS is low. Otherwise the plants fight over the line and the DUT reads garbage. This is the most common source of weird bytes.
- **INTB** is D8 (PORTB bit 0). Pull low by driving the pin low. Release by making it an input. Never drive it high.
- Writing `CFG1_START_MEAS` to CONFIG1: clear that bit, clear `INT_STATUS`, release INTB, set a flag for the main loop. `takeStartRequest()` returns true once per flag.
- Reading `INT_STATUS` releases INTB.
- `INT_STATUS` and the result registers are read-only to the DUT.
- CONFIG2 powers up as `CFG2_DEFAULT`.
- `publish()`: copy `r[]` into the result registers with interrupts **off**, set `INT_STATUS`, then pull INTB low unless `intbFault` is set.
- Variables shared between an ISR and the main loop must be `volatile`.

### sim.cpp (see the recipe in `sim.h`)

1. Pairs ~ Poisson(rate x 500 ns). **Both plants must draw the same number of pairs and the same emission times.** Seed a shared random stream from `seq` (for example `seq * 2654435761 + 12345`).
2. Keep each photon with probability 0.2 x 0.6 = **0.12**, using a **private** random stream so A and B survive independently.
3. Detection time = emission + (arm B only: delay) + Gaussian jitter, sigma = 1000 ps / 2.355 = **425 ps**.
4. Dark counts ~ Poisson(dark rate x 500 ns), uniform in the window, pair ID 0.
5. Sort by time.
6. Drop detections within **45 ns** of the previous detection (dead time).
7. Drop events before **12 ns** or after the window. Drop a stop within **67 ns** of the previous recorded stop. Stop at `numStop` stops.

Helpers to write: xorshift random numbers, Knuth Poisson, Box-Muller Gaussian. An Uno has only 2 KB of RAM, so keep arrays small. A xorshift state of 0 stays 0 forever, so make sure no seed becomes 0.

---

## 10. Plant serial commands

115200 baud, then newline.

| Command | Meaning |
|---|---|
| `r<pairs/s>` | pair rate, e.g. `r1000000` |
| `d<ns>` | optical delay on arm B, e.g. `d20` (only plant B uses it) |
| `k<per s>` | dark count rate, e.g. `k250` |
| `f<0\|1>` | fault: `f1` = never pull INTB low (Test 7) |
| `g<0\|1>` | golden: `g1` = send the packet-exercise example |

Both plants **must** have the same `r`, otherwise they draw different numbers of pairs and coincidences vanish. For demos, use `r1000000` on both plants.

---

## 11. Log formats

**Plant (truth):**

```
T,<seq>,<A|B>,<nStops>,<t_ps>,<pairId>,...
```

`pairId` 0 = dark count. Only printed when there is at least one stop.

**DUT:**

```
M,<seq>,A:<ns/ns...>,B:<ns/ns...>,coinc=<n>     both arms saw a stop
S,<nMeas>,both=...,coinc=...,timeouts=...        summary every 1000 measurements
R,...                                            raw registers (when DEBUG_RAW is true)
```

---

## 12. Checkpoints

| Test | What to do | Expected |
|---|---|---|
| 1. Golden packet | `g1` on the plant, `DEBUG_RAW = true` on the DUT | status `0x1A`, T1=860, T2=3865, CAL1=1136, CAL2=11364, TOF 47.30 / 212.56 ns |
| 2. Datasheet math | boot line | 247.061 |
| 3. Dark counts only | `r0` on both plants | no coincidence peak |
| 4. Delay sweep | `d0`, `d10`, `d20`, `d30` on plant B, set `PEAK_NS = -delay` | peak in tA - tB moves with the delay |
| 5. Normal run | compare DUT coincidence count to plant truth lines | a true coincidence is a `T` line from each plant with the same `seq` and the same nonzero `pairId` |
| 6. Brightness sweep | `r10000`, `r100000`, `r1000000` on both plants | CAR trend matches worksheet 6b |
| 7. Stuck INTB | `f1` on one plant | DUT times out and keeps running, no freeze |

**Speed note:** at `r100000`, only about 0.07% of windows contain a true coincidence, so expect one every several seconds. Use `r1000000` for demos.

---

## 13. Debugging tips

- **Debug in layers.** Get a plant to print `Plant A ready`. Then get ONE plant talking to the DUT with `g1` and `DEBUG_RAW`. Only then add the second plant and the physics.
- **All values look shifted by one byte:** you forgot to skip the reply to the command byte.
- **Values look random or garbage:** check CIPO floats when CS is high, ground is shared, and `INTERBYTE_US` is respected.
- **DUT only ever times out:** check INTB wiring (plant D8 to DUT D2 or D3), and check the plant prints `T` lines (meaning it got `START_MEAS`).
- **Coincidences never appear:** both plants must have the same `r`, the DUT must send the same `seq` to both, and `PEAK_NS` must equal `-delay`.
- **Don't print inside an ISR.** Set a flag and print from `loop()`.
- **Corrupted bytes:** try raising `INTERBYTE_US` to 40 or lowering the SPI speed to 500000 in your own tests.

---

## 14. Known simplifications

- INTB is released when the DUT reads `INT_STATUS` or writes `START_MEAS`. The real TDC7200 clears flags by writing 1s (datasheet 8.6.4).
- The CONFIG2 layout used here (bits 7:6 CAL2 periods, bits 2:0 NUM_STOP-1) should be confirmed against datasheet 8.6.3.
- `REG_SEQ` is not a TDC7200 register.
- Parity bit is always 0. Mode 1 only, so `CLOCK_COUNT` registers are always 0.
- Detection probability per arm is 0.12, jitter sigma 425 ps, detector dead time 45 ns, dark rate 250 /s (SPDMH3 maximum per Thorlabs).
- Cycle time and coincidence rates in this README are estimates, not measurements.

---

## 15. References

1. Thorlabs Type II BBO crystal specs: https://www.thorlabs.com/bbo-crystals-for-spontaneous-parametric-down-conversion-spdc?tabName=Specs
2. Thorlabs SPDMH3 single photon detector datasheet: https://media.thorlabs.com/globalassets/items/s/sp/spd/spdmh3/mtn028160-d02.pdf
3. TI TDC7200 datasheet: https://www.ti.com/lit/ds/symlink/tdc7200.pdf