//=====================================================================
//
// VSched.c                                           Date: 2004/12/13
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
// Simulator VProc C interface routines for scheduling
// access to user processes
//
// Configurable to use PLI, VPI, FLI, VHPIDIRECT, VHPI (experimental)
// DPI-C. By default, VProc compiles for Verilog + PLI. By defining
// the following, it can be altered for other HDLs/PLIs.
//
//   VPROC_PLI_VPI                  : Verilog VPI
//   VPROC_VHDL                     : VHDL and FLI
//   VPROC_VHDL && VPROC_VHDL_VHPI  : VHDL and VHPI (experimental)
//   VPROC_VHDL && VPROC_NO_PLI     : VHDL and VHPIDIRECT
//   VPROC_SV                       : SystemVerilog DPI-C
//
//=====================================================================

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "VProc.h"
#include "VUser.h"
#include "VSched_pli.h"

#define ARGS_ARRAY_SIZE     10

// Pointers to state for each node (up to VP_MAX_NODES)
pSchedState_t ns[VP_MAX_NODES];

// VHPI specific functions
#if defined(VPROC_VHDL_VHPI)

// -------------------------------------------------------------------------
// reg_foreign_procs()
//
// Function setting up table of foreign procedure registration data
// and registering with VHPI
// -------------------------------------------------------------------------

static VPROC_RTN_TYPE reg_foreign_procs() {
    int idx;
    vhpiForeignDataT foreignDataArray[] = {
        {vhpiProcF, (char*)"VProc", (char*)"VInit",           NULL, VInit},
        {vhpiProcF, (char*)"VProc", (char*)"VSched",          NULL, VSched},
        {vhpiProcF, (char*)"VProc", (char*)"VProcUser",       NULL, VProcUser},
        {vhpiProcF, (char*)"VProc", (char*)"VIrq",            NULL, VIrq},
        {vhpiProcF, (char*)"VProc", (char*)"VAccess",         NULL, VAccess},
        {0}
    };

    for (idx = 0; idx < ((sizeof(foreignDataArray)/sizeof(foreignDataArray[0]))-1); idx++)
    {
        vhpi_register_foreignf(&(foreignDataArray[idx]));
    }
}

// -------------------------------------------------------------------------
// List of user startup functions to call
// -------------------------------------------------------------------------
#ifdef WIN32
__declspec ( dllexport )
#endif

VPROC_RTN_TYPE (*vhpi_startup_routines[])() = {
    reg_foreign_procs,
    0L
};

// -------------------------------------------------------------------------
// getVhpiParams()
//
// Get the parameter values of a foreign procedure using VHPI methods
// -------------------------------------------------------------------------

static void getVhpiParams(const struct vhpiCbDataS* cb, int args[], int args_size)
{
    int         idx      = 0;
    vhpiValueT  value;

    vhpiHandleT hParam;
    vhpiHandleT hScope   = cb->obj;
    vhpiHandleT hIter    = vhpi_iterator(vhpiParamDecls, hScope);

    while ((hParam = vhpi_scan(hIter)) && idx < args_size)
    {
        value.format     = vhpiIntVal;
        value.bufSize    = 0;
        value.value.intg = 0;
        vhpi_get_value(hParam, &value);
        args[idx++]      = value.value.intg;
        DebugVPrint("getVhpiParams(): %s = %d\n", vhpi_get_str(vhpiNameP, hParam), value.value.intg);
    }
}

// -------------------------------------------------------------------------
// setVhpiParams()
//
// Set the parameters values of a foreign procedure using VHPI methods
// -------------------------------------------------------------------------

static void setVhpiParams(const struct vhpiCbDataS* cb, int args[], int start_of_outputs, int args_size)
{
    int         idx      = 0;
    vhpiValueT  value;

    vhpiHandleT hParam;
    vhpiHandleT hScope   = cb->obj;
    vhpiHandleT hIter    = vhpi_iterator(vhpiParamDecls, hScope);

    while ((hParam = vhpi_scan(hIter)) && idx < args_size)
    {
        if (idx >= start_of_outputs)
        {
            DebugVPrint("setVhpiParams(): %s = %d\n", vhpi_get_str(vhpiNameP, hParam), args[idx]);
            value.format     = vhpiIntVal;
            value.bufSize    = 0;
            value.value.intg = args[idx];
            vhpi_put_value(hParam, &value, vhpiDeposit);
        }
        idx++;
    }
}

