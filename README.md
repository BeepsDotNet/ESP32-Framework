# Chess User vs AI

An embedded chess system where a human player competes against an AI opponent (Gemini) on an Adafruit Feather ESP32 microcontroller.

## Hardware

- **Board**: Adafruit Feather ESP32 (original ESP32)
- **Port**: COM6
- **USB Chip**: CH9102 USB-to-serial converter

## Project Structure

```
├── src/                    # Source code
├── include/                # Header files
├── lib/                    # Project libraries
├── tests/                  # Unit tests
├── docs/                   # Documentation
├── tools/                  # Development tools and utilities
├── scripts/                # Build and deployment scripts
├── examples/               # Example code and demos
├── .github/workflows/      # GitHub Actions CI/CD
├── platformio.ini          # PlatformIO configuration
├── .gitignore             # Git ignore rules
└── README.md              # This file
```

## Development

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- USB drivers for CH9102 chip

### Building

```bash
pio run
```

### Uploading

```bash
pio run --target upload
```

### Monitoring

```bash
pio device monitor
```

## Features

- **Web-based Interface**: Play chess through a modern web interface
- **User vs AI**: Human player (white pieces) vs Gemini AI (black pieces)
- **Real-time Updates**: Live game status and move tracking
- **Move Input**: Simple text-based move entry using algebraic notation
- **Responsive Design**: Works on desktop and mobile devices

## Dependencies

- ESPAsyncWebServer-esphome
- AsyncTCP-esphome

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]