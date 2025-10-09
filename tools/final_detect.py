import serial
import time

def detect_usb_serial_board(port='COM9'):
    """
    Focused detection for USB Serial Device on COM9
    """
    print("Detected: USB Serial Device on COM9")
    print("Attempting to identify the specific processor board...")

    # Common baud rates for USB serial devices
    baud_rates = [9600, 115200, 57600, 38400, 19200]

    for baud in baud_rates:
        try:
            print(f"\nTrying {baud} baud...")

            with serial.Serial(port, baud, timeout=2) as ser:
                time.sleep(1)

                # Clear buffers
                ser.reset_input_buffer()
                ser.reset_output_buffer()

                # Check for boot messages or existing output
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting)
                    print(f"Existing data: {data}")

                # Try common identification commands
                test_commands = [
                    (b'\r\n', 'Newline'),
                    (b'AT\r\n', 'AT command'),
                    (b'?\r\n', 'Help'),
                    (b'help\r\n', 'Help command'),
                    (b'\x03\r\n', 'Ctrl+C'),  # MicroPython interrupt
                    (b'print("test")\r\n', 'Python test'),
                ]

                for cmd, desc in test_commands:
                    print(f"  Sending: {desc}")
                    ser.write(cmd)
                    time.sleep(1)

                    if ser.in_waiting > 0:
                        response = ser.read(ser.in_waiting)
                        print(f"  Response: {response}")

                        # Analyze response
                        text = response.decode('utf-8', errors='ignore').lower()

                        if 'arduino' in text:
                            return f"Arduino board at {baud} baud"
                        elif any(x in text for x in ['esp32', 'esp8266']):
                            return f"ESP32/ESP8266 board at {baud} baud"
                        elif 'micropython' in text or '>>>' in text:
                            return f"MicroPython board (likely Pico/ESP) at {baud} baud"
                        elif 'circuitpython' in text:
                            return f"CircuitPython board at {baud} baud"
                        elif 'ok' in text and 'at' in text:
                            return f"AT command device at {baud} baud"
                        elif any(x in text for x in ['stm32', 'arm']):
                            return f"STM32/ARM board at {baud} baud"

                # Try to trigger a reset or get version info
                print("  Trying reset sequence...")
                ser.write(b'\x03\x04')  # Ctrl+C, Ctrl+D
                time.sleep(2)
                if ser.in_waiting > 0:
                    reset_response = ser.read(ser.in_waiting)
                    print(f"  Reset response: {reset_response}")

        except Exception as e:
            print(f"Error at {baud} baud: {e}")

    return "Unknown USB Serial Device - no identifying response received"

if __name__ == "__main__":
    result = detect_usb_serial_board()
    print(f"\nFINAL RESULT: {result}")

    print("\nAdditional notes:")
    print("- Device appears as 'USB Serial Device' in Windows")
    print("- Could be Arduino, ESP32/ESP8266, Raspberry Pi Pico, or other microcontroller")
    print("- May be in bootloader mode or not running firmware that responds to commands")
    print("- Check if device needs to be reset or if specific software is required")