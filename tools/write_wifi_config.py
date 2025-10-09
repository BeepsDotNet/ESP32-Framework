import os
import sys

def write_wifi_config_to_sd():
    """
    Write wifi.MD file to SD card with the specified credentials
    """

    # WiFi credentials
    ssid = "dRouter"
    password = "WEWENTTOTHESTORE"

    # Content for wifi.MD file
    wifi_content = f"""# WiFi Configuration File
# Automatically generated for Chess AI vs AI system
# Place this file as /wifi.MD on the root of your SD card
#
# Format: key = "value"
# Lines starting with # are comments and will be ignored

ssid = "{ssid}"
password = "{password}"
"""

    print("WiFi Configuration Writer for Chess AI vs AI")
    print("=" * 50)
    print(f"SSID: {ssid}")
    print(f"Password: {password}")
    print()

    # Try to find SD card drive
    sd_drives = []

    if os.name == 'nt':  # Windows
        import string
        available_drives = ['%s:' % d for d in string.ascii_uppercase if os.path.exists('%s:' % d)]

        for drive in available_drives:
            drive_path = drive + "\\"
            try:
                # Check if it's a removable drive (likely SD card)
                import subprocess
                result = subprocess.run(['fsutil', 'fsinfo', 'driveType', drive_path],
                                      capture_output=True, text=True)
                if 'Removable' in result.stdout:
                    sd_drives.append(drive_path)
            except:
                pass

    # Manual drive selection if auto-detection fails
    if not sd_drives:
        print("Could not auto-detect SD card drive.")
        print("Please enter the SD card drive letter (e.g., D:, E:, F:)")
        drive_input = input("SD Card Drive: ").strip().upper()

        if not drive_input.endswith(':'):
            drive_input += ':'

        if not drive_input.endswith('\\'):
            drive_input += '\\'

        if os.path.exists(drive_input):
            sd_drives.append(drive_input)
        else:
            print(f"Drive {drive_input} not found!")
            return False

    # If multiple drives found, let user choose
    if len(sd_drives) > 1:
        print("Multiple removable drives found:")
        for i, drive in enumerate(sd_drives):
            print(f"{i+1}. {drive}")

        try:
            choice = int(input("Select drive (number): ")) - 1
            if 0 <= choice < len(sd_drives):
                selected_drive = sd_drives[choice]
            else:
                print("Invalid selection!")
                return False
        except ValueError:
            print("Invalid input!")
            return False
    else:
        selected_drive = sd_drives[0]

    # Write wifi.MD file to SD card
    wifi_file_path = os.path.join(selected_drive, "wifi.MD")

    try:
        with open(wifi_file_path, 'w', encoding='utf-8') as f:
            f.write(wifi_content)

        print(f"\n✅ WiFi configuration written successfully!")
        print(f"📂 File location: {wifi_file_path}")
        print(f"📄 File size: {len(wifi_content)} bytes")

        # Verify the file was written correctly
        with open(wifi_file_path, 'r', encoding='utf-8') as f:
            written_content = f.read()

        if written_content == wifi_content:
            print("✅ File verification successful!")

            print("\n📋 File contents:")
            print("-" * 30)
            print(wifi_content)
            print("-" * 30)

            print("\n🚀 Next steps:")
            print("1. Safely eject the SD card from your computer")
            print("2. Insert the SD card into your ESP32")
            print("3. Power on your ESP32")
            print("4. The system will automatically load these WiFi credentials")

            return True
        else:
            print("❌ File verification failed!")
            return False

    except Exception as e:
        print(f"❌ Error writing file: {e}")
        return False

def verify_sd_card_structure(drive_path):
    """
    Check if the SD card has the expected structure
    """
    print(f"\n🔍 Checking SD card structure on {drive_path}")

    # Check for existing chess_games directory
    chess_games_path = os.path.join(drive_path, "chess_games")
    if os.path.exists(chess_games_path):
        print("✅ chess_games directory found")

        # Check subdirectories
        subdirs = ["config", "games", "logs"]
        for subdir in subdirs:
            subdir_path = os.path.join(chess_games_path, subdir)
            if os.path.exists(subdir_path):
                print(f"✅ {subdir} directory found")
            else:
                print(f"ℹ️  {subdir} directory will be created by ESP32")
    else:
        print("ℹ️  chess_games directory will be created by ESP32")

    return True

if __name__ == "__main__":
    print("Chess AI vs AI - WiFi Configuration Writer")
    print("This script will write wifi.MD to your SD card")
    print()

    success = write_wifi_config_to_sd()

    if success:
        print("\n🎉 WiFi configuration completed successfully!")
        print("Your ESP32 is now ready to connect to your WiFi network.")
    else:
        print("\n❌ WiFi configuration failed!")
        print("Please check the error messages above and try again.")

    input("\nPress Enter to exit...")