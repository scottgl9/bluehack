"""
Copyright (c) 2017, Michael Stahn <michael.stahn.42@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street,
Boston, MA 02110-1301, USA.
"""
import sys
import os
import struct
from ctypes import cdll, c_int, create_string_buffer, byref
import threading
import argparse
import time
import logging

from pypacker.layer12 import btle
from pypacker import ppcap
from pypacker.checksum import crc_btle_init_reorder
from pypacker.utils import get_vendor_for_mac

logger = logging.getLogger("bluehack")
logger.setLevel(logging.DEBUG)
#logger.setLevel(logging.WARNING)

logger_streamhandler = logging.StreamHandler()
#logger_formatter = logging.Formatter("%(levelname)s (%(funcName)s): %(message)s")
logger_formatter = logging.Formatter("%(levelname)s: %(message)s")
logger_streamhandler.setFormatter(logger_formatter)

logger.addHandler(logger_streamhandler)

logger_filehandler = logging.FileHandler("%s.txt" % "logs")
logger_filehandler.setFormatter(logger_formatter)
logger.addHandler(logger_filehandler)

unpack_I = struct.Struct(">I").unpack

RETURN_OK = 0
RETURN_ERROR = 1

BTLE_MODE_OFF = 0
BTLE_MODE_FIXEDCHANNEL = 1
BTLE_MODE_FOLLOW_STATIC = 2
BTLE_MODE_FOLLOW_DYNAMIC = 3

DEMOD_BYTES_META_LEN = 10

RETURN_OK = 0
RETURN_ERROR = 1

FILENAME_LIB = "libhackrf_wrapper.so"
FILENAME_LIB_ABSOLUTE = os.path.join(os.path.dirname(__file__), "./src/build/" + FILENAME_LIB)

try:
	dll = cdll.LoadLibrary(FILENAME_LIB_ABSOLUTE)
except Exception as ex:
	logger.warning("Could not find wrapper lib: %s", FILENAME_LIB_ABSOLUTE)
	print(ex)
	sys.exit(1)


class HackRFBoard(object):
	"""
	Wrapper around bluehack customized libhackrf
	"""
	def __init__(self):
		self._ct_rcv_data = create_string_buffer(1024)
		self._ct_rcv_data_len = c_int()

	def open(self):
		logger.debug("initializing hackrf")
		ret = dll.open_board()

		if ret != RETURN_OK:
			raise Exception("Error opening board,: %d", ret)
		return ret

	def close(self):
		ret = dll.close_board()

		if ret != RETURN_OK:
			raise Exception("Error closing board: %d", ret)
		return ret

	def set_btle_mode(self, mode, adv_addr):
		logger.debug("setting BTLE mode: %d, AdvAddr: %s", mode, adv_addr)
		ret = dll.set_btle_mode(mode, adv_addr)

		if ret != RETURN_OK:
			# try to close on error
			logger.debug("trying to close board")
			self.close()
			raise Exception("Error setting mode %d: %d", mode, ret)
		return ret

	def set_btle_channel(self, channel):
		ret = dll.set_btle_channel(channel)

		if ret != RETURN_OK:
			raise Exception("Error sending channel %d: %d", channel, ret)
		return ret

	def get_next_packet(self):
		"""
		max_bts -- not used, max length is always 4096
		"""
		ret = dll.get_next_packet(
			self._ct_rcv_data,
			byref(self._ct_rcv_data_len))

		if ret != RETURN_OK:
			raise Exception("Error receiving")
		#logger.debug("bytes received: %d", self._ct_rcv_data_len.value)
		return self._ct_rcv_data.raw[:self._ct_rcv_data_len.value]

if __name__ != "__main__":
	logger.warning("File was re-imported, not executing!")
	sys.exit(1)


#
# Callbacks and et al
#

DATA_TO_RF_CHANNEL = {}

for ch in range(0, 11):
	DATA_TO_RF_CHANNEL[ch] = ch + 1

for ch in range(11, 37):
	DATA_TO_RF_CHANNEL[ch] = ch + 2

DATA_TO_RF_CHANNEL[37] = 0
DATA_TO_RF_CHANNEL[38] = 12
DATA_TO_RF_CHANNEL[39] = 39


