/**
********************************************************************************
\file   pcpserial-sl.c

\brief  Implements the driver for the serial device in slave mode

Defines the platform specific functions for the serial to interconnect the app
with the POWERLINK processor. (Target is the stm32f103rb board)

*******************************************************************************/

/*------------------------------------------------------------------------------
* License Agreement
*
* Copyright 2013 BERNECKER + RAINER, AUSTRIA, 5142 EGGELSBERG, B&R STRASSE 1
* All rights reserved.
*
* Redistribution and use in source and binary forms,
* with or without modification,
* are permitted provided that the following conditions are met:
*
*   * Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer
*     in the documentation and/or other materials provided with the
*     distribution.
*   * Neither the name of the B&R nor the names of its contributors
*     may be used to endorse or promote products derived from this software
*     without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* includes                                                                   */
/*----------------------------------------------------------------------------*/
#include <common/pcpserial.h>

#include <stm32f10x_spi.h>
#include <stm32f10x_dma.h>
#include <stm32f10x_gpio.h>

/*============================================================================*/
/*            G L O B A L   D E F I N I T I O N S                             */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* const defines                                                              */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* module global vars                                                         */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* global function prototypes                                                 */
/*----------------------------------------------------------------------------*/

/*============================================================================*/
/*            P R I V A T E   D E F I N I T I O N S                           */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* const defines                                                              */
/*----------------------------------------------------------------------------*/

/* Defines for the SPI peripheral */
#define SPIx                 SPI1
#define SPIx_RCC_PERIPH      RCC_APB2Periph_SPI1

/* Definition for SPIx Pins */
#define SPIx_SSN_PIN                     GPIO_Pin_4
#define SPIx_SSN_GPIO_PORT               GPIOA
#define SPIx_SCK_PIN                     GPIO_Pin_5
#define SPIx_SCK_GPIO_PORT               GPIOA
#define SPIx_MOSI_PIN                    GPIO_Pin_7
#define SPIx_MOSI_GPIO_PORT              GPIOA

/* Defines for the DMA channels */
#define DMAx_CHANNEL_RX      DMA1_Channel2
#define DMAx_ChannelRx_IRQn  DMA1_Channel2_IRQn
#define DMAx_ChannelRx_IRQHandler    DMA1_Channel2_IRQHandler
#define DMAx_RCC_PERIPH      RCC_AHBPeriph_DMA1

#define DMAx_FLAG_TE2        DMA1_FLAG_TE2
#define DMAx_FLAG_TC2        DMA1_FLAG_TC2
#define DMAx_IT_TE2          DMA1_IT_TE2
#define DMAx_IT_TC2          DMA1_IT_TC2

/*----------------------------------------------------------------------------*/
/* local types                                                                */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* local vars                                                                 */
/*----------------------------------------------------------------------------*/
static tPcpSerialTransferFin pfnTransfFin_l = NULL;
static UINT32 tranfSize_l = 0;

/*----------------------------------------------------------------------------*/
/* local function prototypes                                                  */
/*----------------------------------------------------------------------------*/
static void initGpio(void);
static void initSpi(void);
static void initDma(tHandlerParam * pTransParam_p);
static void initNvic(void);

static void receiveInputStream(UINT32 transfSize_p);

