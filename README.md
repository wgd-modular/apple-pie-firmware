# apple-pie-firmware

The firmware repository for the apple-pie module of wgd modular.

### Flashing your module

To flash your Eurorack module using a USBasp programmer, you can use the Arduino IDE. This method will also allow you to flash the Arduino Nano bootloader to the ATmega328P on your module.

First, download and install the Arduino IDE from the official Arduino website. Once installed, open the Arduino IDE.

Next, clone the firmware repository from GitHub by opening a terminal and running:

```bash
git clone https://github.com/wgd-modular/apple-pie-firmware.git
```

Navigate to the directory:

```bash
cd apple-pie-firmware
```

After opening the file with the Arduino IDE, connect your USBasp programmer to your computer and your module. Ensure that the ISP header is connected correctly.

In the Arduino IDE, select Tools > Board > Arduino Nano and set Processor to ATmega328P. Then, set Port to the appropriate COM port (if applicable), and set Programmer to USBasp.

To flash the Arduino Nano bootloader, go to Tools > Burn Bootloader. This will install the Arduino Nano bootloader onto the ATmega328P chip.

Once the bootloader is installed, you can flash the firmware. Go to Sketch > Upload Using Programmer. The Arduino IDE will use the USBasp programmer to flash the apple-pie-firmware.hex file to your module.

After flashing is complete, disconnect the USBasp programmer and power cycle your module. Your apple-pie should now be running the new firmware.
