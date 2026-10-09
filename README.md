EMBEDDED AI POKÉDEX (COMPUTER VISION & HARDWARE INTERFACE)

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


SYSTEM ARCHITECTURE
----------------------------------------------------------------------
[ Physical Buttons ] ──> [ Arduino Uno (ATmega328P) ]
 (Scan / Page)             │                      ▲
                           │ UART (115200 baud)   │ Custom Binary Packets
                           ▼                      │ (Header, Meta, Text, CRC)
                         [ Python Host Pipeline ] ┘
                           │                 │
                           ▼                 ▼
                  [ YOLO Vision (CUDA) ]   [ PokéAPI REST ]
                  [ Live Camera Feed   ]   [ In-Memory Cache ]


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


SERIAL PROTOCOL SPECIFICATION
----------------------------------------------------------------------
UART Configuration: 115,200 Baud, 8-N-1

1. Hardware -> Host:
   - ASCII character 'S' sent when Pin 2 is triggered.

2. Host -> Hardware Binary Frame:
   [0xAA] [DEX] [BST/4] [NAME (10B)] [LEN] [PAYLOAD (N bytes)] [CRC] [0x55]

   Field       Type        Description
   -------------------------------------------------------------------
   0xAA        uint8       Frame start delimiter
   DEX         uint8       National Pokédex Number (1–255)
   BST/4       uint8       Base Stat Total scaled by 1/4 (fits in 8 bits)
   NAME        char[10]    Left-aligned, space-padded ASCII name
   LEN         uint8       Text payload byte count (max 160 bytes)
   PAYLOAD     char[LEN]   Sanitized, line-padded flavour text (32B/page)
   CRC         uint8       XOR Checksum: (DEX ^ (BST/4) ^ LEN) & 0xFF
   0x55        uint8       Frame end delimiter


TECH STACK & TOOLS
----------------------------------------------------------------------
- Embedded Hardware: Arduino Uno (ATmega328P), 16x2 LCD (HD44780-compatible)
- Firmware: C++ (Arduino Core, LiquidCrystal library)
- Host Software: Python 3.10+
- Computer Vision: Ultralytics YOLO (CUDA-accelerated), OpenCV
- Networking: Requests (PokéAPI integration)
- Communications: PySerial, Struct (binary pack/unpack)


PROJECT STRUCTURE
----------------------------------------------------------------------
├── firmware/
│   └── pokedex.ino        # Microcontroller FSM, button debounce, LCD driver
├── server/
│   ├── main.py            # OpenCV loop, YOLO inference, API cache, serial bridge
│   └── requirements.txt   # Python dependencies
├── models/
│   └── best.pt            # Custom-trained YOLO detection weights
└── README.txt             # Project documentation


HARDWARE PINOUT AND WIRING REFERENCE
----------------------------------------------------------------------
1. ARDUINO UNO PIN ASSIGNMENTS
----------------------------------------------------------------------
Arduino Pin   Direction   Connected Component       Function / Notes
----------------------------------------------------------------------
Pin 2 (D2)    INPUT       Scan Button (Tactile)     Active-LOW (INPUT_PULLUP)
Pin 3 (D3)    INPUT       Page Button (Tactile)     Active-LOW (INPUT_PULLUP)
Pin 7 (D7)    OUTPUT      16x2 LCD - Pin 4 (RS)     Register Select
Pin 8 (D8)    OUTPUT      16x2 LCD - Pin 6 (EN)     Enable Clock
Pin 9 (D9)    OUTPUT      16x2 LCD - Pin 11 (D4)    Data Bit 4
Pin 10 (D10)  OUTPUT      16x2 LCD - Pin 12 (D5)    Data Bit 5
Pin 11 (D11)  OUTPUT      16x2 LCD - Pin 13 (D6)    Data Bit 6
Pin 12 (D12)  OUTPUT      16x2 LCD - Pin 14 (D7)    Data Bit 7
5V            POWER       Breadboard Power Rail     Supplies VDD, A, & Pot
GND           GROUND      Breadboard Ground Rail    Common Ground Reference


2. 16x2 LCD (HD44780) PINOUT CONNECTION TABLE
----------------------------------------------------------------------
LCD Pin  Label  Function               Wiring Connection
----------------------------------------------------------------------
Pin 1    VSS    Ground                 Breadboard GND
Pin 2    VDD    Logic Power (+5V)      Breadboard +5V
Pin 3    V0     Contrast Voltage       Center Wiper of 10k Potentiometer
Pin 4    RS     Register Select        Arduino Pin 7
Pin 5    RW     Read / Write Select    Breadboard GND (Write Mode Only)
Pin 6    E      Enable Clock Pulse     Arduino Pin 8
Pin 7    D0     Data Bit 0 (Unused)    No Connection (NC)
Pin 8    D1     Data Bit 1 (Unused)    No Connection (NC)
Pin 9    D2     Data Bit 2 (Unused)    No Connection (NC)
Pin 10   D3     Data Bit 3 (Unused)    No Connection (NC)
Pin 11   D4     Data Bit 4             Arduino Pin 9
Pin 12   D5     Data Bit 5             Arduino Pin 10
Pin 13   D6     Data Bit 6             Arduino Pin 11
Pin 14   D7     Data Bit 7             Arduino Pin 12
Pin 15   A      Backlight Anode (+)    +5V (via 220 Ohm current-limiting resistor)
Pin 16   K      Backlight Cathode (-)  Breadboard GND


3. BUTTON WIRING (ACTIVE-LOW)
----------------------------------------------------------------------
Both tactile buttons utilize internal microcontroller pull-up resistors:
- Scan Button: Terminal A -> Arduino Pin 2  |  Terminal B -> GND
- Page Button: Terminal A -> Arduino Pin 3  |  Terminal B -> GND
*(Pressing either button shorts the input pin directly to ground)*


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