#endif

// If not VHDL FLI, VHDL VHPI or PLI TF, define VPI specific instructions
#if !defined (VPROC_VHDL) && !defined(VPROC_VHDL_VHPI) && defined(VPROC_PLI_VPI)

// -------------------------------------------------------------------------
// register_vpi_tasks()
//
// Registers the VProc system tasks for VPI
// -------------------------------------------------------------------------

static void register_vpi_tasks()
{
    s_vpi_systf_data data[] =
      {{vpiSysTask, 0, "$vinit",     VInit,     0, 0, 0},
       {vpiSysTask, 0, "$vsched",    VSched,    0, 0, 0},
       {vpiSysTask, 0, "$vaccess",   VAccess,   0, 0, 0},
       {vpiSysTask, 0, "$vprocuser", VProcUser, 0, 0, 0},
       {vpiSysTask, 0, "$virq",      VIrq,      0, 0, 0},
      };


    for (int idx= 0; idx < sizeof(data)/sizeof(s_vpi_systf_data); idx++)
    {
        debug_io_printf("registering %s\n", data[idx].tfname);
        vpi_register_systf(&data[idx]);
    }
}

// -------------------------------------------------------------------------
// Contains a zero-terminated list of functions that have
// to be called at startup
// -------------------------------------------------------------------------

void (*vlog_startup_routines[])() =
{
    register_vpi_tasks,
    0
};

// -------------------------------------------------------------------------
// getArgs()
//
// Get task arguments using VPI calls
// -------------------------------------------------------------------------

static int getArgs (vpiHandle taskHdl, int value[])
{
  int                  idx = 0;
  struct t_vpi_value   argval;
  vpiHandle            argh;

  vpiHandle            args_iter = vpi_iterate(vpiArgument, taskHdl);

  while (argh = vpi_scan(args_iter))
  {
    argval.format      = vpiIntVal;

    vpi_get_value(argh, &argval);
    value[idx]         = argval.value.integer;

    debug_io_printf("VPI routine received %x at offset %d\n", value[idx++], idx);
  }

  return idx;
}

// -------------------------------------------------------------------------
// updateArgs()
//
// Update task arguments using VPI calls
// -------------------------------------------------------------------------

static int updateArgs (vpiHandle taskHdl, int value[])
{
  int                 idx = 0;
  struct t_vpi_value  argval;
  vpiHandle           argh;

  vpiHandle           args_iter = vpi_iterate(vpiArgument, taskHdl);

  while (argh = vpi_scan(args_iter))
  {
    argval.format        = vpiIntVal;
    argval.value.integer = value[idx++];

    vpi_put_value(argh, &argval, NULL, vpiNoDelay);
  }

  return idx;
}

#endif

// =========================================================================
// Foreign procedure C functions
// =========================================================================

// -------------------------------------------------------------------------
// VInit()
//
// Main routine called whenever $vinit task invoked from
// initial block of VProc module.
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VInit (VINIT_PARAMS)
{
    int                args[ARGS_ARRAY_SIZE];

    //----------------------------------------------
    // Get node input argument
    //----------------------------------------------

#if !defined(VPROC_VHDL) && !defined(VPROC_SV)
    // Verilog
    int node;

# ifndef VPROC_PLI_VPI
    // PLI 1.0
    // Get single argument value of $vinit call
    node = tf_getp(VPNODENUM_ARG);
# else
    // VPI
    vpiHandle          taskHdl;

    // Obtain a handle to the argument list
    taskHdl            = vpi_handle(vpiSysTfCall, NULL);

    getArgs(taskHdl, &args[1]);

    // Get single argument value of $vinit call
    node = args[VPNODENUM_ARG];
# endif
#else
    // VHDL + VHPI
# ifdef VPROC_VHDL_VHPI
    int node;

    getVhpiParams(cb, &args[1], VINIT_NUM_ARGS);

    // Get single argument value of $vinit call
    node = args[VPNODENUM_ARG];
# endif
#endif

    // Range check node number
    if (node < 0 || node >= VP_MAX_NODES)
    {
        VPrint("***Error: VInit() got out of range node number (%d)\n", node);
        exit(VP_USER_ERR);
    }

    // Print message displaying node number, programming interface, and VProc version
    VPrint("VInit(%d): initialising %s interface\n  %s\n", node, PLI_STRING, VERSION_STRING);

    //----------------------------------------------
    // Initialise state
    //----------------------------------------------

    // Allocate some space for the node state and update pointer
    ns[node] = (pSchedState_t) calloc(1, sizeof(SchedState_t));

    // Set up semaphores for this node
    debug_io_printf("VInit(): initialising semaphores for node %d\n", node);

    if (sem_init(&(ns[node]->snd), 0, 0) == -1)
    {
        VPrint("***Error: VInit() failed to initialise semaphore\n");
        exit(1);
    }
    if (sem_init(&(ns[node]->rcv), 0, 0) == -1)
    {
        VPrint("***Error: VInit() failed to initialise semaphore\n");
        exit(1);
    }

    debug_io_printf("VInit(): initialising semaphores for node %d---Done\n", node);

    //----------------------------------------------
    // Issue a new thread to run the user code
    //----------------------------------------------

    VUser(node);

#if !defined(VPROC_VHDL) && !defined(VPROC_SV)
    return 0;
#endif
}

