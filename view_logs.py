#!/usr/bin/env python3
"""
Chess AI Log Viewer
Fetches and displays unified debug log from ESP32 via HTTP
Uses chunked streaming to handle large log files
"""

import requests
import sys
import argparse
import subprocess
import platform
from datetime import datetime

def get_esp32_ip():
    """Prompt for ESP32 IP address"""
    default_ip = "192.168.1.208"
    ip = input(f"Enter ESP32 IP address (default: {default_ip}): ").strip()
    return ip if ip else default_ip

def fetch_log_chunked(ip, progress_callback=None):
    """
    Fetch unified debug log file from ESP32 using chunked streaming
    This allows reading arbitrarily large log files without memory limits
    """
    endpoint = f'http://{ip}/api/logs/console'

    try:
        print(f"Fetching debug log from {ip} (streaming mode)...")

        # Use streaming to handle large files
        response = requests.get(endpoint, stream=True, timeout=30)

        if response.status_code == 404:
            print(f"Debug log file not found on ESP32")
            return None
        elif response.status_code != 200:
            print(f"Error: HTTP {response.status_code}")
            return None

        # Get total file size if available
        total_size = response.headers.get('content-length')
        if total_size:
            total_size = int(total_size)
            print(f"Log file size: {total_size:,} bytes")

        # Read content in chunks
        content_chunks = []
        bytes_downloaded = 0
        chunk_size = 8192  # 8KB chunks

        for chunk in response.iter_content(chunk_size=chunk_size, decode_unicode=True):
            if chunk:
                content_chunks.append(chunk)
                bytes_downloaded += len(chunk.encode('utf-8'))

                # Show progress if we know the total size
                if total_size and progress_callback:
                    progress = (bytes_downloaded / total_size) * 100
                    progress_callback(bytes_downloaded, total_size, progress)

        content = ''.join(content_chunks)
        print(f"\n[OK] Downloaded {bytes_downloaded:,} bytes")
        return content

    except requests.exceptions.Timeout:
        print(f"Error: Connection timeout - log file may be very large")
        return None
    except requests.exceptions.RequestException as e:
        print(f"Error connecting to ESP32: {e}")
        return None

def fetch_log(ip):
    """Fetch unified debug log file from ESP32 (legacy non-streaming method)"""
    endpoint = f'http://{ip}/api/logs/console'

    try:
        print(f"Fetching debug log from {ip}...")
        response = requests.get(endpoint, timeout=10)

        if response.status_code == 200:
            return response.text
        elif response.status_code == 404:
            print(f"Debug log file not found on ESP32")
            return None
        else:
            print(f"Error: HTTP {response.status_code}")
            return None

    except requests.exceptions.RequestException as e:
        print(f"Error connecting to ESP32: {e}")
        return None

def clear_logs(ip):
    """Clear debug log file on ESP32"""
    try:
        print(f"Clearing logs on {ip}...")
        response = requests.post(f'http://{ip}/api/logs/clear', timeout=10)

        if response.status_code == 200:
            data = response.json()
            print(f"[OK] Debug log cleared: {data.get('debugCleared', False)}")
            return True
        else:
            print(f"Error: HTTP {response.status_code}")
            return False

    except requests.exceptions.RequestException as e:
        print(f"Error connecting to ESP32: {e}")
        return False

def save_log(content, ip):
    """Save log content to file"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"debug_log_{timestamp}.txt"

    try:
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(f"Log Type: Unified Debug Log (Serial + Browser)\n")
            f.write(f"ESP32 IP: {ip}\n")
            f.write(f"Downloaded: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write("=" * 60 + "\n\n")
            f.write(content)
        print(f"[OK] Saved to: {filename}")
        return filename
    except Exception as e:
        print(f"Error saving file: {e}")
        return None

def open_in_editor(filename):
    """Open file in default text editor"""
    if not filename:
        return False

    try:
        system = platform.system()
        if system == "Windows":
            # Use notepad on Windows
            subprocess.Popen(['notepad.exe', filename])
            print(f"[OK] Opened in Notepad: {filename}")
        elif system == "Darwin":  # macOS
            subprocess.Popen(['open', '-a', 'TextEdit', filename])
            print(f"[OK] Opened in TextEdit: {filename}")
        else:  # Linux
            # Try common Linux text editors
            for editor in ['gedit', 'kate', 'mousepad', 'xdg-open']:
                try:
                    subprocess.Popen([editor, filename])
                    print(f"[OK] Opened in {editor}: {filename}")
                    return True
                except FileNotFoundError:
                    continue
            print(f"Warning: Could not find text editor. File saved to: {filename}")
        return True
    except Exception as e:
        print(f"Error opening file: {e}")
        return False

def display_log(content, max_lines=None):
    """Display log content to console"""
    lines = content.split('\n')

    if max_lines:
        if len(lines) > max_lines:
            print(f"\n... (showing last {max_lines} lines of {len(lines):,} total) ...\n")
            lines = lines[-max_lines:]

    for line in lines:
        print(line)

def progress_indicator(bytes_downloaded, total_size, progress):
    """Show download progress"""
    bar_length = 40
    filled_length = int(bar_length * progress / 100)
    bar = '#' * filled_length + '-' * (bar_length - filled_length)
    sys.stdout.write(f'\rDownloading: [{bar}] {progress:.1f}% ({bytes_downloaded:,}/{total_size:,} bytes)')
    sys.stdout.flush()

def main():
    parser = argparse.ArgumentParser(description='View Chess AI ESP32 unified debug log')
    parser.add_argument('-i', '--ip', help='ESP32 IP address')
    parser.add_argument('-s', '--save', action='store_true', help='Save log to file')
    parser.add_argument('-c', '--clear', action='store_true', help='Clear log on ESP32')
    parser.add_argument('-l', '--lines', type=int, help='Show only last N lines')
    parser.add_argument('-o', '--open', action='store_true', help='Open saved log file in text editor (requires --save)')
    parser.add_argument('--no-stream', action='store_true', help='Use legacy non-streaming mode')

    args = parser.parse_args()

    # Get ESP32 IP
    ip = args.ip if args.ip else get_esp32_ip()

    # Clear logs if requested
    if args.clear:
        if clear_logs(ip):
            print("\nLog cleared successfully!")
        return

    # Fetch unified debug log
    print(f"\n{'='*60}")
    print(f"  UNIFIED DEBUG LOG (Serial + Browser)")
    print(f"{'='*60}\n")

    # Use streaming mode by default for large files
    if args.no_stream:
        content = fetch_log(ip)
    else:
        content = fetch_log_chunked(ip, progress_callback=progress_indicator)

    if content:
        total_lines = len(content.split('\n'))
        print(f"Total log lines: {total_lines:,}")

        # Save to file if requested
        saved_filename = None
        if args.save:
            saved_filename = save_log(content, ip)

        # Open in editor if requested
        if args.open:
            if saved_filename:
                open_in_editor(saved_filename)
            else:
                print("Warning: --open requires --save flag to save file first")

        # Display to console
        display_log(content, args.lines)
        print(f"\n{'='*60}\n")

if __name__ == '__main__':
    main()
