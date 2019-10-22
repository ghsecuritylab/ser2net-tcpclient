//
// Created by kompilator on 13.10.2019.
//

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "lwip.h"
#include "lwip/tcp.h"
#include "main.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/memp.h"
#include "app_ethernet.h"
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
__IO uint32_t message_count=0;
__IO uint8_t net_stat=0;

uint8_t data[100];
uint8_t dest_ip[4];
uint16_t dest_port;
uint8_t is_tcp_conn;

struct tcp_pcb *client_pcb;

/* Client protocol states */
enum tcp_client_states
{
    ES_NOT_CONNECTED = 0,
    ES_CONNECTING,
    ES_CONNECTED,
    ES_RECEIVED,
    ES_CLOSING,
};

/* structure to be passed as argument to the tcp callbacks */
struct tcp_client
{
    enum tcp_client_states state;     /* connection status */
    struct tcp_pcb *pcb;              /* pointer on the current tcp_pcb */
    struct pbuf *p_tx;                /* pointer on pbuf to be transmitted */
};

/* Private function prototypes -----------------------------------------------*/
static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void tcp_client_connection_close(struct tcp_pcb *tpcb, struct tcp_client * es);
static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb);
static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static void tcp_client_send(struct tcp_pcb *tpcb, struct tcp_client * es);
static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Connects to the TCP server
  * @param  None
  * @retval None
  */
void tcp_client_connect(void)
{
    ip_addr_t DestIPaddr;
    /* create new tcp pcb */
    client_pcb = tcp_new();

    if (client_pcb != NULL)
    {
        IP4_ADDR(&DestIPaddr, dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3]);

        /* connect to destination address/port */
        tcp_connect(client_pcb, &DestIPaddr, dest_port, tcp_client_connected);
        printf("connected to server:%d.%d.%d.%d:%d\r\n", dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3], dest_port );
    }
    else
    {
        /* deallocate the pcb */
        memp_free(MEMP_TCP_PCB, client_pcb);
        printf("\n\r can not create tcp pcb");
    }
}

/**
 * @brief
 */
void net_ini(void)
{
    dest_ip[0] = DEST_IP_ADDR0;
    dest_ip[1] = DEST_IP_ADDR1;
    dest_ip[2] = DEST_IP_ADDR2;
    dest_ip[3] = DEST_IP_ADDR3;
    dest_port = DEST_PORT;
    net_stat = ES_CONNECTING;
    tcp_client_connect();
}

/**
  * @brief Function called when TCP connection established
  * @param tpcb: pointer on the connection contol block
  * @param err: when connection correctly established err should be ERR_OK
  * @retval err_t: returned error
  */
static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    struct tcp_client *es = NULL;

    if (err == ERR_OK)
    {
        /* allocate structure es to maintain tcp connection informations */
        es = (struct tcp_client *)mem_malloc(sizeof(struct tcp_client));

        if (es != NULL)
        {
            es->state = ES_CONNECTED;
            es->pcb = tpcb;

            sprintf((char*)data, "sending tcp client message %d\r\n", (int)message_count);

            /* allocate pbuf */
            es->p_tx = pbuf_alloc(PBUF_TRANSPORT, strlen((char*)data) , PBUF_POOL);

            if (es->p_tx)
            {
                /* copy data to pbuf */
                pbuf_take(es->p_tx, (char*)data, strlen((char*)data));

                /* pass newly allocated es structure as argument to tpcb */
                tcp_arg(tpcb, es);

                /* initialize LwIP tcp_recv callback function */
                tcp_recv(tpcb, tcp_client_recv);

                /* initialize LwIP tcp_sent callback function */
                tcp_sent(tpcb, tcp_client_sent);

                /* initialize LwIP tcp_poll callback function */
                tcp_poll(tpcb, tcp_client_poll, 1);

                /* send data */
                tcp_client_send(tpcb,es);

                return ERR_OK;
            }
        }
        else
        {
            /* close connection */
            tcp_client_connection_close(tpcb, es);

            /* return memory allocation error */
            return ERR_MEM;
        }
    }
    else
    {
        /* close connection */
        tcp_client_connection_close(tpcb, es);
    }
    return err;
}

/**
  * @brief tcp_receiv callback
  * @param arg: argument to be passed to receive callback
  * @param tpcb: tcp connection control block
  * @param err: receive error code
  * @retval err_t: retuned error
  */
