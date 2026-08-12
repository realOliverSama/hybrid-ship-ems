/**
 ****************************************************************************************************
 * @file        app_netxduo.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_netxduo.c文件
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#include "app_netxduo.h"
#include "nx_stm32_eth_driver.h"
#include "nx_stm32_phy_driver.h"
#include "nxd_dhcp_client.h"
#include "./LED/led.h"
#include "./UART/uart.h"

#define TCP_SERVER_IP_ADDRESS   IP_ADDRESS(192, 168, 2, 11)
#define TCP_SERVER_PORT         8080
#define LOCAL_PORT              8080

#define PAYLOAD_SIZE            1536
#define PACKET_POOL_SIZE        ((PAYLOAD_SIZE + sizeof(NX_PACKET)) * 10)
#define IP_THREAD_STACK_SIZE    (2 * 1024)
#define IP_THREAD_PRIORITY      10
#define LINK_THREAD_STACK_SIZE  (2 * 1024)
#define LINK_THREAD_PRIORITY    11
#define APP_THREAD_STACK_SIZE   (2 * 1024)
#define APP_THREAD_PRIORITY     10

static NX_PACKET_POOL packet_pool;
static NX_IP ip_instance;
static NX_DHCP dhcp_client;
static TX_SEMAPHORE dhcp_semaphore;
static ULONG ip_address;
static ULONG network_mask;
static TX_THREAD link_thread;
static TX_THREAD app_thread;

static VOID link_thread_entry(ULONG id);
static VOID app_thread_entry(ULONG id);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *additional_info);

UINT app_netxduo_init(VOID *memory_ptr)
{
    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
    CHAR *pointer;

    printf("TCP Server Configuration\r\n");
    printf("TCP Server IP: %lu.%lu.%lu.%lu\r\n", (TCP_SERVER_IP_ADDRESS >> 24) & 0xFF, (TCP_SERVER_IP_ADDRESS >> 16) & 0xFF, (TCP_SERVER_IP_ADDRESS >> 8) & 0xFF, TCP_SERVER_IP_ADDRESS & 0xFF);
    printf("TCP Server Port: %u\r\n", TCP_SERVER_PORT);
    printf("\r\n");

    nx_system_initialize();

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (nx_packet_pool_create(&packet_pool, "Packet Pool", PAYLOAD_SIZE, pointer, PACKET_POOL_SIZE) != NX_SUCCESS)
    {
        return NX_POOL_ERROR;
    }

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, IP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (nx_ip_create(&ip_instance, "IP Instance", 0, 0, &packet_pool, nx_stm32_eth_driver, pointer, IP_THREAD_STACK_SIZE, IP_THREAD_PRIORITY) != NX_SUCCESS)
    {
        return NX_NOT_SUCCESSFUL;
    }

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, 1024, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (nx_arp_enable(&ip_instance, (VOID *)pointer, 1024) != NX_SUCCESS)
    {
        return NX_NOT_SUCCESSFUL;
    }

    if (nx_icmp_enable(&ip_instance) != NX_SUCCESS)
    {
        return NX_NOT_SUCCESSFUL;
    }

    if (nx_tcp_enable(&ip_instance) != NX_SUCCESS)
    {
        return NX_NOT_SUCCESSFUL;
    }

    if (nx_udp_enable(&ip_instance) != NX_SUCCESS)
    {
        return NX_NOT_SUCCESSFUL;
    }

    if (nx_dhcp_create(&dhcp_client, &ip_instance, "DHCP Client") != NX_SUCCESS)
    {
        return NX_DHCP_ERROR;
    }

    if (tx_semaphore_create(&dhcp_semaphore, "DHCP Semaphore", 0) != TX_SUCCESS)
    {
        return TX_SEMAPHORE_ERROR;
    }

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, LINK_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_thread_create(&link_thread, "Link Thread", link_thread_entry, 0, pointer, LINK_THREAD_STACK_SIZE, LINK_THREAD_PRIORITY, LINK_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_thread_create(&app_thread, "App Thread", app_thread_entry , 0, pointer, APP_THREAD_STACK_SIZE, APP_THREAD_PRIORITY, APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START) != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }

    return TX_SUCCESS;
}

static VOID app_thread_entry(ULONG id)
{
    NX_TCP_SOCKET tcp_client_socket;
    UINT ret;
    NX_PACKET *data_packet;
    NX_PACKET *server_packet;
    ULONG source_ip_address;
    UINT source_port;
    CHAR data_buffer[512];
    ULONG bytes_read;

    if (nx_ip_address_change_notify(&ip_instance, ip_address_change_notify_callback, NULL) != NX_SUCCESS)
    {
        Error_Handler();
    }

    if (nx_dhcp_start(&dhcp_client) != NX_SUCCESS)
    {
        Error_Handler();
    }

    printf("Looking for DHCP server...\r\n");
    if(tx_semaphore_get(&dhcp_semaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
        Error_Handler();
    }

    printf("IP Address: %lu.%lu.%lu.%lu\r\n", (ip_address >> 24) & 0xFF, (ip_address >> 16) & 0xFF, (ip_address >> 8) & 0xFF, ip_address & 0xFF);
    printf("Network Mask: %lu.%lu.%lu.%lu\r\n", (network_mask >> 24) & 0xFF, (network_mask >> 16) & 0xFF, (network_mask >> 8) & 0xFF, network_mask & 0xFF);

    if (nx_tcp_socket_create(&ip_instance, &tcp_client_socket, "TCP Client Socket", NX_IP_NORMAL, NX_FRAGMENT_OKAY, NX_IP_TIME_TO_LIVE, 512, NX_NULL, NX_NULL) != NX_SUCCESS)
    {
        Error_Handler();
    }

    if (nx_tcp_client_socket_bind(&tcp_client_socket, LOCAL_PORT, NX_WAIT_FOREVER) != NX_SUCCESS)
    {
        Error_Handler();
    }

    printf("Connecting to TCP server...");
    while (1)
    {
        ret = nx_tcp_client_socket_connect(&tcp_client_socket, TCP_SERVER_IP_ADDRESS, TCP_SERVER_PORT, NX_IP_PERIODIC_RATE);
        if (ret == NX_SUCCESS)
        {
            break;
        }
        else if (ret == NX_NOT_CONNECTED)
        {
            printf(".");
            continue;
        }
        else
        {
            Error_Handler();
        }
    }
    printf("\r\nConnected to TCP server. [%lu.%lu.%lu.%lu:%u]\r\n", (TCP_SERVER_IP_ADDRESS >> 24) & 0xFF, (TCP_SERVER_IP_ADDRESS >> 16) & 0xFF, (TCP_SERVER_IP_ADDRESS >> 8) & 0xFF, (TCP_SERVER_IP_ADDRESS & 0xFF), TCP_SERVER_PORT);

    while (1)
    {
        if (nx_tcp_socket_receive(&tcp_client_socket, &server_packet, NX_WAIT_FOREVER) != NX_SUCCESS)
        {
            break;
        }

        if (nx_udp_source_extract(server_packet, &source_ip_address, &source_port) != NX_SUCCESS)
        {
            break;
        }

        if (nx_packet_data_retrieve(server_packet, data_buffer, &bytes_read) != NX_SUCCESS)
        {
            break;
        }

        nx_packet_release(server_packet);

        data_buffer[bytes_read] = '\0';
        printf("[%lu.%lu.%lu.%lu:%u] -> '\r\n%s\r\n'\r\n\r\n", (source_ip_address >> 24) & 0xFF, (source_ip_address >> 16) & 0xFF, (source_ip_address >> 8) & 0xFF, (source_ip_address & 0xFF), source_port, data_buffer);

        if (nx_packet_allocate(&packet_pool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER) != NX_SUCCESS)
        {
            break;
        }

        if (nx_packet_data_append(data_packet, (VOID *)data_buffer, strlen(data_buffer) + 1, &packet_pool, TX_WAIT_FOREVER) != NX_SUCCESS)
        {
            nx_packet_release(data_packet);
            break;
        }

        if (nx_tcp_socket_send(&tcp_client_socket, data_packet, 10 * NX_IP_PERIODIC_RATE) != NX_SUCCESS)
        {
            break;
        }
    }

    nx_packet_release(server_packet);
    nx_tcp_socket_disconnect(&tcp_client_socket, 10 * NX_IP_PERIODIC_RATE);
    nx_tcp_client_socket_unbind(&tcp_client_socket);
    nx_tcp_socket_delete(&tcp_client_socket);
}

static VOID link_thread_entry(ULONG id)
{
    UINT status;
    ULONG actual_status;
    UINT linkdown = 2;
    int32_t link_state;
  
    while(1)
    {
        status = nx_ip_interface_status_check(&ip_instance, 0, NX_IP_LINK_ENABLED, &actual_status, 10);
        if(status == NX_SUCCESS)
        {
            if ((linkdown == 1) || (linkdown == 2))
            {
                linkdown = 0;

                printf("The network cable is connected.\r\n");

                link_state = nx_eth_phy_get_link_state();
                if (link_state == ETH_PHY_STATUS_100MBITS_FULLDUPLEX)
                {
                    printf("100Mbps full-duplex.\r\n");
                }
                else if (link_state == ETH_PHY_STATUS_100MBITS_HALFDUPLEX)
                {
                    printf("100Mbps half-duplex.\r\n");
                }
                else if (link_state == ETH_PHY_STATUS_10MBITS_FULLDUPLEX)
                {
                    printf("10Mbps full-duplex.\r\n");
                }
                else
                {
                    printf("10Mbps half-duplex.\r\n");
                }

                nx_ip_driver_direct_command(&ip_instance, NX_LINK_ENABLE, &actual_status);

                tx_thread_resume(&app_thread);
                tx_thread_relinquish();
            }
        }
        else
        {
            if ((linkdown == 0) || (linkdown == 2))
            {
                linkdown = 1;

                printf("The network cable is not connected.\r\n");

                nx_ip_driver_direct_command(&ip_instance, NX_LINK_DISABLE, &actual_status);
            }
        }

        tx_thread_sleep(NX_IP_PERIODIC_RATE);
    }
}

static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *additional_info)
{
    if (nx_ip_address_get(ip_instance, &ip_address, &network_mask) != NX_SUCCESS)
    {
        Error_Handler();
    }

    if(ip_address != 0)
    {
        tx_semaphore_put(&dhcp_semaphore);
    }
}
