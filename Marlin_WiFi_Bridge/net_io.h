/*
 * Copyright (c) 2026 warez4me
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

#define T_STR_SIZE    13
#define NET_DATA_MAX  (PACKET_BUF_SIZE - T_STR_SIZE)
char tBuf[T_STR_SIZE + 5 + (T_STR_SIZE & 1)];

#define SEND_BUF_SIZE 256
uint8_t netSendBuf[SEND_BUF_SIZE];

#define NET_BUF_SIZE  2048
#define NET_BUF_RING  (NET_BUF_SIZE - 1)
uint8_t netBuf[NET_BUF_SIZE];

volatile uint32_t netBufBeg = 0, netBufEnd = 0;

struct __attribute__((__packed__)) netData_t {
  uint32_t time; // Время добавления в очередь
  uint32_t size;
  uint8_t cMap;
};
