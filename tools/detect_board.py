import serial
import time
import sys

def detect_processor_board(port='COM9', baudrates=[9600, 115200, 57600, 38400, 19200]):
    """
    Attempt to detect what processor board is connected to the specified port
    """

    for baudrate in baudrates:
        try:
            print(f"Trying {port} at {baudrate} baud...")

            # Open serial connection
            ser = serial.Serial(
                port=port,
                baudrate=baudrate,
                timeout=2,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS
            )

            time.sleep(0.5)  # Give time for connection

            # Clear any existing data
            ser.flushInput()
            ser.flushOutput()

            # Try common commands to identify the board
            commands = [
                b'\r\n',  # Just a newline
                b'AT\r\n',  # AT command for some modules
                b'?\r\n',  # Help command
                b'help\r\n',  # Help command
                b'info\r\n',  # Info command
                b'version\r\n',  # Version command
                b'\x03',  # Ctrl+C
            ]

            print(f"Connected to {port} at {baudrate} baud")

            # Send commands and read responses
            for cmd in commands:
                try:
                    ser.write(cmd)
                    time.sleep(0.5)

                    if ser.in_waiting > 0:
                        response = ser.read(ser.in_waiting)
                        if response:
                            print(f"Command: {cmd}")
                            print(f"Response: {response}")
                            print("-" * 40)
                except Exception as e:
                    print(f"Error sending command {cmd}: {e}")

            # Also try to read any spontaneous output
            print("Listening for spontaneous output...")
            time.sleep(2)
            if ser.in_waiting > 0:
                spontaneous = ser.read(ser.in_waiting)
                print(f"Spontaneous output: {spontaneous}")

            ser.close()
            return baudrate

        except serial.SerialException as e:
            print(f"Could not connect at {baudrate} baud: {e}")
            continue
        except Exception as e:
            print(f"Unexpected error at {baudrate} baud: {e}")
            continue

    print("Could not establish communication at any standard baud rate")
    return None

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM9'
    print(f"Attempting to detect processor board on {port}")
    print("=" * 50)

    result = detect_processor_board(port)
    if result:
        print(f"\nSuccessfully communicated at {result} baud")
    else:
        print("\nFailed to establish communication")