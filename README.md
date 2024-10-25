# apple-pie-firmware

The firmware repository for the apple-pie module of wgd modular.

## Flashing your module

Flashing the module can be done differently mostly dependent on what you have on hand. Here we explain the process using a USBasp programmer or any Arduino. Both ways require the same setup:

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

Open the `.ino` file using your Arduino IDE. Also make sure you have the [DAC Library](https://reference.arduino.cc/reference/en/libraries/mcp48xx-dac-library/) installed.

### USBasp

Connect your USBasp programmer to your computer and your module. Ensure that the ISP header is connected correctly so the GND marking on both the pcb and programmer line up.

In the Arduino IDE, select Tools > Board > Arduino Nano and set Processor to ATmega328P. Then, set Port to the appropriate COM port (if applicable), and set Programmer to USBasp.

To flash the Arduino Nano bootloader, go to Tools > Burn Bootloader. This will install the Arduino Nano bootloader onto the ATmega328P chip. **Attention**: Beware that you might need to set a jumper on your USBasp programmer to flash the bootloader (*JP3* on most chinese clones).

Once the bootloader is installed, you can flash the firmware. Go to Sketch > Upload Using Programmer. The Arduino IDE will use the USBasp programmer to flash the apple-pie-firmware.hex file to your module. **Absolutely make sure you really use _Upload Using Programmer_!**

After flashing is complete, disconnect the USBasp programmer and power cycle your module. Your apple-pie should now be running the new firmware.

### Arduino as ISP

If you don’t have a USBasp programmer, you can use an Arduino (e.g., Arduino Uno) to flash your Eurorack module. Follow the shared setup instructions above, then continue with the steps below.

To prepare the Arduino as an ISP, start by connecting your Arduino to your computer using a USB cable. Open the Arduino IDE, navigate to Tools > Board, and select your Arduino model (e.g., Arduino Uno). Set the Port to the port your Arduino is connected to, then go to File > Examples, select the correct Arduino as ISP sketch and upload this to your Arduino. This configures the Arduino as an ISP programmer.

Next, wire the Arduino to the module’s ISP header as follows: connect Arduino Pin 10 to the module’s RESET pin, Arduino Pin 11 to MOSI, Arduino Pin 12 to MISO, Arduino Pin 13 to SCK, Arduino GND to the module’s GND, and Arduino 5V to VCC.

In the Arduino IDE, go to Tools > Board and select Arduino Nano, then set the Processor to ATmega328P. Set the Programmer to “Arduino as ISP.”

Burn the bootloader by going to Tools > Burn Bootloader. This step installs the Arduino Nano bootloader onto the ATmega328P on your module, which is required before flashing the firmware.

With the .ino file open, go to Sketch > Upload Using Programmer to upload the firmware to your module via the Arduino. **Absolutely make sure you really use _Upload Using Programmer_!** Once the upload is complete, disconnect the Arduino and power cycle the module to start running the new firmware.
