# IAM-20380HT-gyroscope-using-Pi
Reading the IAM-20380HT gyroscope with the Invense DK-20380HT developement board and Raspberry Pi (i2C)

![DK-20380HT_Pi](https://github.com/user-attachments/assets/68085a4e-41a6-446b-9b5a-286d141a041e)


# IAM-20380HT Gyroscope Data Logger

A high-performance data logging utility for the InvenSense IAM-20380HT gyroscope sensor. This tool provides accurate gyroscope readings with proper calibration, temperature compensation, and high-speed sampling capabilities.

## Table of Contents

- [Overview](#overview)
- [Hardware Requirements](#hardware-requirements)
- [Physical Connections](#physical-connections)
- [Dependencies](#dependencies)
- [Installation](#installation)
- [Usage](#usage)
- [Features](#features)
- [Data Format](#data-format)
- [Troubleshooting](#troubleshooting)
- [License](#license)

## Overview

This application interfaces with the IAM-20380HT gyroscope sensor via I2C and provides several key features:

- High-speed sampling at 1000Hz
- Sensor self-test and validation
- Gyroscope calibration and zero-offset calculation
- Temperature compensation
- Timestamped data logging to CSV files
- Precise timing control for consistent sampling rates

## Hardware Requirements

- Raspberry Pi (any model with I2C capability)
- DK-20380HT development board or IAM-20380HT sensor
- Jumper wires
- 3.3V power source (provided by Raspberry Pi)

## Physical Connections

Connect the IAM-20380HT to the Raspberry Pi as follows:

| IAM-20380HT Pin | Raspberry Pi Pin | Description |
|-----------------|------------------|-------------|
| SDA (CN3 Pin 3) | GPIO 2 (Pin 3)   | I2C Data    |
| SCL (CN3 Pin 5) | GPIO 3 (Pin 5)   | I2C Clock   |
| GND (CN3 Pin 9) | Ground (Pin 6)   | Ground      |
| VCC (3.3V)      | 3.3V (Pin 1)     | Power       |

**Important Note:** Ensure you're using the CN3 header on the DK-20380HT board for I2C connections, as this provides the correct SDA and SCL pins. The I2C address of the device is set to 0x69 in the code.

## Dependencies

- Linux-based operating system (tested on Raspberry Pi OS)
- GCC compiler
- I2C development libraries
- Math library

## Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/yourusername/iam20380ht-gyro-logger.git
   cd iam20380ht-gyro-logger
   ```

2. Install required packages:
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential libi2c-dev i2c-tools
   ```

3. Enable I2C on your Raspberry Pi (if not already enabled):
   ```bash
   sudo raspi-config
   # Navigate to Interface Options > I2C > Enable
   ```

4. Compile the program:
   ```bash
   gcc -o gyro_logger gyro.c -lm
   ```

## Usage

1. Connect the IAM-20380HT to your Raspberry Pi according to the [Physical Connections](#physical-connections) section.

2. Run the program with sudo privileges (required for I2C access):
   ```bash
   sudo ./gyro_logger
   ```

3. The program will:
   - Verify connection to the sensor
   - Perform self-test and calibration
   - Calculate zero offsets (keep the sensor still during this process)
   - Begin logging data at 1000Hz to a timestamped file

4. Press Ctrl+C to safely stop data logging.

5. Find your data in the generated file named `gyro_data_YYYYMMDD_HHMMSS.txt`.

## Features

### 1. Self-Test and Calibration
The program performs factory self-test to validate the sensor's functionality by:
- Measuring baseline gyroscope values
- Enabling built-in self-test actuation
- Comparing response to factory calibration data
- Validating that test results are within acceptable range

### 2. Offset Calculation
To eliminate drift and bias:
- The program samples the gyroscope while stationary
- Calculates average zero-point offsets on all axes
- Applies temperature compensation to calibrate readings
- Applies these offsets to all subsequent measurements

### 3. High-Speed Sampling
For precise motion tracking:
- Configures the sensor for maximum bandwidth
- Maintains consistent 1000Hz sampling rate
- Uses precise timing control to maintain sampling frequency
- Adjusts sleep intervals dynamically to ensure timing accuracy

### 4. Timestamped Data Logging
For accurate data analysis:
- Creates a new log file with date/time in filename
- Records human-readable timestamps with each measurement
- Includes Unix epoch time for easy programmatic processing
- Periodically flushes data to ensure it's written to disk

## Data Format

The logged data is stored in CSV format with the following columns:

| Column     | Description                                 |
|------------|---------------------------------------------|
| Timestamp  | Human-readable date/time (YYYY-MM-DD HH:MM:SS) |
| UnixTime   | Unix epoch time in seconds                  |
| GyroX      | X-axis rotation rate in degrees per second  |
| GyroY      | Y-axis rotation rate in degrees per second  |
| GyroZ      | Z-axis rotation rate in degrees per second  |
| Temperature| Calibrated temperature in °C                |

Example:

## Troubleshooting

### Common Issues

1. **"Failed to open I2C device"**
   - Ensure I2C is enabled: `sudo raspi-config`
   - Check that you're running with sudo privileges
   - Verify that i2c-dev module is loaded: `lsmod | grep i2c`

2. **"Failed to set I2C slave address"**
   - Verify the I2C address is correct (default: 0x69)
   - Check connections to the sensor

3. **"Unexpected WHOAMI value"**
   - Verify correct sensor is connected
   - Check for correct I2C address
   - Ensure proper power to the sensor

4. **"Self-test FAILED"**
   - Keep the sensor stationary during self-test
   - Check for proper power supply to the sensor
   - Verify sensor is properly connected

### Verifying I2C Connection

Check if the sensor is detected on the I2C bus:
```bash
sudo i2cdetect -y 1
```

The IAM-20380HT should appear at address 0x69 (or 0x68 if AD0 pin is configured differently).

## License

This project is licensed under the MIT License - see the LICENSE file for details.


