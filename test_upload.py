#!/usr/bin/env python3
"""
Quick test script to verify HTML upload system
"""

import requests
import os

def test_upload():
    esp32_ip = "192.168.1.229"
    html_file = "chess-app.html"

    print("🧪 Testing HTML upload system...")

    # Check if HTML file exists
    if not os.path.exists(html_file):
        print(f"❌ {html_file} not found!")
        return False

    # Read HTML content
    with open(html_file, 'r', encoding='utf-8') as f:
        html_content = f.read()

    print(f"📊 File size: {len(html_content)} bytes")

    # Test upload
    try:
        url = f"http://{esp32_ip}/api/upload-html"
        response = requests.post(url, data={'html': html_content}, timeout=30)

        if response.status_code == 200:
            result = response.json()
            if result.get('success'):
                print("✅ Upload test PASSED!")
                print(f"📁 Saved as: {result.get('filename')}")
                print(f"📊 Size: {result.get('size')} bytes")
                return True
            else:
                print(f"❌ Upload test FAILED: {result.get('message')}")
        else:
            print(f"❌ HTTP Error {response.status_code}")

    except Exception as e:
        print(f"❌ Connection error: {e}")

    return False

def test_browse():
    esp32_ip = "192.168.1.229"

    print("🌐 Testing web interface...")

    try:
        url = f"http://{esp32_ip}/"
        response = requests.get(url, timeout=10)

        if response.status_code == 200:
            print("✅ Web interface test PASSED!")
            print(f"📊 Response size: {len(response.text)} bytes")
            return True
        else:
            print(f"❌ HTTP Error {response.status_code}")

    except Exception as e:
        print(f"❌ Connection error: {e}")

    return False

if __name__ == "__main__":
    print("🚀 Testing Malcolm's Chess v2.9 SD Card System")
    print("=" * 50)

    upload_ok = test_upload()
    print()
    browse_ok = test_browse()

    print("\n" + "=" * 50)
    if upload_ok and browse_ok:
        print("🎉 ALL TESTS PASSED! System ready for development.")
    else:
        print("⚠️  Some tests failed. Check ESP32 connection and SD card.")

    input("\nPress Enter to continue...")