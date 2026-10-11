EMBEDDED AI POKÉDEX (COMPUTER VISION & HARDWARE INTERFACE)
----------------------------------------------------------------------

An end-to-end physical Pokédex system combining an edge-accelerated computer 
vision, real-time UART serial telemetry, and an Arduino-driven 16x2 LCD 
display. When triggered via physical hardware, the system identifies 
Pokémon in a live camera feed using a custom-trained YOLO model, retrieves 
live Pokédex data via PokéAPI, and streams structured binary packets back 
to the microcontroller for display.

PROJECT OVERVIEW
----------------------------------------------------------------------
The system operates across two interconnected subsystems:

1. Host Server (Python / CUDA):
   - Streams live video via OpenCV and awaits hardware interrupts.
   - Runs target inference using a custom-trained YOLO model on GPU.
   - Fetches Pokédex indices, base stats, and retro flavour text from PokéAPI.
   - Word-wraps and formats text into 32-character pages (two 16-char lines).
   - Packs data into a custom binary framed packet with XOR checksum validation 
     and dispatches it over UART at 115,200 baud.

2. Embedded Firmware (Arduino Uno / ATmega328P):
   - Listens for debounced pushbuttons (Active-LOW with internal pull-ups).
   - Transmits a scan trigger byte ('S') to the host over UART.
   - Implements a non-blocking finite state machine (FSM) to parse incoming 
     framed serial packets (Header, Metadata, Payload, Checksum, Footer).
   - Renders primary stats and multi-page flavour text across a 16x2 LCD.


HARDWARE WIRING & PINOUT
----------------------------------------------------------------------
Component: Arduino Uno / ATmega328P

- Pushbuttons (Active-LOW, INPUT_PULLUP to GND):
  * Pin 2 (BTN_SCAN_PIN): Initiates camera scan/target classification
  * Pin 3 (BTN_PAGE_PIN): Cycles between Stats view and Flavour Text pages

- 16x2 Character LCD (Parallel 4-bit mode):
  * RS  -> Pin 7
  * EN  -> Pin 8
  * D4  -> Pin 9
  * D5  -> Pin 10
  * D6  -> Pin 11
  * D7  -> Pin 12
  * VSS/RW -> GND, VDD -> 5V, V0 -> Contrast Potentiometer


TECH STACK & TOOLS
----------------------------------------------------------------------
- Embedded Hardware: Arduino Uno (ATmega328P), 16x2 LCD (HD44780-compatible)
- Firmware: C++ (Arduino Core, LiquidCrystal library)
- Host Software: Python 3.10+
- Computer Vision: Ultralytics YOLO (CUDA-accelerated), OpenCV
- Networking: Requests (PokéAPI integration)
- Communications: PySerial, Struct (binary pack/unpack)


SETUP & EXECUTION
----------------------------------------------------------------------
1. Hardware Flashing:
   - Wire the circuit following the pinout listed above.
   - Connect the Arduino to your host system via USB.
   - Open `firmware/pokedex.ino` in the Arduino IDE and upload to the board.

2. Python Environment Setup:
   - Install dependencies:
     pip install opencv-python pyserial requests ultralytics

3. Configuration:
   - Verify `SERIAL_PORT` in `main.py` matches your board (e.g., COM3 on Windows 
     or /dev/ttyUSB0 on Linux).
   - Verify `MODEL_PATH` points to your trained YOLO `.pt` weights.

4. Run:
   - Execute the host pipeline:
     python server/main.py
   - Press the physical Scan button (Pin 2) to identify a target via webcam.
   - Press the Page button (Pin 3) on the hardware to cycle flavour text pages.


KEY ENGINEERING IMPLEMENTATIONS
----------------------------------------------------------------------
- Non-Blocking Embedded FSM: Custom byte-by-byte parser prevents CPU stalls 
  and serial buffer overflow on the ATmega328P during multi-byte transfers.
- Payload Optimization: Scaled base stats (BST/4) down to an 8-bit integer and 
  fixed string fields to minimize transfer latency and stay within the 2KB SRAM.
- Text Pagination Engine: Automated line wrapping and space-padding in Python 
  chunks descriptions into exact 32-character screens for zero-computation 
  paging on the microcontroller.
- API Request Caching: In-memory dictionary cache eliminates redundant network 
  requests on repeated scans.
