LOQC Control Electronics Onboarding
HIL Coincidence Detector (3x Arduino Uno-compatible boards)

=====================================================================
1. WHAT YOU ARE BUILDING
=====================================================================

We are simulating a photon-pair experiment (SPDC) with hardware in the loop.
Two Arduinos pretend to be the detectors and a TDC chip. One Arduino is the
real device we are testing.

  DUT   (1 board)  The detector MCU. It is the SPI controller. It starts
                   measurements, reads both plants, converts counts to
                   nanoseconds, and finds coincidences.
  Plant (2 boards) Plant A and Plant B. Each is an SPI peripheral that acts
                   like a TDC7200 for one detector arm. It simulates photons,
                   detectors, and the TDC, then hands the DUT a result packet.

Control loop, one measurement:
  1. DUT writes a measurement number to both plants (REG_SEQ).
  2. DUT writes START_MEAS to both plants.
  3. Each plant simulates one 500 ns window, fills its registers, then pulls
     its INTB line LOW.
  4. DUT sees INTB fall, reads INT_STATUS, then reads the 13 result registers
     over SPI.
  5. DUT converts counts to ns and checks for a coincidence between arms.

You will be split into two teams:

  DUT team    writes: spi_controller.cpp, decode.cpp, and TODO 1 and TODO 2
                      in student_dut.ino
  Plant team  writes: spi_peripheral.cpp, sim.cpp

Each team tests against the instructor's REFERENCE version of the other team's
half, so nobody waits. When both halves work, we swap in your code on both
sides. Any failure then is a real SPI interop bug, and we debug it together.

=====================================================================
2. ARDUINO IDE SETUP
=====================================================================

  - The sketch folder and the main .ino must have the SAME name.
    Folder student_dut needs student_dut.ino. Folder student_plant needs
    student_plant.ino.
  - To add a file: click the small arrow at the top right of the editor,
    choose New Tab, and type the FULL name with extension, for example
    spi_controller.cpp. The IDE saves it in the sketch folder.
  - Do NOT edit tdc_contract.h. It is the contract both teams share. If you
    think it is wrong, tell an instructor.
  - Select the board: Tools > Board > Arduino Uno (Elegoo R3 works as Uno),
    and pick the right Port.
  - Serial Monitor: 115200 baud, line ending set to "Newline".
  - WARNING: opening the Serial Monitor on an Uno RESETS that board. A reset
    plant forgets every serial setting (r, d, k, f, g). Set each plant up,
    then leave its monitor alone. Reopening the DUT's monitor is fine.
  - The sketch will not link until all your .cpp files exist, even if they
    are empty stubs.

=====================================================================
3. WIRING (all boards 5 V, all grounds tied together)
=====================================================================

  Signal   DUT                       Plant A     Plant B
  -------  ------------------------  ----------  ----------
  SCK      D13                       D13         D13
  COPI     D11  (was "MOSI")         D11         D11
  CIPO     D12  (was "MISO")         D12         D12
  CS       D10 -> Plant A CS         D10         D10
           D9  -> Plant B CS
  INTB     D2 <- Plant A D8          D8          D8
           D3 <- Plant B D8                      D8

  - SCK, COPI, and CIPO are ONE shared bus across all three boards.
  - Each plant has its own CS line. INTB lines are separate.
  - The plants drive INTB "open-drain style": low or floating, never driven
    high. The DUT uses INPUT_PULLUP on D2 and D3.
  - Tie all grounds together, even when each board has its own USB cable.

=====================================================================
4. THE REGISTER CONTRACT (summary, tdc_contract.h is the truth)
=====================================================================

  Address     Name           Notes
  ----------  -------------  ----------------------------------------------
  0x00        CONFIG1        bit0 START_MEAS (self-clearing), bit7 FORCE_CAL
  0x01        CONFIG2        default 0x44: 10 cal periods, 5 stops
                             (bits 7:6 = CAL2 periods, bits 2:0 = NUM_STOP-1)
  0x02        INT_STATUS     read-only. 0x19 = done, 0x1A = timeout
  0x10..0x1C  results        13 registers x 24 bits, read-only
  0x20..0x23  REG_SEQ        4 bytes, big endian. NOT a real TDC7200
                             register. It exists so both plants can build
                             the same photon pairs.

  Result registers, in address order:
    0x10 TIME1   0x11 CLOCK_COUNT1   0x12 TIME2   0x13 CLOCK_COUNT2
    0x14 TIME3   0x15 CLOCK_COUNT3   0x16 TIME4   0x17 CLOCK_COUNT4
    0x18 TIME5   0x19 CLOCK_COUNT5   0x1A TIME6   0x1B CALIBRATION1
    0x1C CALIBRATION2

  In your code (array index 0..12): TIME1=0, TIME2=2, TIME3=4, TIME4=6,
  TIME5=8, CAL1=11, CAL2=12. CLOCK_COUNTn are always 0 (Mode 1 only).

  INT_STATUS bits: bit4 complete, bit3 started, bit1 coarse overflow,
  bit0 new measurement.
    0x19 = 1 1001 = complete + started + new_meas  (all stops arrived)
    0x1A = 1 1010 = complete + started + overflow  (window ended early)

  Each result register is 24 bits sent MSB first. Bit 23 is parity (always 0
  here), and the value is in bits 22:0. The DUT should mask bit 23 off.

