#!/usr/bin/env python3
"""
View timestamped crash logs from ESP32
Shows previous session logs before reboots
"""

import requests
import sys

def view_crash_logs(esp32_ip="192.168.1.208", file_name=None):
    """Fetch and display crash logs from ESP32"""

    print("=" * 60)
    print("  CRASH LOGS (Previous Sessions)")
    print("=" * 60)
    print()

    try:
        # Get list of crash logs
        list_url = f"http://{esp32_ip}/api/crashlogs"
        print(f"Fetching crash log list from {esp32_ip}...")
        response = requests.get(list_url, timeout=10)

        if response.status_code != 200:
            print(f"Error: HTTP {response.status_code}")
            return

        crash_logs = response.json()

        if not crash_logs:
            print("No crash logs found")
            return

        # Sort by name (timestamp) - newest first
        crash_logs.sort(key=lambda x: x['name'], reverse=True)

        print(f"Found {len(crash_logs)} crash log(s):\n")
        for i, log in enumerate(crash_logs, 1):
            print(f"  {i}. {log['name']} ({log['size']} bytes)")
        print()

        # If specific file requested, show only that one
        if file_name:
            target_log = next((log for log in crash_logs if log['name'] == file_name), None)
            if not target_log:
                print(f"Error: Crash log '{file_name}' not found")
                return
            logs_to_show = [target_log]
        else:
            # Show latest crash log
            logs_to_show = [crash_logs[0]]
            print(f"Showing latest: {logs_to_show[0]['name']}\n")

        # Fetch and display each log
        for log in logs_to_show:
            content_url = f"http://{esp32_ip}/api/crashlog?file={log['name']}"
            response = requests.get(content_url, timeout=10)

            if response.status_code == 200:
                print("-" * 60)
                print(f"File: {log['name']}")
                print("-" * 60)
                print(response.text)
                print()
            else:
                print(f"Error fetching {log['name']}: HTTP {response.status_code}")

        print("=" * 60)
        print("End of crash log(s)")
        print("=" * 60)

    except requests.exceptions.Timeout:
        print(f"Error: Connection to {esp32_ip} timed out")
    except requests.exceptions.ConnectionError:
        print(f"Error: Could not connect to {esp32_ip}")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    esp32_ip = "192.168.1.208"
    file_name = None

    if len(sys.argv) > 1:
        # First arg could be IP or filename
        if sys.argv[1].startswith("CrashLog_"):
            file_name = sys.argv[1]
        else:
            esp32_ip = sys.argv[1]

    if len(sys.argv) > 2:
        file_name = sys.argv[2]

    view_crash_logs(esp32_ip, file_name)
