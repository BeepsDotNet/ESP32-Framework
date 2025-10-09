#!/usr/bin/env python3
"""
Edit admin-auth.md file on ESP32 SD card via HTTP
Allows remote editing of admin IP addresses without physically accessing the SD card
"""

import sys
import os
import requests

ESP32_IP = "192.168.1.208"
ADMIN_FILE = "/admin-auth.md"
LOCAL_FALLBACK = "admin-auth.md"

def read_admin_file():
    """Read current admin IPs from ESP32 SD card via HTTP"""
    try:
        url = f"http://{ESP32_IP}/api/file/read?path={ADMIN_FILE}"
        response = requests.get(url, timeout=5)

        if response.status_code == 200:
            content = response.text
            # Parse IPs (skip comments and empty lines)
            ips = []
            for line in content.split('\n'):
                line = line.strip()
                if line and not line.startswith('#'):
                    ips.append(line)
            return ips, content
        elif response.status_code == 404:
            print(f"File {ADMIN_FILE} not found on ESP32 SD card")
            return [], ""
        else:
            print(f"Error reading file from ESP32: HTTP {response.status_code}")
            print("Falling back to local file mode...")
            return read_local_file()
    except requests.exceptions.RequestException as e:
        print(f"Error connecting to ESP32 at {ESP32_IP}: {e}")
        print("Falling back to local file mode...")
        return read_local_file()

def read_local_file():
    """Read from local file as fallback"""
    if not os.path.exists(LOCAL_FALLBACK):
        return [], ""

    with open(LOCAL_FALLBACK, 'r') as f:
        content = f.read()

    # Parse IPs (skip comments and empty lines)
    ips = []
    for line in content.split('\n'):
        line = line.strip()
        if line and not line.startswith('#'):
            ips.append(line)

    return ips, content

def write_admin_file(content):
    """Write content to ESP32 SD card via HTTP"""
    try:
        url = f"http://{ESP32_IP}/api/file/write"
        data = {
            'path': ADMIN_FILE,
            'content': content
        }
        response = requests.post(url, data=data, timeout=10)

        if response.status_code == 200:
            result = response.json()
            print(f"\n[OK] File written to ESP32: {result['path']} ({result['size']} bytes)")
            print("\n*** IMPORTANT: You must REBOOT the ESP32 for changes to take effect! ***")
            return True
        else:
            print(f"Error writing file to ESP32: HTTP {response.status_code}")
            print("Falling back to local file mode...")
            write_local_file(content)
            return False
    except requests.exceptions.RequestException as e:
        print(f"Error connecting to ESP32 at {ESP32_IP}: {e}")
        print("Falling back to local file mode...")
        write_local_file(content)
        return False

def write_local_file(content):
    """Write to local file as fallback"""
    with open(LOCAL_FALLBACK, 'w') as f:
        f.write(content)
    print(f"\n[OK] File '{LOCAL_FALLBACK}' saved locally")
    print(f"Next steps:")
    print(f"1. Copy '{LOCAL_FALLBACK}' to the SD card root as {ADMIN_FILE}")
    print(f"2. Reboot the ESP32")

def add_ip(ip_address):
    """Add an IP address to the admin list"""
    ips, content = read_admin_file()

    if ip_address in ips:
        print(f"IP {ip_address} is already in the admin list")
        return

    # Add to list
    if content:
        new_content = content.rstrip() + f"\n{ip_address}\n"
    else:
        # Create new file with header
        new_content = f"""# Admin Authentication File
# Place this file as /admin-auth.md on the ESP32 SD card
# List one IP address per line
# Lines starting with # are comments

{ip_address}
"""

    if write_admin_file(new_content):
        print(f"[OK] Added {ip_address} to admin list")
    else:
        print(f"[ERROR] Failed to add {ip_address}")

def remove_ip(ip_address):
    """Remove an IP address from the admin list"""
    ips, content = read_admin_file()

    if ip_address not in ips:
        print(f"IP {ip_address} is not in the admin list")
        return

    # Remove from content
    lines = content.split('\n')
    new_lines = [line for line in lines if line.strip() != ip_address]
    new_content = '\n'.join(new_lines)

    if write_admin_file(new_content):
        print(f"[OK] Removed {ip_address} from admin list")
    else:
        print(f"[ERROR] Failed to remove {ip_address}")

def list_ips():
    """List all current admin IPs"""
    ips, _ = read_admin_file()

    if not ips:
        print(f"No admin IPs configured")
        print("\nFile does not exist or is empty.")
    else:
        print(f"Current admin IPs ({len(ips)}):")
        for ip in ips:
            print(f"  - {ip}")

def show_usage():
    """Show usage information"""
    print("""
Admin IP Manager for ESP32 Chess (HTTP Remote Mode)

Usage:
  python edit_admin_ips.py list              - List current admin IPs
  python edit_admin_ips.py add <ip>          - Add an IP address
  python edit_admin_ips.py remove <ip>       - Remove an IP address

Examples:
  python edit_admin_ips.py list
  python edit_admin_ips.py add 192.168.1.38
  python edit_admin_ips.py remove 192.168.1.100

This script edits the admin-auth.md file directly on the ESP32 SD card via HTTP.
If the ESP32 is not reachable, it falls back to local file mode.
After making changes, you must REBOOT the ESP32 for them to take effect.
""")

def main():
    if len(sys.argv) < 2:
        show_usage()
        return

    command = sys.argv[1].lower()

    if command == "list":
        list_ips()

    elif command == "add":
        if len(sys.argv) < 3:
            print("Error: Missing IP address")
            print("Usage: python edit_admin_ips.py add <ip>")
            return
        add_ip(sys.argv[2])

    elif command == "remove":
        if len(sys.argv) < 3:
            print("Error: Missing IP address")
            print("Usage: python edit_admin_ips.py remove <ip>")
            return
        remove_ip(sys.argv[2])

    else:
        print(f"Error: Unknown command '{command}'")
        show_usage()

if __name__ == "__main__":
    main()
