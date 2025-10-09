#!/usr/bin/env python3
"""
Upload chess-app.html to ESP32 via Serial
ESP32 will save it to SD card
"""

import serial
import time
import os
import sys

def upload_html_via_serial(port="COM6", html_file="chess-app.html", baudrate=115200):
    """Upload HTML file to ESP32 via serial for saving to SD card"""

    if not os.path.exists(html_file):
        print(f"❌ Error: {html_file} not found!")
        return False

    try:
        with open(html_file, 'r', encoding='utf-8') as f:
            html_content = f.read()

        print(f"📤 Uploading {html_file} via serial port {port}...")
        print(f"📊 File size: {len(html_content)} bytes")

        # Open serial connection
        ser = serial.Serial(port, baudrate, timeout=10)
        time.sleep(2)  # Wait for ESP32 to be ready

        # Send upload command
        ser.write(b"UPLOAD_HTML_START\n")
        time.sleep(0.1)

        # Send file size
        ser.write(f"SIZE:{len(html_content)}\n".encode())
        time.sleep(0.1)

        # Send content in chunks
        chunk_size = 512
        for i in range(0, len(html_content), chunk_size):
            chunk = html_content[i:i+chunk_size]
            ser.write(chunk.encode())
            time.sleep(0.05)  # Small delay between chunks

            # Show progress
            progress = min(100, (i + chunk_size) * 100 // len(html_content))
            print(f"\r📊 Progress: {progress}%", end="", flush=True)

        # Send end marker
        ser.write(b"\nUPLOAD_HTML_END\n")

        # Wait for response
        print(f"\n⏳ Waiting for ESP32 to save to SD card...")
        response = ser.readline().decode().strip()

        ser.close()

        if "SUCCESS" in response:
            print(f"✅ Successfully uploaded to SD card!")
            return True
        else:
            print(f"❌ ESP32 error: {response}")
            return False

    except serial.SerialException as e:
        print(f"❌ Serial error: {e}")
    except Exception as e:
        print(f"❌ Upload failed: {e}")

    return False

if __name__ == "__main__":
    # Get COM port from command line or use default
    port = sys.argv[1] if len(sys.argv) > 1 else "COM6"

    success = upload_html_via_serial(port)
    if not success:
        print(f"\n💡 Make sure:")
        print(f"  1. ESP32 is connected to {port}")
        print(f"  2. ESP32 is in development mode")
        print(f"  3. SD card is properly inserted")
        print(f"  4. Serial monitor is closed")

    input("Press Enter to continue...")