=====================================================================
5. THE SPI PROTOCOL
=====================================================================

  SPI mode 0 (clock idle low, sample on rising edge), MSB first, 1 MHz.

  First byte after CS falls is the COMMAND byte:

      bit 7   auto-increment (1 = address moves to the next register)
      bit 6   1 = write, 0 = read
      bits 5:0  register address

  The tdc_contract.h macros build it for you:
      CMD_READ(a)  = 0x80 | a     (read, auto-increment)
      CMD_WRITE(a) = 0xC0 | a     (write, auto-increment)

  The byte clocked back while the command is sent is DON'T-CARE.

  Example: read TIME1..CAL2 in one transaction.
      COPI (DUT -> plant):  0x90, then 39 bytes of 0x00   (40 bytes total)
      CIPO (plant -> DUT):  1 ignored byte, then 39 bytes
                            (13 registers x 3 bytes)

  SPI is full duplex: to READ a byte from the plant, the DUT must SEND a
  byte (a dummy 0x00). Reads and writes both clock the bus.

  Inter-byte pause (IMPORTANT):
    The DUT waits INTERBYTE_US (20 us) between bytes. An AVR peripheral
    reloads SPDR inside an interrupt, and it cannot keep up with back-to-back
    bytes. This is why SPI clock speed barely matters here. Per transaction:
        begin transaction -> CS low -> wait 20 us
        byte, wait 20 us, byte, wait 20 us, ...
        wait 20 us -> CS high -> end transaction

=====================================================================
6. THE DECODE MATH (DUT side)
=====================================================================

  The TDC has a fine timer with a nominal LSB of 55 ps. It calibrates itself
  by measuring 1 and 10 clock periods.

      calCount = (CAL2 - CAL1) / (cal2periods - 1)
      normLSB  = clkPeriodNs / calCount
      TOF_ns   = TIME * normLSB

  Check 1 (datasheet example):
      TIME1=4175, CAL1=2110, CAL2=21121, 10 periods, 8 MHz (125 ns)
      -> 247.061 ns  (the DUT prints this once at boot)

  Check 2 (our example, 16 MHz = 62.5 ns):
      TIME1=860, TIME2=3865, CAL1=1136, CAL2=11364
      -> 47.30 ns and 212.56 ns

=====================================================================
7. EXAMPLE PACKET (the golden packet)
=====================================================================

  Plant A sees two stops: 47.3 ns and 212.6 ns. The DUT reads TIME1..CAL2:

    Reply bytes  Register        Value    Bytes (hex)
    1..3         TIME1 (0x10)    860      00 03 5C
    4..6         CLOCK_COUNT1    0        00 00 00
    7..9         TIME2 (0x12)    3865     00 0F 19
    10..12       CLOCK_COUNT2    0        00 00 00
    13..30       TIME3..CLOCK5   0        all 00
    31..33       TIME6           0        00 00 00
    34..36       CALIBRATION1    1136     00 04 70
    37..39       CALIBRATION2    11364    00 2C 64

  INT_STATUS reads 0x1A (2 of 3 stops arrived, window ended).