/*============================================================================*/
/*            P U B L I C   F U N C T I O N S                                 */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/**
\brief  Initialize the PCP serial in slave mode

This function init's the SPI serial device which connects the uP-Slave with the
POWERLINK processor in slave mode. It simply sniffs the incoming data from
the uP-Master <-> PCP connection.

\param pTransParam_p    The transfer parameters (rx/tx base and size)
\param pfnTransfFin_p   Pointer to the transfer finished interrupt

\retval TRUE    On success
\retval FALSE   Error during initialization

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
BOOL pcpserial_init(tHandlerParam * pTransParam_p, tPcpSerialTransferFin pfnTransfFin_p)
{
    UINT8 fReturn = FALSE;

    if(pTransParam_p != NULL)
    {
        /* Initialize all needed peripherals */
        initGpio();
        initSpi();
        initDma(pTransParam_p);
        initNvic();

        tranfSize_l = pTransParam_p->consDesc_m.buffSize_m;

        /* Init the SPI1 core */
        SPI_Cmd(SPIx, ENABLE);

        /* Disable both DMA channels for now */
        DMA_Cmd(DMAx_CHANNEL_RX, DISABLE);

        /* Enable SPI DMA interface */
        SPI_I2S_DMACmd(SPIx, SPI_I2S_DMAReq_Rx, ENABLE);

        /* Assign transfer finished interrupt callback function */
        pfnTransfFin_l = pfnTransfFin_p;

        receiveInputStream(tranfSize_l);

        fReturn = TRUE;
    }

    return fReturn;
}

/*----------------------------------------------------------------------------*/
/**
\brief  Close the serial device

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
void pcpserial_exit(void)
{
    pfnTransfFin_l =  NULL;

    /* Close the SPIx serial */
    SPI_I2S_DeInit(SPIx);

    /* Close the DMAx channels */
    DMA_DeInit(DMAx_CHANNEL_RX);

    /* Close the GPIO pins */
    GPIO_DeInit(SPIx_SCK_GPIO_PORT);
    GPIO_DeInit(SPIx_MOSI_GPIO_PORT);
    GPIO_DeInit(SPIx_SSN_GPIO_PORT);

}

/*----------------------------------------------------------------------------*/
/**
\brief  Start an SPI transfer

In slave mode no SPI transfer is started! Only the DMA finished interrupt
signals that an input stream was incoming.

\param[in] pHandlParam_p       The parameters of the SPI transfer handler

\retval TRUE        On success
\retval FALSE       SPI send or receive failed

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
BOOL pcpserial_transfer(tHandlerParam* pHandlParam_p)
{
    UNUSED_PARAMETER(pHandlParam_p);

    return TRUE;
}

/*============================================================================*/
/*            P R I V A T E   F U N C T I O N S                               */
/*============================================================================*/
/* \name Private Functions */
/* \{ */

/*----------------------------------------------------------------------------*/
/**
\brief  Initialize the SPI GPIO pins

Enable SCK, MOSI and NSS as input.

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
static void initGpio(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitTypeDef));

    /* Enable peripheral clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    /*  SPIx: SCK */
    GPIO_InitStructure.GPIO_Pin = SPIx_SCK_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SPIx_SCK_GPIO_PORT, &GPIO_InitStructure);

    /*  SPIx: MOSI */
    GPIO_InitStructure.GPIO_Pin = SPIx_MOSI_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SPIx_MOSI_GPIO_PORT, &GPIO_InitStructure);

    /* SPIx: SPI_NSS_Soft */
    GPIO_InitStructure.GPIO_Pin = SPIx_SSN_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SPIx_SSN_GPIO_PORT, &GPIO_InitStructure);
}

/*----------------------------------------------------------------------------*/
/**
\brief  Initialize the SPI core as slave

Setup SPI core as half duplex 8bit data with CPOL, CPHA = (0,0)

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
static void initSpi(void)
{
    SPI_InitTypeDef SPI_InitStructure;

    memset(&SPI_InitStructure, 0, sizeof(SPI_InitTypeDef));

    /* Enable the SPI periph */
    RCC_APB2PeriphClockCmd(SPIx_RCC_PERIPH, ENABLE);

    /* Reset SPI Interface */
    SPI_I2S_DeInit(SPIx);

    /* SPI configuration */
    SPI_StructInit(&SPI_InitStructure);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_RxOnly;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Slave;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Hard;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16; /* SPI_BaudRatePrescaler_2; */
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPIx, &SPI_InitStructure);
}

