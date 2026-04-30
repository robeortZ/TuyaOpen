# EINK NFC Demo Application

This demo application demonstrates the hardware features of the TUYA_T5AI_EINK_NFC board.

## Features

- **Hardware Initialization**: Initializes all board peripherals including audio, buttons, LED, SD card, power domains, charge detection, and battery ADC.

- **Charge Detection**: 
  - Registers a callback function that triggers when charger is plugged/unplugged
  - Displays charge state in status reports

- **Battery Monitoring**:
  - Reads battery voltage (in mV) and percentage (0-100%)
  - Displays battery status periodically

- **Button State Monitoring**:
  - Monitors all 6 buttons (UP, DOWN, ENTER, RETURN, LEFT, RIGHT)
  - Displays button press/release states

## Status Reports

The demo prints a status report every 3 seconds showing:
- Charge state (PLUGGED/UNPLUGGED)
- Battery voltage and percentage
- State of all 6 buttons

When the charge state changes, an immediate notification is printed.

## Usage

1. Build and flash the application to the TUYA_T5AI_EINK_NFC board
2. Connect to the serial console (UART0) to see the status output
3. The demo will automatically start and begin printing status reports

## Example Output

```
========================================
TUYA_T5AI_EINK_NFC Demo Application
========================================
Hardware initialized
Charge detect callback registered

Initial Status:
=== Charge Status ===
  State:       UNPLUGGED
=== Battery Status ===
  Voltage:     3850 mV (3.85 V)
  Percentage:  71%
=== Button States ===
  UP (P22):    RELEASED
  DOWN (P23):  RELEASED
  ENTER (P24): RELEASED
  RETURN (P25): RELEASED
  LEFT (P26):  RELEASED
  RIGHT (P28): RELEASED

========== Status Report ==========
=== Charge Status ===
  State:       PLUGGED
=== Battery Status ===
  Voltage:     4100 mV (4.10 V)
  Percentage:  92%
=== Button States ===
  UP (P22):    PRESSED
  DOWN (P23):  RELEASED
  ENTER (P24): RELEASED
  RETURN (P25): RELEASED
  LEFT (P26):  RELEASED
  RIGHT (P28): RELEASED
===================================
```

## Configuration

The status print interval can be adjusted by modifying `STATUS_PRINT_INTERVAL` in `example_eink_nfc.c` (default: 3000ms).

