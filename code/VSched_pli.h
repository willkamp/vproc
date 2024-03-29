//=====================================================================
//
// VSched_pli.h                                       Date: 2004/12/13
//
// Copyright (c) 2004-2024 Simon Southwell.
//
// This file is part of VProc.
//
// VProc is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VProc is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VProc. If not, see <http://www.gnu.org/licenses/>.
//
//=====================================================================
//
// VSched function's export prototypes
//
//=====================================================================

#include <string.h>

#if !defined(NVC) && !defined(GHDL)
#include "veriuser.h"
#include "vpi_user.h"
#endif

#ifdef INCL_VLOG_MEM_MODEL
#include "mem_model.h"
#endif

#ifndef _VSCHED_PLI_H_
#define _VSCHED_PLI_H_
#endif

// VHPI implies VHDL
#ifdef VPROC_VHDL_VHPI

# ifndef VPROC_VHDL
#define VPROC_VHDL
# endif

#endif

# ifdef VPROC_VHDL

#define VPROC_TF_TBL_SIZE  0
#define VPROC_TF_TBL       {0}

#  ifdef VPROC_VHDL_VHPI

#   ifdef DEBUG
#   define debug_io_printf vhpi_printf
#   else
#   define debug_io_printf //
#   endif

#include <vhpi_user.h>

#define VINIT_PARAMS       const struct vhpiCbDataS* cb
#define VSCHED_PARAMS      const struct vhpiCbDataS* cb
#define VPROCUSER_PARAMS   const struct vhpiCbDataS* cb
#define VIRQ_PARAMS        const struct vhpiCbDataS* cb
#define VACCESS_PARAMS     const struct vhpiCbDataS* cb
#define VHALT_PARAMS       int, int

#define VINIT_NUM_ARGS     1
#define VSCHED_NUM_ARGS    7
#define VPROCUSER_NUM_ARGS 2
#define VIRQ_NUM_ARGS      2
#define VACCESS_NUM_ARGS   4

#define VPROC_RTN_TYPE     void

#  else

#   ifdef DEBUG
#   define debug_io_printf mti_PrintFormatted
#   else
#   define debug_io_printf //
#   endif
    
#define VINIT_PARAMS       int  node
#define VSCHED_PARAMS      int  node, int Interrupt, int VPDataIn, int* VPDataOut, int* VPAddr, int* VPRw,int* VPTicks
#define VPROCUSER_PARAMS   int  node, int value
#define VIRQ_PARAMS        int  node, int value
#define VACCESS_PARAMS     int  node, int  idx, int VPDataIn, int* VPDataOut
#define VHALT_PARAMS       int, int

#define VPROC_RTN_TYPE     void

#  endif

# else

#  ifndef VPROC_PLI_VPI
#  ifdef DEBUG
#  define debug_io_printf io_printf
#  else
#  define debug_io_printf //
#  endif

#define VPROC_TF_TBL \
    {usertask, 0, NULL, 0, VInit,     NULL,  "$vinit",     1}, \
    {usertask, 0, NULL, 0, VSched,    NULL,  "$vsched",    1}, \
    {usertask, 0, NULL, 0, VAccess,   NULL,  "$vaccess",   1}, \
    {usertask, 0, NULL, 0, VProcUser, NULL,  "$vprocuser", 1}, \
    {usertask, 0, NULL, 0, VIrq,      NULL,  "$virq",      1}

#define VPROC_TF_TBL_SIZE 5

#define VINIT_PARAMS      void
#define VSCHED_PARAMS     void
#define VPROCUSER_PARAMS  void
#define VIRQ_PARAMS       void
#define VACCESS_PARAMS    void
#define VHALT_PARAMS      int data, int reason

#define VPROC_RTN_TYPE    int

#  else

#  ifdef DEBUG
#  define debug_io_printf vpi_printf
#  else
#  define debug_io_printf //
#  endif

#define VPROC_TF_TBL_SIZE  1
#define VPROC_TF_TBL       {0}

#define VINIT_PARAMS      char* userdata
#define VSCHED_PARAMS     char* userdata
#define VPROCUSER_PARAMS  char* userdata
#define VIRQ_PARAMS       char* userdata
#define VACCESS_PARAMS    char* userdata
#define VHALT_PARAMS      int data, int reason

#define VPROC_RTN_TYPE    int

#  endif

# endif

extern VPROC_RTN_TYPE VInit     (VINIT_PARAMS);
extern VPROC_RTN_TYPE VSched    (VSCHED_PARAMS);
extern VPROC_RTN_TYPE VProcUser (VPROCUSER_PARAMS);
extern VPROC_RTN_TYPE VIrq      (VIRQ_PARAMS);
extern VPROC_RTN_TYPE VAccess   (VACCESS_PARAMS);
extern int            VHalt     (VHALT_PARAMS);

