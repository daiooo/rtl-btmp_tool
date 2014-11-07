#define LOG_TAG "bt_mp_device_efuse_base"

#include "bt_syslog.h"
#include "bluetoothmp.h"
#include "bt_mp_device_efuse_base.h"

enum
{
    TYPE_15K = 0,   // 1.5k
    TYPE_10K,       // 10K
};

#define _DO_WRITE_   0
#define _DO_READ_    1

#define DEFAULT_WINDOWS_SIZE 4


int
BTDevice_Efuse_SetLdo225_10K15K(
        BT_DEVICE *pBtDevice,
        int ReadWriteMode,
        int R0
        )
{
    uint16_t data;

    if (bt_default_GetSysRegMaskBits(pBtDevice, 0x37, 7, 0, &data)!=BT_FUNCTION_SUCCESS)
        goto error;

    // LDO V = 2.25 v
    if (ReadWriteMode == _DO_WRITE_)
    {
        data = (data & 0x83) | 0x80;
    }
    else
    {
        data  &= 0x83;
    }

    if ( R0 == TYPE_15K )
        data = data | 0x74;    // 1.5k
    else
        data = data | 0x70;   // 10k

    if (bt_default_SetSysRegMaskBits(pBtDevice, 0x37, 7, 0, data) !=BT_FUNCTION_SUCCESS)
        goto error;


    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;
}

#define READ_WRITE_EFUSE_REG_MAX_LEN 32
int
BTDevice_Efuse_HCIIO_GetBytes(
        BT_DEVICE *pBtDevice,
        int Bank,
        unsigned int address,
        unsigned char *pDataBytes,
        unsigned int DataBytesLen
        )
{
    uint8_t pData[MAX_HCI_COMANND_BUF_SIZ];
    uint8_t pEvent[MAX_HCI_EVENT_BUF_SIZ];
    uint32_t EvtLen;

    unsigned int length;

    memset(pData,0,MAX_HCI_COMANND_BUF_SIZ);
    memset(pEvent,0,MAX_HCI_EVENT_BUF_SIZ);

    pData[0]=Bank;
    pData[1]=address&0xff;
    pData[2]=(address>>8)&0xff;
    pData[3]=DataBytesLen&0xff;
    pData[4]=(DataBytesLen>>8)&0xff;

    if (bt_default_SendHciCommandWithEvent(pBtDevice, 0xFC6C, LEN_5_BYTE, pData, 0x0e, pEvent, &EvtLen))
        goto error;

    //check event
    if (pEvent[5] != 0) {
        goto error;
    }

    length = (pEvent[7]<<8) | pEvent[6];
    if (DataBytesLen != length)
        goto error;

    memcpy(pDataBytes,&pEvent[8],length);

    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;
}



int
BTDevice_Efuse_HCIIO_SetBytes(
        BT_DEVICE *pBtDevice,
        int Bank,
        unsigned int address,
        unsigned char *pDataBytes,
        unsigned int DataBytesLen
        )
{
    uint8_t pData[MAX_HCI_COMANND_BUF_SIZ];
    uint8_t pEvent[MAX_HCI_EVENT_BUF_SIZ];
    unsigned char length;
    uint32_t EvtLen;

    memset(pData,0,MAX_HCI_COMANND_BUF_SIZ);
    memset(pEvent,0,MAX_HCI_EVENT_BUF_SIZ);

    pData[0]=Bank;
    pData[1]=address&0xff;
    pData[2]=(address>>8)&0xff;
    pData[3]=DataBytesLen&0xff;
    pData[4]=(DataBytesLen>>8)&0xff;
    memcpy(&pData[5],pDataBytes,DataBytesLen);
    length=LEN_5_BYTE + DataBytesLen;

    if (bt_default_SendHciCommandWithEvent(pBtDevice, 0xFC6B, length, pData, 0x0e, pEvent, &EvtLen))
        goto error;

    //check event
    if (pEvent[5] != 0)
        goto error;

    length = (pEvent[7]<<8) | pEvent[6];
    if (DataBytesLen != length)
        goto error;


    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;
}

