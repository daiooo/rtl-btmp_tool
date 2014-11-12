#define LOG_TAG "btif_mp_build"

#include "bluetoothmp.h"
#include "bt_mp_device_efuse_base.h"
#include "bt_mp_build.h"

int
BuildBluetoothDevice(
        BASE_INTERFACE_MODULE *pBaseInterface,
        BT_DEVICE             **ppBtDeviceBase,
        BT_DEVICE             *pDeviceBasememory
        )
{
    BT_DEVICE *pBtDevice = pDeviceBasememory;
    *ppBtDeviceBase = pDeviceBasememory;

    pBtDevice->InterfaceType = pBaseInterface->InterfaceType;
    pBtDevice->pBaseInterface = pBaseInterface;

    pBtDevice->pBTInfo = &pBtDevice->BaseBTInfoMemory ;

    pBtDevice->SetTxGainTable = BTDevice_SetTxGainTable;
    pBtDevice->SetTxDACTable  = BTDevice_SetTxDACTable;
    // Register Read/Write
    pBtDevice->SetMdRegMaskBits = BTDevice_SetMDRegMaskBits;
    pBtDevice->GetMdRegMaskBits = BTDevice_GetMDRegMaskBits;
    pBtDevice->SetRfRegMaskBits = BTDevice_SetRFRegMaskBits;;
    pBtDevice->GetRfRegMaskBits = BTDevice_GetRFRegMaskBits;
    pBtDevice->SetSysRegMaskBits = BTDevice_SetSysRegMaskBits;
    pBtDevice->GetSysRegMaskBits = BTDevice_GetSysRegMaskBits;
    pBtDevice->SetBBRegMaskBits = BTDevice_SetBBRegMaskBits;
    pBtDevice->GetBBRegMaskBits = BTDevice_GetBBRegMaskBits;
    // HCI Command & Event
    pBtDevice->SendHciCommandWithEvent = BTDevice_SendHciCommandWithEvent;
    pBtDevice->RecvAnyHciEvent = BTDevice_RecvAnyHciEvent;
    // Register Control
    pBtDevice->SetPowerGainIndex = BTDevice_SetPowerGainIndex;
    pBtDevice->SetPowerGain = BTDevice_SetPowerGain;
    pBtDevice->SetPowerDac = BTDevice_SetPowerDac;
    pBtDevice->SetRestMDCount = BTDevice_SetResetMDCount;

    pBtDevice->TestModeEnable = BTDevice_TestModeEnable;
    pBtDevice->SetRtl8761Xtal = BTDevice_SetRTL8761Xtal;
    pBtDevice->GetRtl8761Xtal = BTDevice_GetRTL8761Xtal;
    pBtDevice->ReadThermal = BTDevice_ReadThermal;

    pBtDevice->GetStage = BTDevice_GetStage;
    // Vendor HCI Command Control
    pBtDevice->SetHoppingMode        =      BTDevice_SetHoppingMode;
    pBtDevice->SetHciReset           =      BTDevice_SetHciReset;

    pBtDevice->TxTriggerPktCnt = 0;

    //CON-TX
    pBtDevice->SetContinueTxBegin   =       BTDevice_SetContinueTxBegin;
    pBtDevice->SetContinueTxStop    =       BTDevice_SetContinueTxStop;
    pBtDevice->SetContinueTxUpdate  =       BTDevice_SetContinueTxUpdate;
    //LE Test
    pBtDevice->LeTxTestCmd          =       BTDevice_LeTxTestCmd;
    pBtDevice->LeRxTestCmd          =       BTDevice_LeRxTestCmd;
    pBtDevice->LeTestEndCmd         =       BTDevice_LeTestEndCmd;
    //PKT-TX
    pBtDevice->SetPktTxBegin        =       BTDevice_SetPktTxBegin;
    pBtDevice->SetPktTxStop         =       BTDevice_SetPktTxStop;
    pBtDevice->SetPktTxUpdate       =       BTDevice_SetPktTxUpdate;
    //PKT-RX
    pBtDevice->SetPktRxBegin        =       BTDevice_SetPktRxBegin;
    pBtDevice->SetPktRxStop         =       BTDevice_SetPktRxStop;
    pBtDevice->SetPktRxUpdate       =       BTDevice_SetPktRxUpdate;
    //Base Function
    pBtDevice->GetChipVersionInfo   =       BTDevice_GetBTChipVersionInfo;
    pBtDevice->BTDlFW               =       BTDevice_BTDlFW;
    pBtDevice->BTDlMERGERFW         =       BTDevice_BTDlMergerFW;
    //PG Efuse
    pBtDevice->BT_PGEfuseRawData    =       BTDevice_PGEfuseRawData;
    pBtDevice->BT_ReadEfuseLogicalData = BTDevice_ReadEfuseLogicalData;

    BuildEfuseLogicUnit(pBtDevice, &(pBtDevice->pSysEfuse), &(pBtDevice->SysEfuseMemory), 128, 128, 0, 1);
    BuildEfuseLogicUnit(pBtDevice, &(pBtDevice->pBtEfuse), &(pBtDevice->BtEfuseMemory), 1024, 512, 1, 2);

    return 0;
}

// Base Module interface builder
int
BuildBluetoothModule(
        BASE_INTERFACE_MODULE *pBaseInterfaceModule,
        BT_MODULE             *pBtModule
        )
{
    int rtn = BT_FUNCTION_SUCCESS;

    pBtModule->pBtParam             =       &pBtModule->BaseBtParamMemory;
    pBtModule->pBtDevice            =       &pBtModule->BaseBtDeviceMemory;
    pBtModule->pModuleBtReport      =       &pBtModule->BaseModuleBtReportMemory;

    pBtModule->UpDataParameter      =       BTModule_UpDataParameter;
    pBtModule->ActionControlExcute  =       BTModule_ActionControlExcute;
    pBtModule->ActionReport         =       BTModule_ActionReport ;
    pBtModule->DownloadPatchCode    =       BTModule_DownloadPatchCode;

    pBtModule->SetRfRegMaskBits     =       BTModule_SetRFRegMaskBits;
    pBtModule->GetRfRegMaskBits     =       BTModule_GetRFRegMaskBits;
    pBtModule->SetMdRegMaskBits     =       BTModule_SetMDRegMaskBits;
    pBtModule->GetMdRegMaskBits     =       BTModule_GetMDRegMaskBits;

    pBtModule->SendHciCommandWithEvent  =   BTModule_SendHciCommandWithEvent;
    pBtModule->RecvAnyHciEvent      =       BTModule_RecvAnyHciEvent;

    pBtModule->SetSysRegMaskBits    =       BTModule_SetSysRegMaskBits;
    pBtModule->GetSysRegMaskBits    =       BTModule_GetSysRegMaskBits;
    pBtModule->SetBBRegMaskBits     =       BTModule_SetBBRegMaskBits;
    pBtModule->GetBBRegMaskBits     =       BTModule_GetBBRegMaskBits;
    pBtModule->SetRegMaskBits       =       BTModule_SetRegMaskBits;
    pBtModule->GetRegMaskBits       =       BTModule_GetRegMaskBits;

    BuildBluetoothDevice(pBaseInterfaceModule,
            &pBtModule->pBtDevice,
            &pBtModule->BaseBtDeviceMemory
            );

    return  rtn;
}