// -------------------------------------------------------------------------
// VSched()
//
// Main routine called whenever $vsched task invoked, on
// clock edge of scheduled cycle.
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VSched (VSCHED_PARAMS)
{
    int VPDataOut_int, VPAddr_int, VPRw_int, VPTicks_int;
    int args[ARGS_ARRAY_SIZE];

    //----------------------------------------------
    // Get input arguments
    //----------------------------------------------

#if !defined(VPROC_VHDL) && !defined(VPROC_SV)
    int node;
    int Interrupt, VPDataIn;
# ifndef VPROC_PLI_VPI
    // Get the input argument values of $vsched
    node         = tf_getp (VPNODENUM_ARG);
    Interrupt    = tf_getp (VPINTERRUPT_ARG);
    VPDataIn     = tf_getp (VPDATAIN_ARG);
# else
    vpiHandle    taskHdl;

    // Obtain a handle to the argument list
    taskHdl      = vpi_handle(vpiSysTfCall, NULL);

    getArgs(taskHdl, &args[1]);
# endif
#else
# ifdef VPROC_VHDL_VHPI
    int node;
    int Interrupt, VPDataIn;

    getVhpiParams(cb, &args[1], VSCHED_NUM_ARGS);
# endif
#endif

    // When VHDL with VHPI, or Verilog with VPI, extract input values from argument array
#if ( defined(VPROC_VHDL) &&  defined(VPROC_VHDL_VHPI)) || \
    (!defined(VPROC_VHDL) && !defined(VPROC_SV) && defined(VPROC_PLI_VPI))

    // Get argument value of $vsched call
    node         = args[VPNODENUM_ARG];
    Interrupt    = args[VPINTERRUPT_ARG];
    VPDataIn     = args[VPDATAIN_ARG];
#endif

    // Sample inputs and update node state
    ns[node]->rcv_buf.data_in   = VPDataIn;
    ns[node]->rcv_buf.interrupt = Interrupt;

    //----------------------------------------------
    // Discard any level interrupt when vectored
    // IRQ enabled
    //----------------------------------------------

    // If call to VSched is for interrupt and vector IRQ enabled (with C or Python callback registered)
    // don't process here with the level interrupt code and just return.
    if (Interrupt && (ns[node]->VUserIrqCB != NULL || ns[node]->PyIrqCB != NULL))
    {
#if !defined(VPROC_VHDL) && !defined(VPROC_SV)
        return 0;
#else
# ifndef VPROC_VHDL_VHPI
        // Since not processing make a delta cycle on return
        *VPTicks   = DELTA_CYCLE;
# endif
        return;
#endif
    }

    //----------------------------------------------
    // Send inputs to user thread
    //----------------------------------------------

    // Send message to VUser with VPDataIn value
    debug_io_printf("VSched(): setting rcv[%d] semaphore\n", node);
    sem_post(&(ns[node]->rcv));

    //----------------------------------------------
    // Get get updates from user thread
    //----------------------------------------------

    // Wait for a message from VUser process with output data
    debug_io_printf("VSched(): waiting for snd[%d] semaphore\n", node);
    sem_wait(&(ns[node]->snd));

    // Update outputs of $vsched task
    if (ns[node]->send_buf.ticks >= DELTA_CYCLE)
    {
        VPDataOut_int = ns[node]->send_buf.data_out;
        VPAddr_int    = ns[node]->send_buf.addr;
        VPRw_int      = ns[node]->send_buf.rw;
        VPTicks_int   = ns[node]->send_buf.ticks;
        debug_io_printf("VSched(): VPTicks=%08x\n", VPTicks_int);
    }

    debug_io_printf("VSched(): returning to simulation from node %d\n\n", node);

    //----------------------------------------------
    // Update outputs in simulation
    //----------------------------------------------

    // When VHDL with VHPI, or Verilog with VPI, use argument array
#if (defined(VPROC_VHDL)  &&  defined(VPROC_VHDL_VHPI)) || \
    (!defined(VPROC_VHDL) && !defined(VPROC_SV)  && defined(VPROC_PLI_VPI))
    args[VPDATAOUT_ARG] = VPDataOut_int;
    args[VPADDR_ARG]    = VPAddr_int;
    args[VPRW_ARG]      = VPRw_int;
    args[VPTICKS_ARG]   = VPTicks_int;
#endif

#if defined(VPROC_VHDL) || defined(VPROC_SV)
# ifndef VPROC_VHDL_VHPI
    // Export outputs directly to function arguments
    *VPDataOut          = VPDataOut_int;
    *VPAddr             = VPAddr_int;
    *VPRw               = VPRw_int;
    *VPTicks            = VPTicks_int;
# else
    // Update VHPI procedure outputs from argument array
    setVhpiParams(cb, &args[1], VPDATAOUT_ARG-1, VSCHED_NUM_ARGS);
# endif
#else
# ifndef VPROC_PLI_VPI
    // Update verilog PLI task ($vsched) output values with returned update data
    tf_putp (VPDATAOUT_ARG, VPDataOut_int);
    tf_putp (VPADDR_ARG,    VPAddr_int);
    tf_putp (VPRW_ARG,      VPRw_int);
    tf_putp (VPTICKS_ARG,   VPTicks_int);
# else
    // Update VPI task outputs from argument array
    updateArgs(taskHdl, &args[1]);
# endif
    return 0;
#endif

}

