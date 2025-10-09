#!/usr/bin/env python3
"""
Chunked HTML upload to ESP32 via HTTP
Sends file in small chunks to avoid ESP32 memory limitations
"""

import requests
import os
import sys

def upload_html_chunked(esp32_ip="192.168.1.229", html_file="chess-app.html", chunk_size=1024):
    """Upload HTML file to ESP32 in chunks"""

    if not os.path.exists(html_file):
        print(f"ERROR: {html_file} not found!")
        return False

    file_size = os.path.getsize(html_file)
    print(f"Uploading {html_file} to ESP32 at {esp32_ip}...")
    print(f"File size: {file_size} bytes")
    print(f"Chunk size: {chunk_size} bytes")

    try:
        # Step 1: Start upload session
        start_url = f"http://{esp32_ip}/api/upload-start"
        start_data = {'filename': html_file, 'filesize': str(file_size)}

        print("1. Starting upload session...")
        response = requests.post(start_url, data=start_data, timeout=10)

        if response.status_code != 200:
            print(f"ERROR: Start upload failed - HTTP {response.status_code}")
            return False

        result = response.json()
        if not result.get('success'):
            print(f"ERROR: Start upload failed - {result.get('message')}")
            return False

        print(f"   Upload session started for {result.get('filename')}")

        # Step 2: Send file in chunks
        chunk_url = f"http://{esp32_ip}/api/upload-chunk"

        with open(html_file, 'r', encoding='utf-8') as f:
            chunk_index = 0
            total_chunks = (file_size + chunk_size - 1) // chunk_size

            while True:
                chunk_data = f.read(chunk_size)
                if not chunk_data:
                    break

                print(f"2. Sending chunk {chunk_index + 1}/{total_chunks} ({len(chunk_data)} bytes)...")

                chunk_payload = {
                    'chunk': chunk_data,
                    'chunkIndex': str(chunk_index)
                }

                response = requests.post(chunk_url, data=chunk_payload, timeout=15)

                if response.status_code != 200:
                    print(f"ERROR: Chunk {chunk_index} failed - HTTP {response.status_code}")
                    return False

                result = response.json()
                if not result.get('success'):
                    print(f"ERROR: Chunk {chunk_index} failed - {result.get('message')}")
                    return False

                chunk_index += 1

        # Step 3: Finish upload
        finish_url = f"http://{esp32_ip}/api/upload-finish"

        print("3. Finishing upload...")
        response = requests.post(finish_url, timeout=10)

        if response.status_code != 200:
            print(f"ERROR: Finish upload failed - HTTP {response.status_code}")
            return False

        result = response.json()
        if not result.get('success'):
            print(f"ERROR: Finish upload failed - {result.get('message')}")
            return False

        print(f"SUCCESS: Upload completed!")
        print(f"Final file: {result.get('filename')}")
        print(f"Final size: {result.get('finalSize')} bytes")
        return True

    except requests.exceptions.ConnectTimeout:
        print(f"ERROR: Connection timeout. Is ESP32 running at {esp32_ip}?")
    except requests.exceptions.ConnectionError:
        print(f"ERROR: Connection failed. Check ESP32 IP address: {esp32_ip}")
    except Exception as e:
        print(f"ERROR: Upload failed: {e}")

    return False

if __name__ == "__main__":
    # Get ESP32 IP from command line or use default
    esp32_ip = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.229"

    success = upload_html_chunked(esp32_ip)
    if not success:
        print("\nMake sure:")
        print("  1. ESP32 is connected and running")
        print("  2. ESP32 is in development mode")
        print("  3. IP address is correct")
        print("  4. SD card is properly inserted")

    input("\nPress Enter to continue...")