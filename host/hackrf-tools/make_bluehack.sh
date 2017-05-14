#
# Copyright (c) 2017, Michael Stahn <michael.stahn.42@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

# Build script for bluehack, the BTLE scanner for HackRF
# Requirements:
# - HackRF hardware
# - GNU Compiler Collection
# - git
# - dfu-util
# - pypacker
# - Python 3.x

function msg {
	echo ""
	echo ">>> $1"
}

msg "Compiling libhackrf"
pushd ../libhackrf/
# remove previous cached build to avoid problems
rm -rf build && mkdir build
cd build
cmake ../
make
popd

msg "Compiling libhackrf-wrapper for bluehack"
pushd src
make clean
make
popd


DIR_PYPACKER="pypacker"

pushd src
if [ ! -d "$DIR_PYPACKER" ]; then
	msg "Cloning Pypacker"
	git clone https://github.com/mike01/pypacker.git

	if [ $? -ne 0 ]; then
		msg "Unable to download pypacker, check your internet connection, will now continue..."
	fi
fi
if [ -d "$DIR_PYPACKER" ]; then
	msg "Installing pypacker. If this raises errors chances are you need to be root. Manual install: cd src/pypacker -> python setup.py install"
	pushd pypacker
	git checkout master
	git pull
	python setup.py install
	popd
fi
popd

msg "Now upload firmware to HackRF in DFU-mode (repeat after every off/on):"
cat <<EOF
DFU-mode: Hold down the DFU button while powering it on or while pressing and
releasing the RESET button. Release the DFU button after the 3V3 LED illuminates.
Then call 'dfu-util --device 1fc9:000c --alt 0 --download ../../firmware/hackrf_usb/build/hackrf_usb.dfu'
EOF

msg "After firmware upload: Green blinking LED = standby. Call 'python bluehack.py --help' to see available commands."