=====================================================================
8. DUT TEAM GUIDE
=====================================================================

  Files you write:
    spi_controller.cpp   (functions are declared in spi_controller.h)
    decode.cpp           (function is declared in decode.h)
    student_dut.ino      (fill in TODO 1 and TODO 2)

  Suggested order:
    a. decodeNs() in decode.cpp. Check the boot message prints 247.061.
    b. Private helpers in spi_controller.cpp: begin a transaction (CS low,
       pause), end a transaction (pause, CS high), and transfer one byte
       followed by the pause. Everything else uses these.
    c. writeReg, writeBurst, readReg.
    d. readResult: 1 command byte + 39 dummy bytes. Skip the first reply
       byte. Unpack 13 x 3 bytes into out[], masking off bit 23.
    e. TODO 1 (readArm): read INT_STATUS first, because that also releases
       INTB, then readResult, then decode TIME1, TIME2, ... A TIME of 0 means
       "no stop here". Stop collecting.
    f. TODO 2 (countCoincidences): count pairs (a[i], b[j]) with
       |(a[i] - b[j]) - PEAK_NS| <= COINC_HALF_NS.

  Things to watch:
    - Don't forget to send the dummy byte when you want to read.
    - readReg returns the byte clocked back WITH the dummy, not with the
      command.
    - If a plant never pulls INTB low (Test 7), readArm must return -1 and
      the program must keep running. The skeleton loop already has a 30 ms
      timeout.

=====================================================================
9. PLANT TEAM GUIDE
=====================================================================

  Files you write:
    spi_peripheral.cpp   (declared in spi_peripheral.h)
    sim.cpp              (declared in sim.h)
  The main sketch student_plant.ino is provided complete. Set MY_ARM to 'A'
  or 'B' before flashing each board.

  spi_peripheral.cpp (most of the difficulty is here):
    - You are an SPI PERIPHERAL. Do NOT call SPI.begin(), because that makes
      you a controller. Set up the hardware registers directly:
        SPCR = (1<<SPE) | (1<<SPIE);   // enable SPI, enable its interrupt
      and write ISR(SPI_STC_vect) to run once per byte received.
    - Byte state machine in the ISR:
        state 0: this byte is the command. Decode write/read, auto-increment,
                 address. Preload SPDR with the first reply byte.
        state 1: data bytes. Read -> load SPDR with the next register byte
                 (result registers are 3 bytes each, MSB first). Write ->
                 store the byte. With auto-increment, advance to the next
                 register after each register's last byte.
      Load SPDR for the NEXT byte inside the ISR, before the next byte
      arrives.
    - CS rising must reset the state machine to "expect command". Use a
      pin-change interrupt on D10: PCICR |= (1<<PCIE0), PCMSK0 |= (1<<PCINT2),
      and ISR(PCINT0_vect).
    - CIPO sharing: two plants share D12. Make D12 an INPUT (high-Z) while
      CS is high, and an OUTPUT while CS is low. Otherwise the plants will
      fight over the line and the DUT reads garbage. This is the most common
      source of weird bytes.
    - INTB is D8 (PORTB bit 0). Pull LOW by driving the pin low. Release by
      making it an input. Never drive it high.
    - Writing CFG1_START_MEAS to CONFIG1: clear that bit, clear INT_STATUS,
      release INTB, set a flag for the main loop. takeStartRequest() returns
      true once per flag.
    - Reading INT_STATUS releases INTB.
    - INT_STATUS and the result registers are read-only to the DUT.
    - CONFIG2 powers up as CFG2_DEFAULT.
    - publish(): copy r[] into the result registers with interrupts OFF, set
      INT_STATUS, then pull INTB low unless intbFault is set.
    - Variables shared between the ISR and the main loop must be volatile.

  sim.cpp (read the recipe in sim.h):
    1. Pairs ~ Poisson(rate x 500 ns). BOTH plants must draw the same number
       of pairs and the same emission times. Seed a shared random stream
       from seq (for example seq * 2654435761 + 12345).
    2. Keep each photon with probability 0.2 x 0.6 = 0.12, using a PRIVATE
       random stream so A and B survive independently.
    3. Detection time = emission + (arm B only: delay) + Gaussian jitter
       with sigma = 1000 ps / 2.355 = 425 ps.
    4. Dark counts ~ Poisson(dark rate x 500 ns), uniform, pair ID 0.
    5. Sort by time.
    6. Drop detections within 45 ns of the previous detection (dead time).
    7. Drop events before 12 ns or after the window. Drop a stop within 67 ns
       of the previous recorded stop. Stop at numStop stops.
    Helpers to write: xorshift random numbers, Knuth Poisson, Box-Muller
    Gaussian. An Uno has only 2 KB of RAM, so keep arrays small.
    A random generator state of 0 gets stuck at 0 forever in xorshift. Make
    sure no seed becomes 0.

