import serial
import time

def quick_detect(port='COM9'):
    # Try most common baud rates first
    common_bauds = [9600, 115200, 57600]

    for baud in common_bauds:
        try:
            print(f"Testing {port} at {baud} baud...")

            ser = serial.Serial(port, baud, timeout=1)
            time.sleep(0.5)

            # Check for immediate output
            if ser.in_waiting > 0:
                data = ser.read_all()
                print(f"Immediate data: {data}")

            # Send simple commands
            commands = [b'\r\n', b'AT\r\n', b'\x03']

            for cmd in commands:
                ser.write(cmd)
                time.sleep(0.5)
                if ser.in_waiting > 0:
                    response = ser.read_all()
                    print(f"Response to {cmd}: {response}")

                    # Quick analysis
                    text = str(response).lower()
                    if 'arduino' in text:
                        print("*** ARDUINO DETECTED ***")
                    elif 'esp' in text:
                        print("*** ESP BOARD DETECTED ***")
                    elif 'micropython' in text or '>>>' in text:
                        print("*** MICROPYTHON/REPL DETECTED ***")
                    elif 'ok' in text:
                        print("*** AT COMMAND DEVICE DETECTED ***")

            ser.close()
            break

        except Exception as e:
            print(f"Error at {baud}: {e}")
            continue

if __name__ == "__main__":
    quick_detect('COM6')