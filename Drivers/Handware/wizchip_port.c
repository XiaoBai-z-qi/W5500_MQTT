/*
 * wizchip_port.c
 *
 *  Created on: Oct 27, 2025
 *      Author: controllerstech
 */

#include "main.h"
#include "wizchip_conf.h"
#include "stdio.h"
#include "string.h"
#include "socket.h"
#include "stdbool.h"
#include "DHCP/dhcp.h"
#include "DNS/dns.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "debug_uart.h"
#include "wizchip_port.h"

#define SPI_DMA_CHUNK 256
#define W5500_SPI hspi1
#define USE_DHCP  0

static SemaphoreHandle_t spi_mutex;          // SPI互斥锁（防止多任务冲突）
static SemaphoreHandle_t spi_txrx_done_sem;  // DMA完成信号量


wiz_NetInfo netInfo = {
    .mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    .ip = {192, 168, 137, 20},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 137, 1},
    .dns = {114, 114, 114, 114},
#if USE_DHCP
	.dhcp = NETINFO_DHCP
#else
    .dhcp = NETINFO_STATIC
#endif
};

/*************************************************   NO Changes After This   ***************************************************************/

#define W5500_CS_LOW()     HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET)
#define W5500_CS_HIGH()    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET)
#define W5500_RST_LOW()    HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_RESET)
#define W5500_RST_HIGH()   HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_SET)


extern SPI_HandleTypeDef W5500_SPI;

