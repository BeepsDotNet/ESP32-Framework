#!/usr/bin/env python3
"""
Upload chess-app.html to ESP32 via HTTP
ESP32 will save it to SD card
"""

import requests
import os
import sys
import time

def cleanup_file_descriptors(esp32_ip):
    """Attempt to cleanup file descriptors on ESP32"""
    try:
        cleanup_url = f"http://{esp32_ip}/api/cleanup-files"
        response = requests.get(cleanup_url, timeout=5)
        if response.status_code == 200:
            print("File descriptors cleaned successfully")
            return True
        else:
            print(f"Cleanup failed: HTTP {response.status_code}")
    except Exception as e:
        print(f"Cleanup attempt failed: {e}")
    return False

def upload_html_attempt(esp32_ip, html_file, html_content):
    """Upload attempt - use chunked upload for large files"""

    # For small files (< 1KB), use direct upload
    if len(html_content) < 1024:
        return upload_direct(esp32_ip, html_content)
    else:
        return upload_chunked(esp32_ip, html_file, html_content)

def upload_direct(esp32_ip, html_content):
    """Direct upload for small files"""
    url = f"http://{esp32_ip}/api/upload-html"

    try:
        response = requests.post(url, data={'html': html_content}, timeout=30)

        if response.status_code == 200:
            result = response.json()
            if result.get('success'):
                print(f"SUCCESS: Successfully uploaded to SD card!")
                print(f"Saved as: {result.get('filename', 'chess-app.html')}")
                return True
            else:
                error_msg = result.get('message', 'Unknown error')
                print(f"ERROR: ESP32 error: {error_msg}")
                return False
        else:
            print(f"ERROR: HTTP Error {response.status_code}: {response.text}")
            return False

    except Exception as e:
        print(f"ERROR: Direct upload failed: {e}")
        return False

def upload_chunked(esp32_ip, html_file, html_content):
    """Chunked upload for large files using ESP32 chunked API"""
    filename = "/chess-app.html"  # Target filename on SD card

    try:
        print(f"Using chunked upload for large file ({len(html_content)} bytes)")

        # Step 1: Start upload
        start_url = f"http://{esp32_ip}/api/upload-start"
        start_data = {
            'filename': filename,
            'filesize': len(html_content)
        }

        response = requests.post(start_url, data=start_data, timeout=10)
        if response.status_code != 200:
            print(f"ERROR: Failed to start chunked upload: {response.text}")
            return False

        result = response.json()
        if not result.get('success'):
            print(f"ERROR: Upload start failed: {result.get('message')}")
            return False

        print("Chunked upload started successfully")

        # Step 2: Send chunks (as binary body data)
        chunk_size = 512  # 512 bytes per chunk to stay well under ESP32 limits
        chunk_url = f"http://{esp32_ip}/api/upload-chunk"
        total_chunks = (len(html_content) + chunk_size - 1) // chunk_size

        for chunk_index in range(total_chunks):
            start_pos = chunk_index * chunk_size
            end_pos = min(start_pos + chunk_size, len(html_content))
            chunk_data = html_content[start_pos:end_pos]

            # Send chunk as binary body data (not form-encoded)
            response = requests.post(
                chunk_url,
                data=chunk_data.encode('utf-8') if isinstance(chunk_data, str) else chunk_data,
                headers={'Content-Type': 'application/octet-stream'},
                timeout=10
            )
            if response.status_code != 200:
                print(f"ERROR: Failed to upload chunk {chunk_index}: {response.text}")
                return False

            result = response.json()
            if not result.get('success'):
                print(f"ERROR: Chunk {chunk_index} failed: {result.get('message')}")
                return False

            # Show progress
            progress = (chunk_index + 1) * 100 // total_chunks
            print(f"\rUploading chunks... {progress}% ({chunk_index + 1}/{total_chunks})", end="", flush=True)

        print()  # New line after progress

        # Step 3: Finish upload
        finish_url = f"http://{esp32_ip}/api/upload-finish"
        response = requests.post(finish_url, timeout=30)

        if response.status_code == 200:
            result = response.json()
            if result.get('success'):
                print(f"SUCCESS: Chunked upload completed!")
                print(f"Saved as: {result.get('filename', filename)}")
                print(f"Final size: {result.get('size', len(html_content))} bytes")
                return True
            else:
                error_msg = result.get('message', 'Unknown error')
                print(f"ERROR: Upload finish failed: {error_msg}")
                return False
        else:
            print(f"ERROR: Failed to finish upload: {response.text}")
            return False

    except Exception as e:
        print(f"ERROR: Chunked upload failed: {e}")
        return False

def upload_html_to_esp32(esp32_ip="192.168.1.208", html_file="chess-app.html", max_retries=3):
    """Upload HTML file to ESP32 with retry logic and cleanup"""

    if not os.path.exists(html_file):
        print(f"ERROR: {html_file} not found!")
        return False

    try:
        with open(html_file, 'r', encoding='utf-8') as f:
            html_content = f.read()

        print(f"Uploading {html_file} to ESP32 at {esp32_ip}...")
        print(f"File size: {len(html_content)} bytes")

        # Phase 1: Try cleanup first
        cleanup_file_descriptors(esp32_ip)
        time.sleep(0.5)  # Brief pause after cleanup

        # Phase 2: Upload with retry logic
        for attempt in range(max_retries):
            if attempt > 0:
                print(f"Retry attempt {attempt + 1}/{max_retries}...")
                # Try cleanup again before retry
                cleanup_file_descriptors(esp32_ip)
                time.sleep(1)  # Longer pause between retries

            success = upload_html_attempt(esp32_ip, html_file, html_content)
            if success:
                return True

            if attempt < max_retries - 1:
                print(f"Upload failed, retrying in 2 seconds...")
                time.sleep(2)

        print(f"Upload failed after {max_retries} attempts")
        return False

    except Exception as e:
        print(f"ERROR: Failed to read {html_file}: {e}")
        return False

if __name__ == "__main__":
    # Get ESP32 IP from command line or use default
    esp32_ip = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.208"

    success = upload_html_to_esp32(esp32_ip)
    if not success:
        print("\nMake sure:")
        print("  1. ESP32 is connected and running")
        print("  2. ESP32 is in development mode")
        print("  3. IP address is correct")
        print("  4. SD card is properly inserted")

    input("Press Enter to continue...")