import cv2
import serial
import struct
import time
import requests
import re
import textwrap
from ultralytics import YOLO

# --- CONFIGURATION ---
SERIAL_PORT = "COM3"
BAUD_RATE = 115200
MODEL_PATH = r"E:\General\Projects\pokedex_project\runs\detect\train-3\weights\best.pt"

# Establish serial communication with Uno
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
time.sleep(2.0)  # Wait for ATmega328P auto-reset

# Load model onto RTX 3070 Ti
model = YOLO(MODEL_PATH)
model.to("cuda")

# In-memory dictionary cache to prevent redundant PokeAPI hits
data_cache = {}

def sanitize_and_wrap(text, max_screens=4):
    """
    Cleans text, applies word-wrapping at 16 characters per line, 
    and pads each line to 16 characters to form clean 32-character screens.
    """
    clean = text.replace("\n", " ").replace("\f", " ").replace("\r", " ")
    clean = re.sub(r"[^a-zA-Z0-9 .,!\'\-]", "", clean)
    clean = re.sub(r"\s+", " ", clean).strip().upper()
    
    # Split text along full words with max 16 chars per line
    lines = textwrap.wrap(clean, width=16)
    
    # Pack lines in pairs (16 chars + 16 chars = 32 char LCD screen)
    screens = []
    for i in range(0, len(lines), 2):
        l1 = f"{lines[i]:<16}"
        l2 = f"{lines[i+1]:<16}" if i + 1 < len(lines) else " " * 16
        screens.append(l1 + l2)
        if len(screens) >= max_screens:
            break
            
    return "".join(screens)

def get_pokemon_info(name):
    name_clean = name.lower().replace(" ", "").replace(".", "").replace("'", "")
    if name_clean in data_cache:
        return data_cache[name_clean]

    info = {
        "dex": 0, 
        "bst": 200, 
        "entry": sanitize_and_wrap("NO DATA AVAILABLE FOR THIS TARGET.")
    }
    
    try:
        # 1. Fetch Pokémon ID and Base Stats
        res_poke = requests.get(f"https://pokeapi.co/api/v2/pokemon/{name_clean}", timeout=2.5)
        if res_poke.status_code == 200:
            d = res_poke.json()
            info["dex"] = d["id"]
            info["bst"] = sum(s["base_stat"] for s in d["stats"])

        # 2. Fetch Pokémon Red/Blue Flavor Text
        res_species = requests.get(f"https://pokeapi.co/api/v2/pokemon-species/{name_clean}", timeout=2.5)
        if res_species.status_code == 200:
            sp_data = res_species.json()
            for flavor in sp_data.get("flavor_text_entries", []):
                if flavor["language"]["name"] == "en" and flavor["version"]["name"] in ["red", "blue", "firered"]:
                    info["entry"] = sanitize_and_wrap(flavor["flavor_text"])
                    break

        data_cache[name_clean] = info
        return info
    except Exception as e:
        print(f"PokeAPI Error: {e}")
        return info

def dispatch_pokemon(name, dex, bst, entry_text):
    formatted_name = f"{name[:10]:<10}"
    bst_scaled = min(255, int(bst / 4))
    text_bytes = entry_text.encode("ascii", errors="ignore")
    text_len = len(text_bytes)

    # Frame XOR Checksum
    checksum = (dex ^ bst_scaled ^ text_len) & 0xFF

    # Packet Structure:
    # 0xAA (Header) | Dex | BST/4 | 10-char Name | TextLen | Text Payload | Checksum | 0x55 (Footer)
    packet_format = f"<BBB10sB{text_len}sBB"
    packet = struct.pack(
        packet_format,
        0xAA,
        dex,
        bst_scaled,
        formatted_name.encode("ascii"),
        text_len,
        text_bytes,
        checksum,
        0x55
    )

    ser.write(packet)
    print(f"\n[+] Dispatched -> #{dex:03d} {name.strip()} | BST: {bst}")
    print(f"    Screens: {text_len // 32} total")

# Start OpenCV Video Stream
cap = cv2.VideoCapture(0)
cv2.namedWindow("Pokedex Vision Feed", cv2.WINDOW_NORMAL)
cv2.resizeWindow("Pokedex Vision Feed", 960, 540)

try:
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        cv2.putText(frame, "POKEDEX READY | PIN 2: SCAN | PIN 3: ENTRY", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 255, 0), 2)
        cv2.imshow("Pokedex Vision Feed", frame)

        # Check for hardware scan command 'S' from Pin 2
        if ser.in_waiting > 0:
            cmd = ser.read().decode("ascii", errors="ignore")
            if cmd == "S":
                print("\n[!] Scan trigger received from hardware!")
                results = model.predict(frame, verbose=False, conf=0.45)
                boxes = results[0].boxes

                if len(boxes) > 0:
                    cls_id = int(boxes[0].cls[0])
                    detected_name = model.names[cls_id]
                    info = get_pokemon_info(detected_name)
                    dispatch_pokemon(detected_name.upper(), info["dex"], info["bst"], info["entry"])
                else:
                    print("No target detected.")
                    dispatch_pokemon("UNKNOWN", 0, 0, sanitize_and_wrap("TARGET COULD NOT BE IDENTIFIED."))

# Exit if 'q' is pressed OR if the user clicks the 'X' button on the window
        if (cv2.waitKey(1) & 0xFF == ord("q")) or (
            cv2.getWindowProperty("Pokedex Vision Feed", cv2.WND_PROP_VISIBLE)
            < 1
        ):
            break

finally:
    cap.release()
    cv2.destroyAllWindows()
    ser.close()