void W5500_SPI_RTOS_Init(void)
{
    spi_mutex = xSemaphoreCreateMutex();          // 互斥锁
    spi_txrx_done_sem = xSemaphoreCreateBinary(); // DMA完成信号量
}
void W5500_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi == &W5500_SPI)
    {
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(spi_txrx_done_sem, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void W5500_TransmitReceive(uint8_t *tx_buf, uint8_t *rx_buf, uint16_t len)
{
    HAL_SPI_TransmitReceive_DMA(&W5500_SPI, tx_buf, rx_buf, len);
    xSemaphoreTake(spi_txrx_done_sem, portMAX_DELAY);
}

void W5500_WriteBuff(uint8_t *pBuf, uint16_t len)
{
    static uint8_t dummy_rx[SPI_DMA_CHUNK];

    /* 防多任务冲突 */
    xSemaphoreTake(spi_mutex, portMAX_DELAY);

    while (len)
    {
        uint16_t chunk = (len > SPI_DMA_CHUNK) ? SPI_DMA_CHUNK : len;

        W5500_TransmitReceive(pBuf, dummy_rx, chunk);

        pBuf += chunk;
        len -= chunk;
    }

    xSemaphoreGive(spi_mutex);
}

void W5500_ReadBuff(uint8_t *pBuf, uint16_t len)
{
    static uint8_t dummy_tx[SPI_DMA_CHUNK];
    memset(dummy_tx, 0xFF, SPI_DMA_CHUNK);

    xSemaphoreTake(spi_mutex, portMAX_DELAY);

    while (len)
    {
        uint16_t chunk = (len > SPI_DMA_CHUNK) ? SPI_DMA_CHUNK : len;

        W5500_TransmitReceive(dummy_tx, pBuf, chunk);

        pBuf += chunk;
        len -= chunk;
    }

    xSemaphoreGive(spi_mutex);
}

// SPI transmit/receive
void W5500_Select(void)   { W5500_CS_LOW(); }
void W5500_Unselect(void) { W5500_CS_HIGH(); }

uint8_t W5500_ReadByte(void)
{
    uint8_t rx;
    uint8_t tx = 0xFF;
    HAL_SPI_TransmitReceive(&W5500_SPI, &tx, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

void W5500_WriteByte(uint8_t byte)
{
    HAL_SPI_Transmit(&W5500_SPI, &byte, 1, HAL_MAX_DELAY);
}


#if USE_DHCP
volatile bool ip_assigned = false;
#define DHCP_SOCKET   7  // last available socket

uint8_t DHCP_buffer[548];


void Callback_IPAssigned(void) {
    ip_assigned = true;
}

void Callback_IPConflict(void) {
    ip_assigned = false;
}
#endif

#define DNS_SOCKET	  6  // 2nd last socket
uint8_t DNS_buffer[512];

int W5500_Init(void)
{
    W5500_SPI_RTOS_Init();
    uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};

    /***** Reset Sequence  *****/
    W5500_RST_LOW();
    wizchip_delay(50);
    W5500_RST_HIGH();
    wizchip_delay(200);

    /***** Register callbacks  *****/
    reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
    reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);
    reg_wizchip_spiburst_cbfunc(W5500_ReadBuff, W5500_WriteBuff);
    /***** Initialize the chip  *****/
    if (ctlwizchip(CW_INIT_WIZCHIP, (void*)memsize) == -1){
    	Debug_Printf("Error while initializing WIZCHIP\r\n");
    	return -1;
    }
    Debug_Printf("WIZCHIP Initialized\r\n");

    /***** check communication by reading Version  *****/
    uint8_t ver = getVERSIONR();
    if (ver != 0x04){
    	Debug_Printf("Error Communicating with W5500\t Version: 0x%02X\r\n", ver);
    	return -2;
    }
    Debug_Printf("Checking Link Status..\r\n");

 	/*****  CHeck Link Status  *****/
    uint8_t link = PHY_LINK_OFF;
    uint8_t retries = 10;
    while ((link != PHY_LINK_ON) && (retries > 0)){
        ctlwizchip(CW_GET_PHYLINK, &link);
        if (link == PHY_LINK_ON) Debug_Printf("Link: UP\r\n");
        else Debug_Printf("Link: DOWN Retrying : %d\r\n", 10-retries);
        retries--;
        wizchip_delay(500);
    }
    if (link != PHY_LINK_ON){
    	Debug_Printf ("Link is Down,please reconnect and retry\nExiting Setup..\r\n");
    	return 3;
    }

    /***** Use DHCP or Static IP  *****/
#if USE_DHCP
    Debug_Printf ("Using DHCP.. Please Wait..\r\n");
    setSHAR(netInfo.mac);
    DHCP_init(DHCP_SOCKET, DHCP_buffer);

    reg_dhcp_cbfunc(Callback_IPAssigned, Callback_IPAssigned, Callback_IPConflict);

    retries = 20;
    while((!ip_assigned) && (retries > 0)) {
        DHCP_run();
        wizchip_delay(500);
        retries--;
    }
    if(!ip_assigned) {
    	// DHCP Failed, switch to static IP
    	Debug_Printf ("DHCP Failed, switching to static IP\r\n");
    	ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
    }
    else {
    	// if IP is allocated, read it
        getIPfromDHCP(netInfo.ip);
        getGWfromDHCP(netInfo.gw);
        getSNfromDHCP(netInfo.sn);
        getDNSfromDHCP(netInfo.dns);

        // Now apply them to the chip
        ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
        Debug_Printf("DHCP IP assigned successfully\r\n");
    }

#else
    // use static IP (Not DHCP)
    Debug_Printf ("Using Static IP..\r\n");
    ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
#endif

    /***** Configure DNS  *****/
    wizchip_delay(500);
    Debug_Printf("Configuring DNS..\r\n");
    DNS_init(DNS_SOCKET, DNS_buffer);

    /***** Print assigned IP on the console  *****/
    wiz_NetInfo tmpInfo;
    ctlnetwork(CN_GET_NETINFO, &tmpInfo);
    Debug_Printf("IP: %d.%d.%d.%d\r\n", tmpInfo.ip[0], tmpInfo.ip[1], tmpInfo.ip[2], tmpInfo.ip[3]);
    Debug_Printf("SUBNET: %d.%d.%d.%d\r\n", tmpInfo.sn[0], tmpInfo.sn[1], tmpInfo.sn[2], tmpInfo.sn[3]);
    Debug_Printf("GATEWAY: %d.%d.%d.%d\r\n", tmpInfo.gw[0], tmpInfo.gw[1], tmpInfo.gw[2], tmpInfo.gw[3]);
    Debug_Printf("DNS: %d.%d.%d.%d\r\n", tmpInfo.dns[0], tmpInfo.dns[1], tmpInfo.dns[2], tmpInfo.dns[3]);

    return 0;
}