def create_btle_header(packet_meta_btschannel, crc_ok=None):
	"""
	return -- btle header packet
	"""
	channel = packet_meta_btschannel[0]
	# data channel -> RF channel
	channel = DATA_TO_RF_CHANNEL[channel]

	# 0->255 => -128->127
	sig = (packet_meta_btschannel[1] - 128) & 0xFF
	btle_header = btle.BTLEHdr()

	btle_header.channel = channel
	btle_header.whitening = 1
	btle_header.signal = sig
	btle_header.sigvalid = 1

	if crc_ok is not None:
		btle_header.crcchecked = 1
		btle_header.crcvalid = 1 if crc_ok else 0
	return btle_header


def store_packet(fh_pcap, bts, crc_ok=None):
	"""
	Store parsed bts to pcap file

	fh_pcap -- Instance of ppcap.Writer
	"""
	bts_header = create_btle_header(bts[-DEMOD_BYTES_META_LEN:], crc_ok).bin()
	bts_pkt = bts[:-DEMOD_BYTES_META_LEN]
	fh_pcap.write(bts_header + bts_pkt)


def print_packet(args, bts):
	"""
	Print basic information to console.

	bts -- Bytes retrieved by HackRFBoard
	return -- True if crc ok, False otherwise
	"""
	crc_ok = False

	try:
		bts_pkt = bts[:-DEMOD_BYTES_META_LEN]
		pkt = btle.BTLE(bts_pkt)
		bts_ch = bts[-DEMOD_BYTES_META_LEN]
		bts_sig = bts[-DEMOD_BYTES_META_LEN + 1]
		counter = unpack_I(b"\x00" + bts[-DEMOD_BYTES_META_LEN + 2:-DEMOD_BYTES_META_LEN + 5])[0]
		upper_layer = pkt.upper_layer
		crc_ok = pkt.is_crc_ok(crc_init=args.crc_init)

		print("%-14s CH: %2d sig: %3d/255 (CRC: %-3s, Miss: %7d)" %
			(upper_layer.__class__.__name__,
			bts_ch,
			bts_sig,
			"OK" if crc_ok else "NOK",
			counter),
			end=""
		)

		if upper_layer.__class__ == btle.AdvInd:
			vendor = get_vendor_for_mac(upper_layer.adv_addr_s)

			print(" AdvAddr: %12s %s" % (upper_layer.adv_addr_s, vendor), end="")
		elif upper_layer.__class__ == btle.ScanRequest:
			vendor1 = get_vendor_for_mac(upper_layer.scan_addr_s)
			vendor2 = get_vendor_for_mac(upper_layer.adv_addr_s)

			print(" ScanAddr: %12s %s AdvAddr: %12s %s" %
				(upper_layer.scan_addr_s, vendor1,
				upper_layer.adv_addr_s, vendor2),
				end="")
		elif upper_layer.__class__ == btle.ScanResponse:
			vendor1 = get_vendor_for_mac(upper_layer.adv_addr_s)

			print(" AdvAddr: %12s %s" % (upper_layer.adv_addr_s, vendor1),
			end="")
		elif upper_layer.__class__ == btle.ConnRequest:
			print(" InitAddr: %12s AdvAddr: %12s AA: %4s CRCinit: %3s" %
				(upper_layer.init_addr_s,
				upper_layer.adv_addr_s,
				upper_layer.access_addr_s,
				upper_layer.crcinit),
				end="")
		print()
		# TODO: check if AdvAddr is given and compare
		if upper_layer.__class__ == btle.ConnRequest and args.btle_mode != BTLE_MODE_FIXEDCHANNEL:
			logger.debug("setting new CRC init")
			args.crc_init = crc_btle_init_reorder(upper_layer.crcinit_rev_int)
			print("channel map:")
			print(upper_layer.get_active_channels())
	except Exception as ex:
		logger.warning("could not parse: %r (corrupted packet)", bts)
		#print(ex)

	return crc_ok