int
BTDevice_Efuse_IO_SetBytes(
        BT_DEVICE *pBtDevice,
        int Bank,
        unsigned int address,
        unsigned char *pDataBytes,
        unsigned int DataBytesLen
        )
{

    uint16_t tmpdata;

    if (bt_default_GetSysRegMaskBits(pBtDevice, 0x35, 7, 0, &tmpdata)!=BT_FUNCTION_SUCCESS)
        goto error;

    tmpdata= (tmpdata&0xFC) | 0x08 |Bank ;

    if (bt_default_SetSysRegMaskBits(pBtDevice, 0x35, 7, 0, tmpdata) !=BT_FUNCTION_SUCCESS)
        goto error;

    if (BTDevice_Efuse_HCIIO_SetBytes(pBtDevice, Bank, address, pDataBytes, DataBytesLen)!=BT_FUNCTION_SUCCESS)
        goto error;

    if (bt_default_GetSysRegMaskBits(pBtDevice, 0x35, 7, 0, &tmpdata)!=BT_FUNCTION_SUCCESS)
        goto error;

    tmpdata = tmpdata & 0xF4;
    tmpdata |= Bank & 0x0f;

    if (bt_default_SetSysRegMaskBits(pBtDevice, 0x35, 7, 0, tmpdata) !=BT_FUNCTION_SUCCESS)
        goto error;

    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;

}



int
BTDevice_Efuse_IO_GetBytes(
        BT_DEVICE *pBtDevice,
        int Bank,
        unsigned int address,
        unsigned char *pDataBytes,
        unsigned int DataBytesLen
        )
{
    uint16_t tmpdata;

    if (BTDevice_Efuse_HCIIO_GetBytes(pBtDevice, Bank, address, pDataBytes, DataBytesLen)!=BT_FUNCTION_SUCCESS)
        goto error;

    if (bt_default_GetSysRegMaskBits(pBtDevice, 0x35, 7, 0, &tmpdata)!=BT_FUNCTION_SUCCESS)
        goto error;

    tmpdata = tmpdata & 0xF4;
    tmpdata |= Bank & 0x0f;

    if (bt_default_SetSysRegMaskBits(pBtDevice, 0x35, 7, 0, tmpdata) !=BT_FUNCTION_SUCCESS)
        goto error;

    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;

}



int
BTDevice_Efuse_GetBytes(
        BT_DEVICE *pBtDevice,
        int Bank,
        int RegStartAddr,
        unsigned char *pReadingBytes,
        unsigned int ByteNum
        )
{
    unsigned int i;

    unsigned int ReadingByteNum, ReadingByteNumRem;
    unsigned int RegReadingAddr;

    //unsigned char tmpdata;


    for(i = 0; i < ByteNum; i += READ_WRITE_EFUSE_REG_MAX_LEN)
    {
        // Set register reading address.
        RegReadingAddr = RegStartAddr + i;

        // Calculate remainder reading byte number.
        ReadingByteNumRem = ByteNum - i;

        // Determine reading byte number.
        ReadingByteNum = (ReadingByteNumRem > READ_WRITE_EFUSE_REG_MAX_LEN) ? READ_WRITE_EFUSE_REG_MAX_LEN : ReadingByteNumRem;

        if (BTDevice_Efuse_SetLdo225_10K15K(pBtDevice, _DO_READ_, TYPE_15K))	// 1.5K
            goto error;

        if (BTDevice_Efuse_IO_GetBytes(pBtDevice, Bank, RegReadingAddr, pReadingBytes+i, ReadingByteNum)!=BT_FUNCTION_SUCCESS)
            goto error;

    }


    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;

}



int
BTDevice_Efuse_SetBytes(
        BT_DEVICE *pBtDevice,
        int Bank,
        int RegStartAddr,
        unsigned char *pWritingBytes,
        unsigned int ByteNum
        )
{
    unsigned int i, j;

    unsigned int WritingByteNum, WritingByteNumRem;
    unsigned int RegWritingAddr;

    unsigned char pReadingBytes[READ_WRITE_EFUSE_REG_MAX_LEN];
    unsigned char times;

    for(i = 0; i < ByteNum; i += READ_WRITE_EFUSE_REG_MAX_LEN)
    {

        times =0;

        // Set register reading address.
        RegWritingAddr = RegStartAddr + i;

        // Calculate remainder reading byte number.
        WritingByteNumRem = ByteNum - i;

        // Determine reading byte number.
        WritingByteNum = (WritingByteNumRem > READ_WRITE_EFUSE_REG_MAX_LEN) ? READ_WRITE_EFUSE_REG_MAX_LEN : WritingByteNumRem;

retry:
        times++;
        if (times == 1 )
        {
            //set 1.5K
            if (BTDevice_Efuse_SetLdo225_10K15K(pBtDevice, _DO_WRITE_, TYPE_15K))	goto error;
        }
        else if (times ==2)
        {
            //set 10K
            if (BTDevice_Efuse_SetLdo225_10K15K(pBtDevice, _DO_WRITE_, TYPE_10K))	goto error;
        }
        else
        {
            goto error;
        }

        if (BTDevice_Efuse_IO_SetBytes(pBtDevice, Bank, RegWritingAddr, pWritingBytes+i, WritingByteNum)!=BT_FUNCTION_SUCCESS)
        {
            goto retry;
        }

        if (BTDevice_Efuse_IO_GetBytes(pBtDevice, Bank, RegWritingAddr, pReadingBytes, WritingByteNum)!=BT_FUNCTION_SUCCESS)
            goto error;

        for (j=0; j<WritingByteNum; j++)
        {
            if (pReadingBytes[j] != pWritingBytes[i+j])
            {
                goto retry;
            }
        }

    }

    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;

}



