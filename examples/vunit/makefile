###################################################################
# Makefile for Virtual Processor testcode in Modelsim
#
# Copyright (c) 2005-2024 Simon Southwell.
#
# This file is part of VProc.
#
# VProc is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# VProc is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with VProc. If not, see <http://www.gnu.org/licenses/>.
#
###################################################################

#------------------------------------------------------
# User overridable definitions

MAX_NUM_VPROC      = 64
USRFLAGS           =
SRCDIR             = ../../code
USRCDIR            = src
TESTDIR            = vunit_out/modelsim
VOBJDIR            = $(TESTDIR)/obj

# User test source code file list
USER_C             = $(USRCDIR)/VUserMain0.cpp $(USRCDIR)/VUserMain1.cpp

#------------------------------------------------------
# Settings specific to target simulator 

CC                 = $(MODEL_TECH)/../gcc64/bin/gcc
C++                = $(MODEL_TECH)/../gcc64/bin/g++ -std=c++17
AR                 = $(MODEL_TECH)/../gcc64/bin/gcc-ar
# Simulator/Language specific C/C++ compile and link flags
ARCHFLAG           = -m64
OPTFLAG            = -g
HDLLANGUAGE        = -DVPROC_VHDL
SIMULATOR          = -DMODELSIM
PLIVERSION         = -DVPROC_PLI_VPI
VERIUSEROBJ        = $(VOBJDIR)/veriuser.o
SIMINCLUDEFLAG     = -I$(MODEL_TECH)/../include
SIMFLAGSSO         = -L$(MODEL_TECH)/../linux_x86_64/ -lmtipli

# Optional Memory model definitions
MEM_C              =
MEMMODELDIR        = .

# Common logic simulator flags
VLOGFLAGS          = +define+VPROC_BURST_IF +define+VPROC_BYTE_INTERFACE +incdir+../
VPROC_TOP          = test
VSIMFLAGS          = -pli $(VPROC_PLI) $(VPROC_TOP)

#------------------------------------------------------
# MODEL_TECH path

# Get OS type
OSTYPE:=$(shell uname)

# If run from a place where MODEL_TECH is not defined, construct from path to PLI library
ifeq ("$(MODEL_TECH)", "")
  ifeq ($(OSTYPE), Linux)
    PLILIB         = libmtipli.so
  else
    PLILIB         = mtipli.dll
  endif

  VSIMPATH         = $(shell which vsim)
  SIMROOT          = $(shell dirname $(VSIMPATH))/..
  PLILIBPATH       = $(shell find $(SIMROOT) -name "$(PLILIB)")
  MODEL_TECH       = $(shell dirname $(PLILIBPATH))
endif

#------------------------------------------------------
# BUILD RULES
#------------------------------------------------------

all: $(TESTDIR)/VProc.so

# Include common build rules
include $(SRCDIR)/../test/makefile.common

#------------------------------------------------------
# EXECUTION RULES
#------------------------------------------------------

run: $(TESTDIR)/VProc.so
	@python3 run.py

rungui: 
	@python3 run.py -g

gui: rungui

.SILENT:
help:
	@$(info make help          Display this message)
	@$(info make               Build C/C++ and HDL code without running simulation)
	@$(info make run           Build and run batch simulation)
	@$(info make rungui/gui    Build and run GUI simulation)
	@$(info make clean         clean previous build artefacts)

#------------------------------------------------------
# CLEANING RULES
#------------------------------------------------------

clean:
	@rm -rf $(VPROC_PLI) $(VLIB) $(VOBJDIR) *.wlf transcript
	@if [ -d "./work" ]; then                              \
	    rm -rf work;                                       \
	fi

