/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/************************************************************************************
 *
 *  Filename:      btif_core.c
 *
 *  Description:   Contains core functionality related to interfacing between
 *                 Bluetooth HAL and BTE core stack.
 *
 ***********************************************************************************/

#define LOG_TAG "btif_core"

#include <stdlib.h>
#include <bluetoothmp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

#include "btif_api.h"

#include "gki.h"
#include "btu.h"
#include "hcidefs.h"
#include "hcimsgs.h"
#include "btif_common.h"
#include "bt_syslog.h"
#include "foundation.h"
#include "bt_mp_transport.h"

/* trace level */
/* TODO Bluedroid - Hard-coded trace levels -  Needs to be configurable */
UINT8 appl_trace_level = BT_TRACE_LEVEL_VERBOSE; //APPL_INITIAL_TRACE_LEVEL;
UINT8 btif_trace_level = BT_TRACE_LEVEL_VERBOSE;


/************************************************************************************
**  Constants & Macros
************************************************************************************/

#ifndef BTIF_TASK_STACK_SIZE
#define BTIF_TASK_STACK_SIZE       0x2000         /* In bytes */
#endif



#define BTIF_TASK_STR        ((INT8 *) "BTIF")

/************************************************************************************
**  Local type definitions
************************************************************************************/



typedef enum {
    BTIF_CORE_STATE_DISABLED = 0,
    BTIF_CORE_STATE_ENABLING,
    BTIF_CORE_STATE_ENABLED,
    BTIF_CORE_STATE_DISABLING
} btif_core_state_t;

/************************************************************************************
**  Static variables
************************************************************************************/

bt_bdaddr_t btif_local_bd_addr;

static UINT32 btif_task_stack[(BTIF_TASK_STACK_SIZE + 3) / 4];

/* holds main adapter state */
static btif_core_state_t btif_core_state = BTIF_CORE_STATE_DISABLED;

static int btif_shutdown_pending = 0;

/************************************************************************************
**  Static functions
************************************************************************************/
static bt_status_t btif_associate_evt(void);
static bt_status_t btif_disassociate_evt(void);

/* sends message to btif task */
static void btif_sendmsg(void *p_msg);

/************************************************************************************
**  Externs
************************************************************************************/
extern bt_callbacks_t *bt_hal_cbacks ;
extern BASE_INTERFACE_MODULE   BaseInterfaceModuleMemory;


static bt_status_t btif_mp_test_evt(void* msg);
static bt_status_t btif_mp_notify_evt(void* msg);



/** TODO: Move these to _common.h */
void bte_main_boot_entry(void);
void bte_main_enable(uint8_t *local_addr, bt_hci_if_t hci_if, const char *dev_node);
void bte_main_disable(void);
void bte_main_shutdown(void);
void bte_main_postload_cfg(void);

/************************************************************************************
**  Functions
************************************************************************************/

void btsnd_hcic_mp_test_cmd (void *buffer, UINT16 opcode, UINT8 len, UINT8 *p_data, void *p_cmd_cplt_cback);


/*****************************************************************************
**   Context switching functions
*****************************************************************************/


/*******************************************************************************
**
** Function         btif_context_switched
**
** Description      Callback used to execute transferred context callback
**
**                  p_msg : message to be executed in btif context
**
** Returns          void
**
*******************************************************************************/

static void btif_context_switched(void *p_msg)
{
    tBTIF_CONTEXT_SWITCH_CBACK *p;

    SYSLOGI("btif_context_switched");

    p = (tBTIF_CONTEXT_SWITCH_CBACK *) p_msg;

    /* each callback knows how to parse the data */
    if (p->p_cb)
        p->p_cb(p->event, p->p_param);
}


/*******************************************************************************
**
** Function         btif_transfer_context
**
** Description      This function switches context to btif task
**
**                  p_cback   : callback used to process message in btif context
**                  event     : event id of message
**                  p_params  : parameter area passed to callback (copied)
**                  param_len : length of parameter area
**                  p_copy_cback : If set this function will be invoked for deep copy
**
** Returns          void
**
*******************************************************************************/

