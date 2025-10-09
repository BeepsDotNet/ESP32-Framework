import serial
import time
import sys

def comprehensive_board_detection(port='COM9'):
    """
    Enhanced detection with more commands and better parsing
    """

    baudrates = [9600, 115200, 57600, 38400, 19200, 2400, 1200, 4800]

    # Common commands for different board types
    detection_commands = [
        # Arduino/ESP commands
        (b'\r\n', 'newline'),
        (b'AT\r\n', 'AT command'),
        (b'AT+GMR\r\n', 'ESP version'),
        (b'AT+CGMI\r\n', 'manufacturer info'),

        # MicroPython/CircuitPython
        (b'\x03', 'Ctrl+C (interrupt)'),
        (b'\r\nimport sys; print(sys.implementation)\r\n', 'Python implementation'),
        (b'\r\nprint("Hello")\r\n', 'Python print'),

        # General commands
        (b'help\r\n', 'help command'),
        (b'?\r\n', 'question mark'),
        (b'info\r\n', 'info command'),
        (b'version\r\n', 'version command'),
        (b'\r\n', 'second newline'),
    ]

    for baudrate in baudrates:
        try:
            print(f"\n{'='*50}")
            print(f"Trying {port} at {baudrate} baud...")

            ser = serial.Serial(
                port=port,
                baudrate=baudrate,
                timeout=3,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS
            )

            time.sleep(1)  # Allow connection to stabilize

            # Clear buffers
            ser.flushInput()
            ser.flushOutput()

            print(f"Connected successfully at {baudrate} baud")

            # Check for immediate output
            time.sleep(1)
            if ser.in_waiting > 0:
                initial_data = ser.read(ser.in_waiting)
                print(f"Initial data: {initial_data}")
                analyze_response(initial_data)

            # Send detection commands
            for cmd, description in detection_commands:
                try:
                    print(f"\nSending: {description}")
                    ser.write(cmd)
                    time.sleep(1.5)  # Wait for response

                    if ser.in_waiting > 0:
                        response = ser.read(ser.in_waiting)
                        print(f"Response ({len(response)} bytes): {response}")
                        board_type = analyze_response(response)
                        if board_type:
                            print(f"*** DETECTED: {board_type} ***")
                            ser.close()
                            return board_type, baudrate
                    else:
                        print("No response")

                except Exception as e:
                    print(f"Error with command {description}: {e}")

            # Final check for any delayed responses
            print("\nWaiting for delayed responses...")
            time.sleep(3)
            if ser.in_waiting > 0:
                final_data = ser.read(ser.in_waiting)
                print(f"Delayed data: {final_data}")
                board_type = analyze_response(final_data)
                if board_type:
                    print(f"*** DETECTED: {board_type} ***")
                    ser.close()
                    return board_type, baudrate

            ser.close()

        except serial.SerialException as e:
            print(f"Serial error at {baudrate} baud: {e}")
        except Exception as e:
            print(f"Unexpected error at {baudrate} baud: {e}")

    return None, None

def analyze_response(data):
    """
    Analyze response data to identify board type
    """
    if not data:
        return None

    try:
        text = data.decode('utf-8', errors='ignore').lower()
    except:
        text = str(data).lower()

    # Arduino patterns
    if any(keyword in text for keyword in ['arduino', 'atmega', 'avr']):
        return "Arduino Board"

    # ESP patterns
    if any(keyword in text for keyword in ['esp32', 'esp8266', 'espressif']):
        return "ESP32/ESP8266 Board"

    # Raspberry Pi Pico
    if any(keyword in text for keyword in ['pico', 'rp2040', 'micropython']):
        return "Raspberry Pi Pico"

    # STM32
    if any(keyword in text for keyword in ['stm32', 'stm', 'arm cortex']):
        return "STM32 Board"

    # CircuitPython
    if 'circuitpython' in text:
        return "CircuitPython Board"

    # MicroPython
    if 'micropython' in text:
        return "MicroPython Board"

    # Nordic
    if any(keyword in text for keyword in ['nordic', 'nrf52', 'nrf51']):
        return "Nordic nRF Board"

    # Generic patterns
    if any(keyword in text for keyword in ['ok', 'ready', '>>>', '>>> ']):
        return "Interactive Console/REPL"

    if b'AT' in data and b'OK' in data:
        return "AT Command Device (possibly ESP or modem)"

    return None

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM9'
    print(f"Enhanced detection for processor board on {port}")

    board_type, baudrate = comprehensive_board_detection(port)

    if board_type:
        print(f"\n{'='*60}")
        print(f"RESULT: {board_type} detected at {baudrate} baud")
    else:
        print(f"\n{'='*60}")
        print("Could not identify the processor board type")
        print("The device may be:")
        print("- Not powered on")
        print("- Using a non-standard baud rate")
        print("- Not responding to standard commands")
        print("- A custom or proprietary device")