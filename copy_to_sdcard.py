#!/usr/bin/env python3
"""
Automatically copy chess-app.html to SD card
This script runs during PlatformIO build process
"""

import os
import shutil
import platform

def find_sd_card():
    """Find SD card drive letter on Windows or mount point on Linux/Mac"""
    if platform.system() == "Windows":
        # Common SD card drive letters
        for drive in ['D:', 'E:', 'F:', 'G:', 'H:', 'I:', 'J:']:
            if os.path.exists(drive + os.sep):
                # Check if it looks like an SD card (small size, removable)
                try:
                    # You might want to add more sophisticated detection here
                    return drive + os.sep
                except:
                    continue
    else:
        # Linux/Mac - check common mount points
        mount_points = ['/media', '/mnt', '/Volumes']
        for mount in mount_points:
            if os.path.exists(mount):
                for item in os.listdir(mount):
                    path = os.path.join(mount, item)
                    if os.path.isdir(path):
                        return path
    return None

def copy_html_to_sdcard():
    """Copy chess-app.html to SD card"""
    source_file = "chess-app.html"

    if not os.path.exists(source_file):
        print(f"Warning: {source_file} not found in project directory")
        return False

    # Try to find SD card
    sd_card = find_sd_card()
    if not sd_card:
        print("Warning: SD card not found. Please manually copy chess-app.html")
        return False

    destination = os.path.join(sd_card, "chess-app.html")

    try:
        shutil.copy2(source_file, destination)
        print(f"✅ Successfully copied {source_file} to {destination}")
        return True
    except Exception as e:
        print(f"❌ Error copying to SD card: {e}")
        print("Please manually copy chess-app.html to SD card")
        return False

if __name__ == "__main__":
    copy_html_to_sdcard()