static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    struct tcp_client *es;
    err_t ret_err;

    LWIP_ASSERT("arg != NULL",arg != NULL);

    es = (struct tcp_client *)arg;

    /* if we receive an empty tcp frame from server => close connection */
    if (p == NULL)
    {
        /* remote host closed connection */
        es->state = ES_CLOSING;
        if(es->p_tx == NULL)
        {
            /* we're done sending, close connection */
            tcp_client_connection_close(tpcb, es);
        }
        else
        {
            /* send remaining data*/
            tcp_client_send(tpcb, es);
        }
        ret_err = ERR_OK;
    }
    /* else : a non empty frame was received from server but for some reason err != ERR_OK */
    else if(err != ERR_OK)
    {
        /* free received pbuf*/
        if (p != NULL)
        {
            pbuf_free(p);
        }
        ret_err = err;
    }
    else if(es->state == ES_CONNECTED)
    {
        /* increment message count */
        message_count++;

        /* Acknowledge data reception */
        tcp_recved(tpcb, p->tot_len);

        pbuf_free(p);
        tcp_client_connection_close(tpcb, es);
        ret_err = ERR_OK;
    }

    /* data received when connection already closed */
    else
    {
        /* Acknowledge data reception */
        tcp_recved(tpcb, p->tot_len);

        /* free pbuf and do nothing */
        pbuf_free(p);
        ret_err = ERR_OK;
    }
    return ret_err;
}

/**
  * @brief function used to send data
  * @param  tpcb: tcp control block
  * @param  es: pointer on structure of type echoclient containing info on data
  *             to be sent
  * @retval None
  */
static void tcp_client_send(struct tcp_pcb *tpcb, struct tcp_client * es)
{
    struct pbuf *ptr;
    err_t wr_err = ERR_OK;

    while ((wr_err == ERR_OK) &&
           (es->p_tx != NULL) &&
           (es->p_tx->len <= tcp_sndbuf(tpcb)))
    {

        /* get pointer on pbuf from es structure */
        ptr = es->p_tx;

        /* enqueue data for transmission */
        wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);

        if (wr_err == ERR_OK)
        {
            /* continue with next pbuf in chain (if any) */
            es->p_tx = ptr->next;

            if(es->p_tx != NULL)
            {
                /* increment reference count for es->p */
                pbuf_ref(es->p_tx);
            }

            /* free pbuf: will free pbufs up to es->p (because es->p has a reference count > 0) */
            pbuf_free(ptr);
        }
        else if(wr_err == ERR_MEM)
        {
            /* we are low on memory, try later, defer to poll */
            es->p_tx = ptr;
        }
        else
        {
            /* other problem ?? */
        }
    }
}

/**
  * @brief  This function implements the tcp_poll callback function
  * @param  arg: pointer on argument passed to callback
  * @param  tpcb: tcp connection control block
  * @retval err_t: error code
  */
static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb)
{
    err_t ret_err;
    struct tcp_client *es;

    es = (struct tcp_client*)arg;
    if (es != NULL)
    {
        if (es->p_tx != NULL)
        {
            /* there is a remaining pbuf (chain) , try to send data */
            tcp_client_send(tpcb, es);
        }
        else
        {
            /* no remaining pbuf (chain)  */
            if(es->state == ES_CLOSING)
            {
                /* close tcp connection */
                tcp_client_connection_close(tpcb, es);
            }
        }
        ret_err = ERR_OK;
    }
    else
    {
        /* nothing to be done */
        tcp_abort(tpcb);
        ret_err = ERR_ABRT;
    }
    return ret_err;
}

/**
  * @brief  This function implements the tcp_sent LwIP callback (called when ACK
  *         is received from remote host for sent data)
  * @param  arg: pointer on argument passed to callback
  * @param  tcp_pcb: tcp connection control block
  * @param  len: length of data sent
  * @retval err_t: returned error code
  */
static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct tcp_client *es;

    LWIP_UNUSED_ARG(len);

    es = (struct tcp_client *)arg;

    if(es->p_tx != NULL)
    {
        /* still got pbufs to send */
        tcp_client_send(tpcb, es);
    }

    return ERR_OK;
}

/**
  * @brief This function is used to close the tcp connection with server
  * @param tpcb: tcp connection control block
  * @param es: pointer on echoclient structure
  * @retval None
  */
static void tcp_client_connection_close(struct tcp_pcb *tpcb, struct tcp_client * es )
{
    /* remove callbacks */
    tcp_recv(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_poll(tpcb, NULL,0);

    if (es != NULL)
    {
        mem_free(es);
    }

    /* close tcp connection */
    tcp_close(tpcb);
}