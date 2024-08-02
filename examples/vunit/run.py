#!/usr/bin/python3

from vunit import VUnit

VU = VUnit.from_argv()
VU.add_builtins()
VU.add_verification_components()

LIB = VU.add_library("vproc")
LIB.add_source_file("../../f_vproc_pkg.vhd")
LIB.add_source_file("../../f_vproc_vunit.vhd")
LIB.add_source_file("avalonmm_scratchpad_tb.vhd")
LIB.add_source_file("wishbone_scratchpad_tb.vhd")

VU.main()