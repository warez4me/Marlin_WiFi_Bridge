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

// Глобальный объект сетевого движка
// Только указатель без инициализации для стабильного старта системы
WiFiClient* mqttSock = nullptr;

// Настройки вашего проекта
#define MQTT_CONNACK_INTERVAL   3000  // Макс. время ожидания CONNACK от брокера (=3 сек)
#define MQTT_PING_INTERVAL      30000 // Пинг брокеру раз в 30 секунд
#define MQTT_TRUST_TIME         4000  // задержка логического DISCONNECT после потери .connected()
#define MQTT_REGULAR_LOOP       20    // основной интервал опроса сокета mqtt (=20 мсек)
#define MQTT_LISTEN_LOOP        3     // интервал опроса сокета mqtt при чтении данных (=3 мсек)
#define MQTT_CONNECT_WAIT       5000  // время ожидания след. попытки коннекта с брокером

// 1. Шкала результатов Ника О'Лири (Для переменной mqttSockData.lastRC и ошибок)
#define MQTT_CONNECTION_TIMEOUT     -4
#define MQTT_CONNECTION_LOST        -3
#define MQTT_CONNECT_FAILED         -2
#define MQTT_DISCONNECTED           -1
#define MQTT_CONNECTED               0
#define MQTT_CONNECT_BAD_PROTOCOL    1
#define MQTT_CONNECT_BAD_CLIENT_ID   2
#define MQTT_CONNECT_UNAVAILABLE     3
#define MQTT_CONNECT_BAD_CREDENTIALS 4
#define MQTT_CONNECT_UNAUTHORIZED    5

// 2. Логические состояния лестницы сокета (Для переменной mqttSockData.sockState)
#define MQTT_SOCKET_DISCONNECTED     0
#define MQTT_SOCKET_CONNECTED        1 // Сокет в режиме покоя (ожидание пакетов)
#define MQTT_SOCKET_WAIT             2 // Ждем отправленный CONNECT
#define MQTT_SOCKET_READ_REMAIN_LEN  3 // Пошаговый сбор байт длины пакета (Remaining Length)
#define MQTT_SOCKET_READ_LEN         4 // Пошаговый сбор 2 байт (длины топика или ответа CONNACK)
#define MQTT_SOCKET_READ_TOPIC       5 // Пошаговый сбор и оффсет-парсинг текста топика
#define MQTT_SOCKET_READ_PAYLOAD     6 // Пошаговый сбор чистых данных (payload) команды
#define MQTT_SOCKET_DISCARD          7 // Неблокирующий слив мусора

// СУРОВЫЕ СИ-МАКРОСЫ ДЛЯ АВТОМАТИЧЕСКОГО РАСПОЗНАВАНИЯ RAM/PROGMEM ПО АДРЕСУ
// На ESP8266 адреса >= 0x40000000 железно принадлежат Flash-памяти (PROGMEM)
#define MQTT_GET_LEN(p)  ((p) ? (((uint32_t)(p) >= 0x40000000) ? strlen_P(p) : strlen(p)) : 0)
#define MQTT_SOCK_WRITE(p, len) (((uint32_t)(p) >= 0x40000000) ? mqttSock->write_P(p, len) : mqttSock->write((const uint8_t*)(p), len))

// Глобальные переменные для работы побайтового автомата чтения
struct {
  uint32_t sockState = MQTT_SOCKET_DISCONNECTED;
  uint32_t lastRC = MQTT_CONNECTED;
  uint32_t lastPing = 0;
  uint32_t waitStart = 0;
  uint32_t packetStart = 0;
  uint32_t remainLen = 0;
  uint32_t multiplier = 0;
  uint32_t topicLen = 0;
  uint32_t payloadLen = 0;
  uint32_t topicBytesRead = 0;
  uint8_t  packetType = 0;
  uint8_t  idLo = 0;
  uint8_t  idHi = 0;
  uint8_t  buf[SVC_BUF_SIZE] = {0};
  } mqttSockData;