=====================================================================
10. SERIAL COMMANDS FOR THE PLANTS (115200 baud, then newline)
=====================================================================

  r<pairs/s>   pair rate, e.g. r1000000
  d<ns>        optical delay on arm B, e.g. d20   (only plant B uses it)
  k<per s>     dark count rate, e.g. k250
  f<0|1>       fault: f1 = never pull INTB low (Test 7)
  g<0|1>       golden: g1 = send the packet-exercise example

  Both plants MUST have the same r, otherwise they draw different numbers of
  pairs and coincidences vanish.
  For demos, use r1000000 on both plants.

=====================================================================
11. LOG FORMATS
=====================================================================

  Plant (truth):
    T,<seq>,<A|B>,<nStops>,<t_ps>,<pairId>,...
    pairId 0 = dark count. Only printed when there is at least one stop.

  DUT:
    M,<seq>,A:<ns/ns...>,B:<ns/ns...>,coinc=<n>
        printed for measurements where both arms saw a stop
    S,<nMeas>,both=...,coinc=...,timeouts=...
        summary every 1000 measurements
    R,... (when DEBUG_RAW is true) raw registers and decoded times

=====================================================================
12. CHECKPOINTS (these match the worksheet tests)
=====================================================================

  Test 1 Golden packet: send g1 to the plant, set DEBUG_RAW true on the DUT.
         Expect status 0x1A, T1=860, T2=3865, CAL1=1136, CAL2=11364 and
         TOF 47.30 / 212.56 ns.
  Test 2 Datasheet math: the boot line prints 247.061.
  Test 3 Dark counts only: r0 on both plants. No coincidence peak.
  Test 4 Delay sweep: d0, d10, d20, d30 on plant B. The peak in tA - tB moves
         with it. Update PEAK_NS in the DUT to -delay.
  Test 5 Normal run: compare the DUT's coincidence count with the plants'
         truth lines. A true coincidence is a T line from each plant with the
         same nonzero pairId and the same seq.
  Test 6 Brightness sweep: r10000, r100000, r1000000 on both plants.
  Test 7 Stuck INTB: f1 on one plant. The DUT must time out and keep running,
         not freeze.

  Speed note: at r100000, only about 0.07% of windows contain a true
  coincidence, so expect one every several seconds. Use r1000000 for demos.

=====================================================================
13. DEBUGGING TIPS
=====================================================================

  - Debug in layers. Get the plant to print "Plant A ready". Then get ONE
    plant talking to the DUT with g1 and DEBUG_RAW. Only then add the second
    plant and the physics.
  - First byte of every read is junk: if all your values look shifted by one
    byte, you forgot to skip the reply to the command byte.
  - Values look random or garbage: check CIPO is floating when CS is high,
    ground is shared, and INTERBYTE_US is respected.
  - DUT only ever times out: check the INTB wiring (plant D8 to DUT D2 or
    D3), and check the plant prints T lines (meaning it got START_MEAS).
  - Coincidences never appear: both plants must have the same r, and the DUT
    must send the SAME seq to both. Also check PEAK_NS equals -delay.
  - Prints inside an ISR are a bad idea. Set a flag and print from loop().
  - If bytes are corrupted, try raising INTERBYTE_US to 40 or lowering the
    SPI speed to 500000 (in your own tests, not the contract).

=====================================================================
14. KNOWN SIMPLIFICATIONS
=====================================================================

  - INTB is released when the DUT reads INT_STATUS or writes START_MEAS. The
    real TDC7200 clears flags by writing 1s to them (datasheet 8.6.4).
  - CONFIG2 layout (bits 7:6 CALIBRATION2_PERIODS, bits 2:0 NUM_STOP-1) is
    what we use here. Confirm against datasheet 8.6.3.
  - REG_SEQ is not a TDC7200 register.
  - Parity bit is always 0. Mode 1 only, so CLOCK_COUNT registers are 0.
  - Detection probability per arm is 0.12, jitter sigma 425 ps, detector dead
    time 45 ns, dark rate 250 /s (SPDMH3 maximum per Thorlabs).
  - Cycle time and coincidence rates in this README are estimates, not
    measurements.

=====================================================================
15. REFERENCES
=====================================================================

  [1] Thorlabs Type II BBO crystal specs
  [2] Thorlabs SPDMH3 single photon detector datasheet
  [3] TI TDC7200 datasheet (section numbers above refer to this one)