bt_status_t btif_transfer_context (tBTIF_CBACK *p_cback, UINT16 event, char* p_params, int param_len, tBTIF_COPY_CBACK *p_copy_cback)
{
    tBTIF_CONTEXT_SWITCH_CBACK *p_msg;

    SYSLOGI("btif_transfer_context event %d, len %d", event, param_len);

    /* allocate and send message that will be executed in btif context */
    if ((p_msg = (tBTIF_CONTEXT_SWITCH_CBACK *) GKI_getbuf(sizeof(tBTIF_CONTEXT_SWITCH_CBACK) + param_len)) != NULL)
    {
        p_msg->hdr.event = BT_EVT_CONTEXT_SWITCH_EVT; /* internal event */
        p_msg->p_cb = p_cback;

        p_msg->event = event;                         /* callback event */

        /* check if caller has provided a copy callback to do the deep copy */
        if (p_copy_cback)
        {
            p_copy_cback(event, p_msg->p_param, p_params);
        }
        else if (p_params)
        {
            memcpy(p_msg->p_param, p_params, param_len);  /* callback parameter data */
        }

        btif_sendmsg(p_msg);
        return BT_STATUS_SUCCESS;
    }
    else
    {
        /* let caller deal with a failed allocation */
        return BT_STATUS_NOMEM;
    }
}

/*******************************************************************************
**
** Function         btif_is_enabled
**
** Description      checks if main adapter is fully enabled
**
** Returns          1 if fully enabled, otherwize 0
**
*******************************************************************************/

int btif_is_enabled(void)
{
    return (btif_core_state == BTIF_CORE_STATE_ENABLED);
}

static void btif_mp_rx_data_ind(uint8_t evtcode, uint8_t *buf, uint8_t len)
{
    int i;
    uint8_t *pEvtBuf = BaseInterfaceModuleMemory.evtBuffer;

    BaseInterfaceModuleMemory.evtLen = sizeof(evtcode) + sizeof(len) + len;

    SYSLOGI("<-- HCI EVENT event code: 0x%x %d", evtcode, len);

/*
    for( i = 0 ; i < len; i++ )
    {
        SYSLOGI("0x%x ",buf[i]);
    }
*/

    UINT8_TO_STREAM(pEvtBuf, evtcode);
    UINT8_TO_STREAM(pEvtBuf, len);

    memcpy(pEvtBuf, buf, len);

    bt_transport_signal_event(&BaseInterfaceModuleMemory, MP_TRANSPORT_EVENT_RX_HCIEVT);
}


static bt_status_t btif_mp_test_evt(void* msg)
{
    BT_HDR *p_msg = (BT_HDR *)msg;
    UINT8   *p = (UINT8 *)(p_msg + 1) + p_msg->offset;
    UINT8   hci_evt_code;
    UINT8   hci_evt_len;

    STREAM_TO_UINT8  (hci_evt_code, p);
    STREAM_TO_UINT8  (hci_evt_len, p);

    SYSLOGI("%s: evtcode[0x%02x]", __FUNCTION__, hci_evt_code);

    btif_mp_rx_data_ind(hci_evt_code, (uint8_t*)p, hci_evt_len);

    return BT_STATUS_SUCCESS;
}

static bt_status_t btif_mp_notify_evt(void* msg)
{
    BT_HDR *p_msg = (BT_HDR *)msg;
    char   *p = (char *)(p_msg + 1);
    UINT8 opcode;
    UINT16 param_len;

    opcode = *((uint8_t *)p);
    param_len = *((uint8_t *)p + 1) + *((uint8_t *)p + 2) * 0x100;
    p += 3;

    p[param_len] = '\0'; /* must allocate extra 1 byte */

    SYSLOGI("%s: opcode[0x%02x], params[%u]", __FUNCTION__, opcode, param_len);

    HAL_CBACK(bt_hal_cbacks, dut_mode_recv_cb, opcode, p);

    return BT_STATUS_SUCCESS;
}



/*******************************************************************************
**
** Function         btif_task
**
** Description      BTIF task handler managing all messages being passed
**                  Bluetooth HAL and BTA.
**
** Returns          void
**
*******************************************************************************/

