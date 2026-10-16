#include <LiquidCrystal.h>

// Hook up the 16x2 HD44780 LCD: RS=7, EN=8, D4=9, D5=10, D6=11, D7=12
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Physical pushbuttons (wired directly to ground, using internal pullups)
const uint8_t BTN_SCAN_PIN = 2; // Pin 2: Triggers recognition request over serial
const uint8_t BTN_PAGE_PIN = 3; // Pin 3: Toggles between stats view and text paging

// UI & Data Buffers (holds current active Pokémon info)
char currentName[11] = "POKEDEX"; // Max 10 chars + null terminator
uint8_t currentDex = 0;           // National dex number (001 - 255)
uint16_t currentBst = 0;          // Base stat total (scaled up from single byte)
char entryText[161];              // Flavor text buffer (up to 160 chars + null)
uint8_t entryLen = 0;             // Actual character count received

enum ScreenMode { MODE_STATS, MODE_ENTRY };
ScreenMode activeMode = MODE_STATS;
uint8_t textOffset = 0;           // Text scroll offset in multiples of 32 (16 cols x 2 rows)

// Software debounce state tracking (active-LOW: unpressed = HIGH, pressed = LOW)
uint8_t scanState = HIGH;
uint8_t lastScanReading = HIGH;
unsigned long lastScanDebounceTime = 0;

uint8_t pageState = HIGH;
uint8_t lastPageReading = HIGH;
unsigned long lastPageDebounceTime = 0;

const unsigned long DEBOUNCE_DELAY = 50; // 50ms filter to eliminate switch bounce

// Displays Pokémon Name on row 0, and padded #ID + BST on row 1
void renderStatsScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(currentName);

  lcd.setCursor(0, 1);
  lcd.print("#");
  // Pad with leading zeros to match classic Pokédex format (e.g. #025)
  if (currentDex < 10) lcd.print("00");
  else if (currentDex < 100) lcd.print("0");
  lcd.print(currentDex);
  lcd.print("  BST: ");
  lcd.print(currentBst);
}

// Renders up to 32 characters of flavor text across both LCD rows with blank padding
void renderEntryScreen() {
  lcd.clear();

  // Row 0: Characters textOffset through textOffset + 15
  lcd.setCursor(0, 0);
  for (uint8_t i = 0; i < 16; i++) {
    uint8_t idx = textOffset + i;
    if (idx < entryLen) lcd.write(entryText[idx]);
    else lcd.write(' ');
  }

  // Row 1: Characters textOffset + 16 through textOffset + 31
  lcd.setCursor(0, 1);
  for (uint8_t i = 0; i < 16; i++) {
    uint8_t idx = textOffset + 16 + i;
    if (idx < entryLen) lcd.write(entryText[idx]);
    else lcd.write(' ');
  }
}

void setup() {
  // Fast baud rate for rapid binary packet transfer from host/Raspberry Pi
  Serial.begin(115200);

  // Enable internal 20k-50k pull-ups so switches just need to pull pin to GND
  pinMode(BTN_SCAN_PIN, INPUT_PULLUP);
  pinMode(BTN_PAGE_PIN, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("POKEDEX OS v2.3");
  lcd.setCursor(0, 1);
  lcd.print("READY TO SCAN");
}

/*
 * Binary Protocol State Machine:
 * [0xAA] [13 bytes metadata] [N bytes text] [1 byte checksum] [0x55]
 * 
 * Metadata Layout (13 bytes total):
 * - Byte 0:       Dex Number (uint8)
 * - Byte 1:       Scaled BST (actual BST = byte * 4)
 * - Bytes 2-11:   Name (10 raw ASCII bytes, padded)
 * - Byte 12:      Payload length of flavor text (0-160)
 */
enum RxState { WAIT_HEADER, READ_METADATA, READ_TEXT, WAIT_CHECKSUM, WAIT_FOOTER };
RxState currentState = WAIT_HEADER;
uint8_t metaBuffer[13];
uint8_t metaIdx = 0;
uint8_t textIdx = 0;
uint8_t targetTextLen = 0;

void loop() {
  unsigned long now = millis();

  // --- BUTTON 1: SCAN TRIGGER (PIN 2) ---
  int readingScan = digitalRead(BTN_SCAN_PIN);
  if (readingScan != lastScanReading) {
    lastScanDebounceTime = now;
  }
  if ((now - lastScanDebounceTime) > DEBOUNCE_DELAY) {
    if (readingScan != scanState) {
      scanState = readingScan;
      // Falling edge: button was just pressed down
      if (scanState == LOW) {
        Serial.write('S'); // Send scan command trigger out to the host
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("IDENTIFYING...");
        lcd.setCursor(0, 1);
        lcd.print("PLEASE WAIT...");
      }
    }
  }
  lastScanReading = readingScan;

  // --- BUTTON 2: PAGE / FLIP SCREEN (PIN 3) ---
  int readingPage = digitalRead(BTN_PAGE_PIN);
  if (readingPage != lastPageReading) {
    lastPageDebounceTime = now;
  }
  if ((now - lastPageDebounceTime) > DEBOUNCE_DELAY) {
    if (readingPage != pageState) {
      pageState = readingPage;
      // Falling edge: page through screen data
      if (pageState == LOW) {
        if (entryLen > 0) { // Only allow paging if we actually have entry text loaded
          if (activeMode == MODE_STATS) {
            // Switch from main stats screen to first page of text
            activeMode = MODE_ENTRY;
            textOffset = 0;
            renderEntryScreen();
          } else {
            // Advance by 32 characters (1 full 16x2 screen)
            textOffset += 32;
            if (textOffset >= entryLen) {
              // Reached end of text -> loop back around to main stats screen
              activeMode = MODE_STATS;
              renderStatsScreen();
            } else {
              renderEntryScreen();
            }
          }
        }
      }
    }
  }
  lastPageReading = readingPage;

  // --- SERIAL PACKET PARSER ---
  while (Serial.available() > 0) {
    uint8_t b = Serial.read();

    switch (currentState) {
      case WAIT_HEADER:
        // Framing byte 0xAA starts the packet
        if (b == 0xAA) {
          metaIdx = 0;
          textIdx = 0;
          currentState = READ_METADATA;
        }
        break;

      case READ_METADATA:
        metaBuffer[metaIdx++] = b;
        if (metaIdx >= 13) {
          targetTextLen = metaBuffer[12];
          // Clamp length to prevent buffer overrun on rogue serial input
          if (targetTextLen > 160) targetTextLen = 160;
          currentState = READ_TEXT;
        }
        break;

      case READ_TEXT:
        if (textIdx < targetTextLen) {
          entryText[textIdx++] = (char)b;
        }
        if (textIdx >= targetTextLen) {
          entryText[textIdx] = '\0'; // Null-terminate string safely
          entryLen = textIdx;
          currentState = WAIT_CHECKSUM;
        }
        break;

      case WAIT_CHECKSUM:
        // Checksum placeholder byte
        currentState = WAIT_FOOTER;
        break;

      case WAIT_FOOTER:
        // Framing byte 0x55 verifies entire frame arrived intact
        if (b == 0x55) {
          currentDex = metaBuffer[0];
          currentBst = metaBuffer[1] * 4; // Unpack scaled BST
          memcpy(currentName, &metaBuffer[2], 10);
          currentName[10] = '\0'; // Enforce safe string termination

          // Reset view and paint the newly identified Pokémon
          activeMode = MODE_STATS;
          renderStatsScreen();
        }
        // Always reset state back to hunting for the next header byte
        currentState = WAIT_HEADER;
        break;
    }
  }
}
