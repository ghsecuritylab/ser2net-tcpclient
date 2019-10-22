//
// Created by kompilator on 13.10.2019.
//

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_ETHERNET_H
#define __APP_ETHERNET_H

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

#define DEST_IP_ADDR0   (uint8_t)192
#define DEST_IP_ADDR1   (uint8_t)168
#define DEST_IP_ADDR2   (uint8_t)1
#define DEST_IP_ADDR3   (uint8_t)144

#define DEST_PORT       (uint32_t)7788

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void net_ini(void);

#endif //__APP_ETHERNET_H