int
BuildEfuseLogicModule(
        BT_DEVICE *pBtDevice,
        EFUSE_MODULE **ppEfuseModule,
        EFUSE_MODULE *pEfuseModuleMemory,
        unsigned int EfuseLogSize,
        unsigned int EfusePhySize,
        unsigned char StartBank,
        unsigned char BankNum
        )
{
    EFUSE_MODULE *pEfuse;
    unsigned i;

    *ppEfuseModule = pEfuseModuleMemory;

    pEfuse = *ppEfuseModule;
    pEfuse->pBtDevice = pBtDevice;

    if (EfuseLogSize > MAX_EFUSE_LOG_LEN)
        goto error;

    if (EfusePhySize > MAX_EFUSE_PHY_LEN)
        goto error;

    pEfuse->EfuseLogSize = EfuseLogSize;
    pEfuse->EfusePhySize = EfusePhySize;
    pEfuse->StartBank = StartBank;
    pEfuse->BankNum = BankNum;

    for( i = 0 ; i < pEfuse->EfuseLogSize ; i++)
    {
        pEfuse->pEfuseLogMem[i].bMark = 0;
        pEfuse->pEfuseLogMem[i].Value = 0xff;
    }

    for( i = 0; i <MAX_EFUSE_BANK_NUM; i++)
        pEfuse->pEfusePhyDataLen[i] = 0;

    return BT_FUNCTION_SUCCESS;

error:
    return FUNCTION_ERROR;
}



int
BTDevice_Efuse_ReadData(
        EFUSE_MODULE *pEfuse
        )
{
    unsigned char Bank;
    unsigned char StartBank;
    unsigned char BankNum;

    StartBank = pEfuse->StartBank;
    BankNum = pEfuse->BankNum;

    for( Bank=StartBank; Bank<StartBank+BankNum; Bank++)
    {
        if (BTDevice_Efuse_GetBytes(pEfuse->pBtDevice, Bank, 0, pEfuse->pEfusePhyMem+Bank*MAX_EFUSE_PHY_LEN, pEfuse->EfusePhySize)!=BT_FUNCTION_SUCCESS)
            goto error;
    }

    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;

}



int
BTDevice_Efuse_WriteData(
        EFUSE_MODULE *pEfuse
        )
{
    unsigned int i, j;
    unsigned char Bank, StartBank;
    unsigned char BankNum;
    unsigned char pWritingEntry[LEN_10_BYTE];
    unsigned int WritingLen;


    StartBank = pEfuse->StartBank;
    BankNum = pEfuse->BankNum;

    for (i = 0; i < pEfuse->EfuseLogSize; i += LEN_8_BYTE)
    {
        if (BTDevice_Efuse_LogicDataToWritingEntry(pEfuse, i, pWritingEntry, &WritingLen)!=BT_FUNCTION_SUCCESS)
            goto error;

        if (WritingLen > 0)
        {
            Bank = StartBank;
re_check:
            if ((pEfuse->pEfusePhyDataLen[Bank]+WritingLen) > (pEfuse->EfusePhySize-DUMMY_EFUSE_LEN) )
            {
                Bank++;

                if (Bank>StartBank+BankNum)
                    goto error;
                else
                    goto re_check;
            }

            if (BTDevice_Efuse_SetBytes(pEfuse->pBtDevice, Bank, pEfuse->pEfusePhyDataLen[Bank], pWritingEntry, WritingLen)!=BT_FUNCTION_SUCCESS)
                goto error;

            for( j = i ; j < i + LEN_8_BYTE ; j++)
            {
                pEfuse->pEfuseLogMem[j].bMark = 0;
            }

            pEfuse->pEfusePhyDataLen[Bank] +=WritingLen;

        }
    }

    return BT_FUNCTION_SUCCESS;

error:

    return FUNCTION_ERROR;
}




