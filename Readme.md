### General information
This is Bluehack, a BTLE sniffer using [HackRF](https://github.com/mossmann/hackrf)

Features:
* Listen for packets on advertising channels
* Follow connections
* Show signal strength, CRC checks
* Write packets to pcap file
* python based interface

Example output:

	AdvNonconnInd        CH: 39 sig: 17/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 77/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 78/255 (CRC: OK, Miss: 0)
	AdvNonconnInd        CH: 39 sig: 16/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 78/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 79/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 78/255 (CRC: OK, Miss: 0)
	ScanRequest          CH: 39 sig: 77/255 (CRC: NOK, Miss: 0)
	AdvNonconnInd        CH: 39 sig: 15/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 79/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 78/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 77/255 (CRC: OK, Miss: 0)
	AdvInd               CH: 39 sig: 77/255 (CRC: OK, Miss: 0)

### Requirements
The following hardware/software is needed to build/use Bluehack:

* HackRF hardware
* Linux based system
* GNU Compiler Collection
* Git
* [dfu-util](http://dfu-util.sourceforge.net)
* [pypacker](https://github.com/mike01/pypacker)
* Python 3.x

### Installation
This is an on-the-go installation, avoiding to pollute the system (too much).
Modules are build in place without copying anything to system directories except the python libs.
As a consequence the Bluehack directory should *not* be moved after building, otherwise a re-build is needed.
The firmware is just uploaded temporarely into HackRF so a re-upload is needed after every power cycle.

Install procedure:
* cd [bluehack directory]/host/hackrf-tools
* ./make_bluehack.sh (root or virtualenv needed to install pypacker)
* Download firmware to HackRF RAM. This is a *temporary* installation: the original firmware will be restored on reset!

  Hold down the DFU button while powering it on or while pressing and
  releasing the RESET button. Release the DFU button after the 3V3 LED illuminates. Then:

  dfu-util --device 1fc9:000c --alt 0 --download [bluehack directory]/firmware/hackrf_usb/build/hackrf_usb.dfu

  The green LED should now blink which means it's in idle mode.

### Usage examples
cd [bluehack directory]/host/hackrf-tools

* Listen for packets on advertising channels (press enter to circulate through channels 37,38,39,37...):

  python bluehack.py --mode 0
* Same as above + write packets to pcap file

  python bluehack.py --mode 0 --pcpafile packets.pcap
* Listen for *any* connection setup and follow it (experimental)

  python bluehack.py --mode 1 --pcpafile packets.pcap
* Listen for connections targeting an advertising address and follow it (experimental)

  python bluehack.py --mode 1 --pcpafile packets.pcap --advaddr AABBCCDDEEFF
* Listen for *any* connection setup by circulating through all advertising channels and follow it (experimental)

  python bluehack.py --mode 2 --pcpafile packets.pcap


Call `python bluehack.py --help` for more infos.

### Technical details
The demodulation of the BTLE-packets takes place in firmware as an USB connection is too slow to be able to react
on packets like connection requests. The data flow is as follows: `HackRF(Demodulation) --BTLE-packets--> Host`.
The USB-Connection allows to set the BTLE mode for receiving and the channel to listen on.

### FAQ / troubleshoting
On any error: make sure the green LED is blinking in idle mode before starting listening. Re-upload the firmware on errors.

**Q**:	No packets are showing up

**A**:	Move HackRF or the sending devices a bit. Chances are the HackRF is in a dead spot.

**Q**:	There are many CRC-errors

**A**:	Place the HackRF closer to the sending device. But not too close. The signal strength should give
        an indication about the needed distance.

**Q**:	Connection following was not triggered although the devices succeffully connected.

**A**:	There can be multiple reasons for this:
        1) The connection request was sent on a different advertising channel.
        Chances are approx 33.3% hitting the correct advertising channel. Try mode 2.
        2) The connection request was missed because of poor signal quality.

**Q**:	Connection following does not constantly keep up with the packets

**A**:	There can be multiple reasons for this:
        1) Packets are missed bcause of poor signal quality
        2) The timing logic is still a bit experimental. I'm working on it :D