def packet_cycler(args):
	"""
	Thread handle for printing/storing packets retrieved by HackRFBoard
	"""
	logger.debug("packet cycler started")

	while args.is_running:
		try:
			bts = args.hrf.get_next_packet()
		except:
			args.is_running = False
			break

		if bts is None or len(bts) == 0:
			continue
		crc_ok = print_packet(args, bts)

		if args.fh_pcap is not None:
			store_packet(args.fh_pcap, bts, crc_ok=crc_ok)


def scan_btle(args):
	"""
	Logic to handle bytes retrieved by HackRFBoard
	"""
	hrf = HackRFBoard()
	ret = hrf.open()

	if ret != RETURN_OK:
		return

	args.hrf = hrf

	# advaddr is already LE (UI input: BE -> Logic: LE)
	if args.advaddr is not None:
		logger.info("AdvAddr to follow (LE): %s", args.advaddr)


	# TODO: just for testing: start -> stop
	"""
	logger.debug("on -> off")
	hrf.set_btle_mode(BTLE_MODE_FIXEDCHANNEL, None)
	time.sleep(2)
	hrf.set_btle_mode(BTLE_MODE_OFF, None)
	logger.debug("sleeping...")
	time.sleep(10)
	"""

	hrf.set_btle_mode(args.btle_mode, args.advaddr)
	channel = 37
	args.is_running = True
	thread = threading.Thread(target=packet_cycler, args=[args])
	thread.start()

	while args.is_running:
		try:
			if args.btle_mode in [BTLE_MODE_FIXEDCHANNEL, BTLE_MODE_FOLLOW_STATIC]:
				logger.debug("setting channel to %d", channel)
				hrf.set_btle_channel(channel)
				channel = channel + 1 if channel < 39 else 37
			console_in = input()
		except KeyboardInterrupt:
			args.is_running = False
			dll.unlock_reader()
			logger.info("interrupted!")
			break

		if len(console_in) > 0:
			dll.unlock_reader()
			args.is_running = False

	args.is_running = False
	dll.unlock_reader()
	thread.join()
	hrf.close()


#
# Commmandline logic
#
MODE_DESCR = """
Exit: Ctrl+c and Enter

>>> Mode explanation:

All modes)
- Press Enter to switch channels
- Press [some key] + Enter or Ctrl + c to exit
Mode 0) Advertise scan
- Listen on advertising channels.
Mode 1) Connection follow (static)
- Follow connections on a fixed channel. Parameter advaddr *can* be set.
Mode 2) Connection follow (dynamic)
- Follow connections by following an advertising address. Parameter advaddr *must* be set.
"""


def auto_addr(x):
	bts_array = bytearray.fromhex(x)
	bts_array.reverse()
	return bytes(bts_array)

parser = argparse.ArgumentParser(description="Bluehack - The BTLE scanner using HackRF",
	epilog=MODE_DESCR,
	formatter_class=argparse.RawDescriptionHelpFormatter,)
parser.add_argument("-m", "--mode", type=int, help="chose one of 0=Advertise scan, 1=Connection follow (static), 2=Connection follow (dynamic)", required=False, default=0)
parser.add_argument("-p", "--pcapfile", type=str, help="PCAP filename to store packets to", default=None)
parser.add_argument("-a", "--advaddr", type=auto_addr, help="""Specific advertisting address to follow (mode 1 or 2). If not set all connections fill be followed""", default=None)

CMD_BTLE_MODES = {
	0: BTLE_MODE_FIXEDCHANNEL,
	1: BTLE_MODE_FOLLOW_STATIC,
	2: BTLE_MODE_FOLLOW_DYNAMIC
}

parsed_args = parser.parse_args()

parsed_args.fh_pcap = None
parsed_args.btle_mode = CMD_BTLE_MODES[parsed_args.mode]
parsed_args.crc_init = 0xAAAAAA

if parsed_args.btle_mode == BTLE_MODE_FOLLOW_DYNAMIC and parsed_args.advaddr is None:
	logger.warning("Parameter advaddr NOT set in mode 2!")
	sys.exit(1)

if parsed_args.pcapfile is not None:
	parsed_args.fh_pcap = ppcap.Writer(filename=parsed_args.pcapfile,
		linktype=ppcap.LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR)

scan_btle(parsed_args)

if parsed_args.fh_pcap is not None:
	parsed_args.fh_pcap.close()