// -------------------------------------------------------------------------
// VProcUser()
//
// Calls a user registered function (if available) when
// $vprocuser(node, value) called in verilog
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VProcUser(VPROCUSER_PARAMS)
{
    int       args[ARGS_ARRAY_SIZE];

    // Get input argument values.

#if !defined(VPROC_VHDL) && !defined(VPROC_SV)

    int node, value;

# ifndef VPROC_PLI_VPI

    node      = tf_getp (VPNODENUM_ARG);
    value     = tf_getp (VPINTERRUPT_ARG);

# else
    vpiHandle taskHdl;

    // Obtain a handle to the argument list
    taskHdl   = vpi_handle(vpiSysTfCall, NULL);

    getArgs(taskHdl, &args[1]);

    // Get argument values of $vprocuser call
    node      = args[VPNODENUM_ARG];
    value     = args[VPINTERRUPT_ARG];

# endif
#else
# ifdef VPROC_VHDL_VHPI
    int       node, value;

    getVhpiParams(cb, &args[1], VPROCUSER_NUM_ARGS);

    // Get argument values of VProcUser VHPI call
    node      = args[VPNODENUM_ARG];
    value     = args[VPINTERRUPT_ARG];

# endif
#endif

    // Call any registered user callback function
    if (ns[node]->VUserCB != NULL)
    {
        (*(ns[node]->VUserCB))(value);
    }

#if !defined(VPROC_VHDL) && !defined(VPROC_SV)
    return 0;
#endif
}