int
BTDevice_Efuse_PhysicalToLogicalData(
        EFUSE_MODULE *pEfuse
        )
{
    unsigned int i, j;
    unsigned char Bank;
    unsigned char Header1;
    unsigned char Header2;
    unsigned int BaseAddress, BaseAddress_x, BaseAddress_y;
    unsigned char WordSelect;

    for( i = 0 ; i < pEfuse->EfuseLogSize ; i++)
    {
        pEfuse->pEfuseLogMem[i].bMark = 0;
        pEfuse->pEfuseLogMem[i].Value = 0xff;
    }

    for( Bank = pEfuse->StartBank; Bank < pEfuse->StartBank+pEfuse->BankNum; Bank++)
    {
        i = 0;

        while(i<pEfuse->EfusePhySize-DUMMY_EFUSE_LEN)
        {
            if ((pEfuse->pEfusePhyMem[Bank*MAX_EFUSE_PHY_LEN+i]==0xff) &&
                    (pEfuse->pEfusePhyMem[Bank*MAX_EFUSE_PHY_LEN+i+1]==0xff))
            {
                pEfuse->pEfusePhyDataLen[Bank] = i;
                break;
            }

            Header1 = pEfuse->pEfusePhyMem[Bank*MAX_EFUSE_PHY_LEN+i];
            i++;

            if ((Header1&0x0f)!=0x0f)
            {
                // 1 byte mode
                BaseAddress = ((Header1>>4)&0x0f)*8;
                WordSelect =  Header1&0x0f;
            }
            else
            {
                // 2 byte mode
                Header2 = pEfuse->pEfusePhyMem[Bank*MAX_EFUSE_PHY_LEN+i];
                i++;
                BaseAddress_x = ((Header1>>4)&0x0f);
                BaseAddress_y = ((Header2>>4)&0x0f);
                BaseAddress = ((BaseAddress_x/2)+((BaseAddress_y-2)*8)+16)*8;
                WordSelect = Header2&0x0f;
            }

            for ( j=0; j<LEN_4_BYTE; j++ )
            {
                if (((WordSelect>>j)&0x01)==0x00)
                {
                    pEfuse->pEfuseLogMem[BaseAddress + (j*2)].Value = pEfuse->pEfusePhyMem[Bank*MAX_EFUSE_PHY_LEN+i];
                    i++;
                    pEfuse->pEfuseLogMem[BaseAddress + (j*2)+1].Value = pEfuse->pEfusePhyMem[Bank*MAX_EFUSE_PHY_LEN+i];
                    i++;
                }
            }

        }
    }

    return BT_FUNCTION_SUCCESS;


}



int
BTDevice_Efuse_SetValueToLogicalData(
        EFUSE_MODULE *pEfuse,
        unsigned int Addr,
        unsigned char Value
        )
{
    if (pEfuse->pEfuseLogMem[Addr].Value != Value)
    {
        pEfuse->pEfuseLogMem[Addr].Value = Value;
        pEfuse->pEfuseLogMem[Addr].bMark = 1;
    }

    return BT_FUNCTION_SUCCESS;
}



int
BTDevice_Efuse_LogicDataToWritingEntry(
        EFUSE_MODULE *pEfuse,
        unsigned int StartLogAddr,
        unsigned char *pWritingEntry,
        unsigned int *Len
        )
{
    int BaseAddress, BaseAddress_x, BaseAddress_y, BaseAddress_A;
    unsigned char Header1, Header2, WordSelect;
    unsigned tmp[LEN_10_BYTE];
    unsigned int j, count;

    count=0;
    WordSelect=0xff;
    *Len = 0;

    for ( j=0; j<LEN_4_BYTE; j++)
    {
        if ((pEfuse->pEfuseLogMem[StartLogAddr+j*2].bMark == 1) || (pEfuse->pEfuseLogMem[StartLogAddr+j*2+1].bMark == 1))
        {
            WordSelect=WordSelect & ~(0x01 << (j));
            tmp[count] = pEfuse->pEfuseLogMem[StartLogAddr+j*2].Value;
            tmp[count+1] = pEfuse->pEfuseLogMem[StartLogAddr+j*2+1].Value;
            count+=2;
        }
    }

    if ( (WordSelect&0x0f) == 0x0f)
    {
        return BT_FUNCTION_SUCCESS;
    }

    if (StartLogAddr < 128)
    {
        // 1 byte mode
        BaseAddress=StartLogAddr/8;
        Header1= (BaseAddress&0x0f)<<4;
        Header1 |= WordSelect;
        pWritingEntry[0] = Header1;
        memcpy(pWritingEntry+1, tmp, count);
        *Len = count+1;
    }
    else
    {
        // 2 byte mode
        BaseAddress_A= (StartLogAddr/8)-16;
        BaseAddress_x=  (BaseAddress_A%8)*2;
        BaseAddress_y=  (BaseAddress_A/8)+2;

        Header1=  (BaseAddress_x&0x0F)<<4 | 0x0F;
        Header2=  (BaseAddress_y&0x0F)<<4;
        Header2 |= WordSelect;
        pWritingEntry[0] = Header1;
        pWritingEntry[1] = Header2;
        memcpy(pWritingEntry+2, tmp, count);
        *Len = count+2;
    }

    return BT_FUNCTION_SUCCESS;
}