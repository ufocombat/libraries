# NFC Library for Arduino to communicate with Android

This library is developed by [SeeedStudio](https://github.com/Seeed-Shield/NFC_Shield_DEV). [Elechouse](http://elechouse.com) changes the driver from `Spi` to `Serial`, in order to support the [NFC Module](http://goo.gl/i0EQgd) of elechouse.

## How To

1. Make the NFC module works in HSU mode.
2. Connect NFC Module with Arduino(At least 2 serials)
		
		PN532                     Arduino
		 VCC         -->          5V
		 GND         -->         GND
		 RXD         -->      Serial1-TX
		 TXD         -->      Serail1-RX
3. [Download zip file][zip], extract it to the `Arduino libraries` folder.
4. Open example `nfc_ndef_push_url`, upload it to Arduino.
5. Enable NFC function of your Android device, approximate it to NFC module, then the <http://elechouse.com> will be opened. (The url can be changed in code.)

[zip]:https://github.com/elechouse/NFC_Module_DEV/archive/HSU.zip

