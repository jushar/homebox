# Home box

A Zephyr-based home-automation firmware that integrates a couple of "legacy" devices
and exposes them for usage via Bluetooth (BLE) to be controlled by a set-top box:

* Philips sound system (IR Philips RC5 protocol)
* BenQ projector (IR NEC protocol)
* Ivolum/Celexon electric projector screen (OOK/ACK proprietary 433/315 MHz RF)

## Setup

Assuming you've already installed the required dependencies, west and the Zephyr SDK, run:

```
west init -l .
west update
```

## Build

### ST Nucleo F429ZI/F439ZI

This board is currently used for prototyping, but will be replaced by an ESP32 because of its connectivity features.
```
west build -b nucleo_f429zi -p auto app
```

### ESP32-C6 Devkit

TODO

## References
* Sample app: https://github.com/zephyrproject-rtos/example-application/tree/main/drivers

## Notes

### SigRok CSV format string from oscilosscope
```
format=t,1a
skiprows=4
```
