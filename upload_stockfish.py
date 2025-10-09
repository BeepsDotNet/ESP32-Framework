#!/usr/bin/env python3
"""
Upload Stockfish engine files to ESP32 SD card
"""

import requests
import os
import sys

ESP32_IP = "192.168.1.208"
BASE_URL = f"http://{ESP32_IP}"

def upload_file(local_path, remote_path):
    """Upload a single file to ESP32"""
    if not os.path.exists(local_path):
        print(f"ERROR: File not found: {local_path}")
        return False

    file_size = os.path.getsize(local_path)
    print(f"\nUploading {local_path} to {remote_path}...")
    print(f"File size: {file_size:,} bytes")

    try:
        # Start chunked upload
        response = requests.post(
            f"{BASE_URL}/api/upload-start",
            data=f"filename={remote_path}&filesize={file_size}",
            headers={'Content-Type': 'application/x-www-form-urlencoded'},
            timeout=10
        )

        if not response.ok:
            print(f"ERROR: Failed to start upload: {response.status_code}")
            return False

        print("Upload session started successfully")

        # Upload file in chunks
        CHUNK_SIZE = 512  # Match ESP32 chunk size
        with open(local_path, 'rb') as f:
            chunk_num = 0
            total_chunks = (file_size + CHUNK_SIZE - 1) // CHUNK_SIZE

            while True:
                chunk = f.read(CHUNK_SIZE)
                if not chunk:
                    break

                response = requests.post(
                    f"{BASE_URL}/api/upload-chunk",
                    data=chunk,
                    headers={'Content-Type': 'application/octet-stream'},
                    timeout=10
                )

                chunk_num += 1
                progress = (chunk_num / total_chunks) * 100
                print(f"\rUploading chunks... {progress:.0f}% ({chunk_num}/{total_chunks})", end='', flush=True)

                if not response.ok:
                    print(f"\nERROR: Chunk upload failed: {response.status_code}")
                    return False

        print(f"\nAll chunks uploaded, finalizing...")

        # Finalize the upload
        response = requests.post(
            f"{BASE_URL}/api/upload-finish",
            timeout=10
        )

        if not response.ok:
            print(f"ERROR: Failed to finalize upload: {response.status_code}")
            return False

        print(f"Upload completed successfully")
        return True

    except Exception as e:
        print(f"ERROR: {e}")
        return False

def main():
    print("=" * 60)
    print("Stockfish Engine Upload Tool")
    print("=" * 60)

    files_to_upload = [
        ("stockfish.wasm.js", "/stockfish.wasm.js"),
        ("stockfish.wasm", "/stockfish.wasm")
    ]

    success_count = 0
    for local_file, remote_file in files_to_upload:
        if upload_file(local_file, remote_file):
            success_count += 1
        else:
            print(f"✗ Failed to upload {local_file}")

    print("\n" + "=" * 60)
    print(f"Upload Summary: {success_count}/{len(files_to_upload)} files uploaded successfully")
    print("=" * 60)

    if success_count == len(files_to_upload):
        print("\n[OK] All Stockfish files uploaded successfully!")
        print("You can now use the local Stockfish engine in the chess app.")
    else:
        print("\n[ERROR] Some files failed to upload. Please check the errors above.")
        sys.exit(1)

if __name__ == "__main__":
    main()
