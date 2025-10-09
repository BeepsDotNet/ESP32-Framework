import serial
import time

def analyze_com6_data():
    """
    Analyze the binary data stream from COM6
    """
    print("COM6 Device Info:")
    print("- USB-Enhanced-SERIAL CH9102")
    print("- Manufacturer: wch.cn (WinChipHead)")
    print("- Actively transmitting binary data")
    print()

    # Try different baud rates to see if we get readable data
    baud_rates = [9600, 115200, 57600, 38400, 19200, 2400, 1200]

    for baud in baud_rates:
        try:
            print(f"Testing {baud} baud...")

            with serial.Serial('COM6', baud, timeout=2) as ser:
                time.sleep(1)

                if ser.in_waiting > 0:
                    data = ser.read(min(50, ser.in_waiting))  # Read up to 50 bytes

                    print(f"  Raw bytes: {data}")
                    print(f"  Hex: {data.hex()}")

                    # Try to decode as text
                    try:
                        text = data.decode('utf-8', errors='ignore')
                        if text.isprintable():
                            print(f"  As text: '{text}'")
                    except:
                        pass

                    # Check for patterns
                    if len(set(data)) > 10:  # High entropy suggests data stream
                        print("  -> Looks like continuous data stream")
                    elif b'\x00' in data:
                        print("  -> Contains null bytes")

                    # Check for common protocols
                    if data.startswith(b'\xFF\xFE') or data.startswith(b'\xFE\xFF'):
                        print("  -> Possible Unicode BOM")
                    elif data[0] & 0x80:  # High bit set
                        print("  -> Binary protocol or wrong baud rate")

                    print()

        except Exception as e:
            print(f"  Error: {e}")

    print("Analysis:")
    print("This appears to be a microcontroller board with CH9102 USB-to-serial chip")
    print("transmitting continuous binary data. Could be:")
    print("1. Sensor data output")
    print("2. Debug information")
    print("3. Custom protocol communication")
    print("4. Wrong baud rate causing garbled text")
    print("5. ESP32/Arduino board running custom firmware")

if __name__ == "__main__":
    analyze_com6_data()