static void btif_task(UINT32 params)
{
    UINT16   event;
    BT_HDR   *p_msg;

    SYSLOGI("btif task starting");

    btif_associate_evt();

    for(;;)
    {
        /* wait for specified events */
        event = GKI_wait(0xFFFF, 0);

        /*
         * Wait for the trigger to init chip and stack. This trigger will
         * be received by btu_task once the UART is opened and ready
         */
        if (event == BT_EVT_TRIGGER_STACK_INIT)
        {
            BTIF_TRACE_DEBUG0("btif_task: received trigger stack init event");
            btif_enable_bluetooth_evt(BT_STATE_ON);
        }

        /*
         * Failed to initialize controller hardware, reset state and bring
         * down all threads
         */
        if (event == BT_EVT_HARDWARE_INIT_FAIL)
        {
            BTIF_TRACE_DEBUG0("btif_task: hardware init failed");
            bte_main_disable();
            GKI_task_self_cleanup(BTIF_TASK);
            bte_main_shutdown();
            btif_core_state = BTIF_CORE_STATE_DISABLED;
            HAL_CBACK(bt_hal_cbacks,adapter_state_changed_cb,BT_STATE_OFF);
            break;
        }

        if (event & EVENT_MASK(GKI_SHUTDOWN_EVT))
            break;

        if (event & TASK_MBOX_1_EVT_MASK)
        {
            while((p_msg = GKI_read_mbox(BTU_BTIF_MBOX)) != NULL)
            {
                SYSLOGD("btif task fetched event 0x%x", p_msg->event);

                switch (p_msg->event)
                {
                    case BT_EVT_CONTEXT_SWITCH_EVT:
                        btif_context_switched(p_msg);
                        break;

                    case BT_EVT_RX:
                        btif_mp_test_evt(p_msg);
                        break;

                    case BT_EVT_MP_NOTIFY_BTIF:
                        btif_mp_notify_evt(p_msg);
                        break;

                    default:
                        SYSLOGE("unhandled btif event (%d)", p_msg->event & BT_EVT_MASK);
                        break;
                }

                GKI_freebuf(p_msg);
            }
        }
    }

    btif_disassociate_evt();

    SYSLOGI("btif task exiting");
}


/*******************************************************************************
**
** Function         btif_sendmsg
**
** Description      Sends msg to BTIF task
**
** Returns          void
**
*******************************************************************************/

void btif_sendmsg(void *p_msg)
{
    GKI_send_msg(BTIF_TASK, BTU_BTIF_MBOX, p_msg);
}



/*****************************************************************************
**
**   btif core api functions
**
*****************************************************************************/

/*******************************************************************************
**
** Function         btif_init_bluetooth
**
** Description      Creates BTIF task and prepares BT scheduler for startup
**
** Returns          bt_status_t
**
*******************************************************************************/

