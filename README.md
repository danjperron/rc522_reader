# rc522_reader
RC522 RFID reader base on google code from alexs   
url:https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/rpi-rc522/source-archive.zip

Modification :
- Usage of SPI communication with standard ioctl call.
- Add GPIO acces via /dev/gpiomem.

No need to sudo anymore!


N.B. The GPIO are now using the BCM label description. 

To compile just do a make in the rc522_reader folder.

