#!/usr/bin/env python3
"""
Chess AI Log Tail - Live log viewer
Continuously fetches and displays new unified debug log entries
"""

import requests
import time
import sys

def tail_log(ip, interval=2):
    """Tail unified debug log - show new entries as they appear"""
    endpoint = f'http://{ip}/api/logs/console'  # Now serves unified DebugMessages.log
    last_size = 0

    print(f"Tailing unified debug log from {ip} (Ctrl+C to stop)...")
    print("=" * 60)

    try:
        while True:
            try:
                response = requests.get(endpoint, timeout=5)

                if response.status_code == 200:
                    content = response.text
                    current_size = len(content)

                    # Only show new content
                    if current_size > last_size:
                        new_content = content[last_size:]
                        print(new_content, end='', flush=True)
                        last_size = current_size

                elif response.status_code == 404:
                    if last_size == 0:
                        print(f"Waiting for debug log file to be created...")
                    time.sleep(interval)
                    continue

            except requests.exceptions.RequestException as e:
                print(f"\nConnection error: {e}")
                print("Retrying in 5 seconds...")
                time.sleep(5)
                continue

            time.sleep(interval)

    except KeyboardInterrupt:
        print("\n\nStopped tailing log.")

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Tail ESP32 unified debug log in real-time')
    parser.add_argument('-i', '--ip', default='192.168.1.208', help='ESP32 IP address')
    parser.add_argument('-t', '--interval', type=float, default=2.0,
                       help='Update interval in seconds (default: 2)')

    args = parser.parse_args()

    tail_log(args.ip, args.interval)