// -------------------------------------------------------------------------
// VIrq()
//
// Calls a irq registered function (if available) when
// $virq(node, irq) called in verilog
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VIrq(VIRQ_PARAMS)
{
    int       args[ARGS_ARRAY_SIZE];

#if !defined(VPROC_VHDL) && !defined(VPROC_SV)

    int node, value;

# ifndef VPROC_PLI_VPI
    node      = tf_getp (VPNODENUM_ARG);
    value     = tf_getp (VPINTERRUPT_ARG);
# else
    vpiHandle taskHdl;

    // Obtain a handle to the argument list
    taskHdl   = vpi_handle(vpiSysTfCall, NULL);

    getArgs(taskHdl, &args[1]);

    // Get argument values of $vprocuser call
    node      = args[VPNODENUM_ARG];
    value     = args[VPINTERRUPT_ARG];
# endif
#else
# ifdef VPROC_VHDL_VHPI
    int       node, value;;

    getVhpiParams(cb, &args[1], VIRQ_NUM_ARGS);

    // Get argument values of VProcUser VHPI call
    node      = args[VPNODENUM_ARG];
    value     = args[VPINTERRUPT_ARG];
# endif
#endif

    // Call any registered callback function. VUserIrqCB and PyIrqCB are mutually exclusive.
    if (ns[node]->VUserIrqCB != NULL)
    {
        (*(ns[node]->VUserIrqCB))(value);
    }
    else if (ns[node]->PyIrqCB != NULL)
    {
        (*(ns[node]->PyIrqCB))(value, node);
    }

#if !defined(VPROC_VHDL) && !defined(VPROC_SV)
    return 0;
#endif
}

// -------------------------------------------------------------------------
// VAccess()
//
// Called on $vaccess PLI task. Exchanges block data between
// C and verilog domain
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VAccess(VACCESS_PARAMS)
{
    int       args[ARGS_ARRAY_SIZE];

#if defined(VPROC_VHDL) || defined(VPROC_SV)
# ifndef VPROC_VHDL_VHPI
    *VPDataOut                               = ((int *) ns[node]->send_buf.data_p)[idx];
    ((int *) ns[node]->send_buf.data_p)[idx] = VPDataIn;
# else
    int node, idx;

    getVhpiParams(cb, &args[1], VACCESS_NUM_ARGS);

    node      = args[VPNODENUM_ARG];
    idx       = args[VPINDEX_ARG];

    args[VPDATAOUT_ARG] = ((int *) ns[node]->send_buf.data_p)[idx];

    ((int *) ns[node]->send_buf.data_p)[idx] = args[VPDATAIN_ARG];

    setVhpiParams(cb, &args[1], VPDATAOUT_ARG-1, VACCESS_NUM_ARGS);
# endif
#else
   int node, idx;

# ifndef VPROC_PLI_VPI
    node      = tf_getp (VPNODENUM_ARG);
    idx       = tf_getp (VPINDEX_ARG);

    tf_putp (VPDATAOUT_ARG, ((int *) ns[node]->send_buf.data_p)[idx]);
    ((int *) ns[node]->send_buf.data_p)[idx] = tf_getp (VPDATAIN_ARG);

# else
    vpiHandle taskHdl;

    // Obtain a handle to the argument list
    taskHdl   = vpi_handle(vpiSysTfCall, NULL);

    getArgs(taskHdl, &args[1]);

    node      = args[VPNODENUM_ARG];
    idx       = args[VPINDEX_ARG];

    args[VPDATAOUT_ARG] = ((int *) ns[node]->send_buf.data_p)[idx];

    ((int *) ns[node]->send_buf.data_p)[idx] = args[VPDATAIN_ARG];

    updateArgs(taskHdl, &args[1]);
# endif

    return 0;
#endif
}

// -------------------------------------------------------------------------
// PyIrqCB()
//
// Python vectored irq callback function
// -------------------------------------------------------------------------

int PyIrqCB(int vec, int node)
{
    // Implements a ring buffer
    ns[node]->irqState.eventQueue[ns[node]->irqState.eventPtr & IRQ_QUEUE_INDEX_MASK] = vec;
    ns[node]->irqState.eventPtr = (ns[node]->irqState.eventPtr + 1) & IRQ_QUEUE_COUNT_MASK;

    return 0;
}

// -------------------------------------------------------------------------
// PyFetchIrq()
//
// Python IRQ state fetch function
// -------------------------------------------------------------------------

uint32_t PyFetchIrq (uint32_t *irq, const uint32_t node)
{
    uint32_t eventsInQueue = (ns[node]->irqState.eventPtr - ns[node]->irqState.eventPopPtr) & IRQ_QUEUE_COUNT_MASK;

    if (eventsInQueue)
    {
        // Get event from the beginning of the queue
        *irq = ns[node]->irqState.eventQueue[ns[node]->irqState.eventPopPtr & IRQ_QUEUE_INDEX_MASK];

        // Remove returned event from queue
        ns[node]->irqState.eventPopPtr = (ns[node]->irqState.eventPopPtr + 1) & IRQ_QUEUE_COUNT_MASK;;
    }

    return eventsInQueue;
}