/*----------------------------------------------------------------------------*/
/**
\brief  Initialize the DMA receive channel

Setup the DMA for to receive from SPIx to RAM!

\param pSpiHandler_p    Pointer to the SPI handler

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
static void initDma(tHandlerParam * pTransParam_p)
{
    DMA_InitTypeDef DMA_InitStructure;

    memset(&DMA_InitStructure, 0, sizeof(DMA_InitTypeDef));

    /* Enable DMAx clock */
    RCC_AHBPeriphClockCmd(DMAx_RCC_PERIPH, ENABLE);

    /* Configure DMAx - ChannelRx (SPI -> memory) */
    DMA_DeInit(DMAx_CHANNEL_RX);    /* Reset DMA registers to default values */

    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPIx->DR;     /* Address of peripheral the DMA must map to */
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)(pTransParam_p->consDesc_m.pBuffBase_m);    /* Variable to which received data will be stored */
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = pTransParam_p->consDesc_m.buffSize_m;    /* Buffer size */
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMAx_CHANNEL_RX, &DMA_InitStructure);

    DMA_ITConfig(DMAx_CHANNEL_RX, DMA_IT_TC | DMA_IT_TE, ENABLE);
}

/*----------------------------------------------------------------------------*/
/**
\brief  Initialize the DMA interrupts in NVIC

Setup transfer finished and transfer error interrupts for the DMA
channel.

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
static void initNvic(void)
{
    NVIC_InitTypeDef   NVIC_InitStructure;

    memset(&NVIC_InitStructure, 0, sizeof(NVIC_InitTypeDef));

    /* Enable DMA1 Channel2 interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = DMAx_ChannelRx_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0E;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*----------------------------------------------------------------------------*/
/**
\brief  DMA receive channel interrupt handler (SPI -> memory)

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
void DMAx_ChannelRx_IRQHandler(void)
{
    volatile UINT8 dummy = 0;

    if(DMA_GetITStatus(DMAx_IT_TC2) == SET)
    {
        /* Disable the DMA channel */
        DMA_Cmd(DMAx_CHANNEL_RX, DISABLE);

        /* Wait until SPI is finished -> Then disable the SPI -> DMA interface */
        while(SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_BSY) == SET);

        /**
         * WORKAROUND: Sometimes the last byte of the DMA transfer is left
         * in the SPI DR register. The following workaround reads this byte
         * and prevents a permanent offset of the stream in memory.
         */
        if((SPIx->SR & SPI_SR_RXNE) != 0)
        {
            dummy = SPIx->DR;
        }

        SPI_I2S_DMACmd(SPIx, SPI_I2S_DMAReq_Rx, DISABLE);

        /* Call transfer finished callback function */
        if(pfnTransfFin_l != NULL)
            pfnTransfFin_l(FALSE);

        receiveInputStream(tranfSize_l);

        /* Clear the interrupt flag */
        DMA_ClearFlag(DMAx_FLAG_TC2);
    }
    else if(DMA_GetITStatus(DMAx_IT_TE2) == SET)
    {   /* Transfer error! */
        /* Call transfer finished callback function with error TRUE */
        if(pfnTransfFin_l != NULL)
            pfnTransfFin_l(TRUE);

        DMA_ClearFlag(DMAx_FLAG_TE2);
    }

    (void)dummy;
}

/*----------------------------------------------------------------------------*/
/**
\brief  Enable receive transfer

\param transfSize_p     The length of the transfer

\ingroup module_serial
*/
/*----------------------------------------------------------------------------*/
static void receiveInputStream(UINT32 transfSize_p)
{
    /* Reset the current data counter value */
    DMA_SetCurrDataCounter(DMAx_CHANNEL_RX , transfSize_p);

    /* Enable both DMA channels */
    DMA_Cmd(DMAx_CHANNEL_RX, ENABLE);

    /* Enable the SPIx DMA interface (This starts the transfer!) */
    SPI_I2S_DMACmd(SPIx, SPI_I2S_DMAReq_Rx, ENABLE);
}


/* \} */