bt_status_t btif_init_bluetooth()
{
    UINT8 status;

    bte_main_boot_entry();

    /* start btif task */
    status = GKI_create_task(btif_task, BTIF_TASK, BTIF_TASK_STR,
                (UINT16 *) ((UINT8 *)btif_task_stack + BTIF_TASK_STACK_SIZE),
                sizeof(btif_task_stack));

    if (status != GKI_SUCCESS)
        return BT_STATUS_FAIL;

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_associate_evt
**
** Description      Event indicating btif_task is up
**                  Attach btif_task to JVM
**
** Returns          void
**
*******************************************************************************/

static bt_status_t btif_associate_evt(void)
{
    SYSLOGI("%s: notify ASSOCIATE_JVM", __FUNCTION__);
    HAL_CBACK(bt_hal_cbacks, thread_evt_cb, ASSOCIATE_JVM);

    return BT_STATUS_SUCCESS;
}


/*******************************************************************************
**
** Function         btif_enable_bluetooth
**
** Description      Performs chip power on and kickstarts OS scheduler
**
** Returns          bt_status_t
**
*******************************************************************************/

bt_status_t btif_enable_bluetooth(bt_hci_if_t hci_if, const char *dev_node)
{
    SYSLOGI("BTIF ENABLE BLUETOOTH");

    if (btif_core_state != BTIF_CORE_STATE_DISABLED)
    {
        SYSLOGW("not disabled");
        return BT_STATUS_DONE;
    }

    btif_core_state = BTIF_CORE_STATE_ENABLING;

    /* Create the GKI tasks and run them */
    bte_main_enable(btif_local_bd_addr.address, hci_if, dev_node);

    return BT_STATUS_SUCCESS;
}


/*******************************************************************************
**
** Function         btif_enable_bluetooth_evt
**
** Description      Event indicating bluetooth enable is completed
**                  Notifies HAL user with updated adapter state
**
** Returns          void
**
*******************************************************************************/

void btif_enable_bluetooth_evt(tBTA_STATUS status)
{
    if(status == BT_STATE_ON)
        btif_core_state = BTIF_CORE_STATE_ENABLED;
    else
        btif_core_state = BTIF_CORE_STATE_DISABLED;

    HAL_CBACK(bt_hal_cbacks, adapter_state_changed_cb, status);
}

/*******************************************************************************
**
** Function         btif_disable_bluetooth
**
** Description      Inititates shutdown of Bluetooth system.
**                  Any active links will be dropped and device entering
**                  non connectable/discoverable mode
**
** Returns          void
**
*******************************************************************************/
bt_status_t btif_disable_bluetooth(void)
{
    if (!btif_is_enabled())
    {
        SYSLOGI("btif_disable_bluetooth : not yet enabled");
        return BT_STATUS_NOT_READY;
    }

    bte_main_disable();

    btif_core_state = BTIF_CORE_STATE_DISABLED;

    HAL_CBACK(bt_hal_cbacks, adapter_state_changed_cb, BT_STATE_OFF);

    GKI_destroy_task(BTIF_TASK);

    bte_main_shutdown();


    SYSLOGI("BTIF DISABLE BLUETOOTH");

    return BT_STATUS_SUCCESS;
}



/*******************************************************************************
**
** Function         btif_shutdown_bluetooth
**
** Description      Finalizes BT scheduler shutdown and terminates BTIF
**                  task.
**
** Returns          void
**
*******************************************************************************/

bt_status_t btif_shutdown_bluetooth(void)
{
    SYSLOGI("%s", __FUNCTION__);

    if (btif_is_enabled())
    {
        SYSLOGI("shutdown while still enabled, initiate disable");

        /* shutdown called prior to disabling, initiate disable */
        btif_disable_bluetooth();
        btif_shutdown_pending = 1;
        return BT_STATUS_NOT_READY;
    }

    btif_shutdown_pending = 0;

    GKI_destroy_task(BTIF_TASK);

    bte_main_shutdown();

    SYSLOGI("%s done", __FUNCTION__);

    return BT_STATUS_SUCCESS;
}


/*******************************************************************************
**
** Function         btif_disassociate_evt
**
** Description      Event indicating btif_task is going down
**                  Detach btif_task to JVM
**
** Returns          void
**
*******************************************************************************/

static bt_status_t btif_disassociate_evt(void)
{
     SYSLOGI("%s: notify DISASSOCIATE_JVM", __FUNCTION__);

    HAL_CBACK(bt_hal_cbacks, thread_evt_cb, DISASSOCIATE_JVM);

    /* shutdown complete, all events notified and we reset HAL callbacks */
    bt_hal_cbacks = NULL;

    return BT_STATUS_SUCCESS;
}

void BTM_VendorSpecificCommand(UINT16 opcode, UINT8 param_len,
                                      UINT8 *p_param_buf)
{
    void *p_buf;

    /* Allocate a buffer to hold HCI command plus the callback function */
    if ((p_buf = GKI_getbuf((UINT16)(sizeof(BT_HDR) + sizeof (void *) +
                            param_len + HCIC_PREAMBLE_SIZE))) != NULL)
    {

        btsnd_hcic_mp_test_cmd (p_buf, opcode, param_len, p_param_buf, (void *)NULL);
    }
    else
    {
        SYSLOGI("%s", __FUNCTION__);
    }

}

/*******************************************************************************
**
** Function         btif_dut_mode_send
**
** Description     Sends a HCI Vendor specific command to the controller
**
** Returns          BT_STATUS_SUCCESS on success
**
*******************************************************************************/
bt_status_t btif_dut_mode_send(uint16_t opcode, uint8_t *buf, uint8_t len)
{
    /* TODO: Check that opcode is a vendor command group */
     SYSLOGI("%s", __FUNCTION__);

    BTM_VendorSpecificCommand(opcode, len, buf);

    return BT_STATUS_SUCCESS;
}
