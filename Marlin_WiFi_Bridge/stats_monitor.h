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

#include <stdint.h>
#include <stdbool.h>

// Коэффициент сглаживания для экспоненциального среднего (EMA).
// Сдвиг на 4 означает деление на 16. Окно сглаживания составляет около 16 измерений.
#define AVG_SHIFT   4

#define STAT_R_TIME 3600    //  интервал сброса статистики

// Структура для хранения метрик одной измеряемой величины
typedef struct {
    uint32_t min;      // Абсолютный минимум за период (сбрасываемый)
    uint32_t minT;     // Uptime в момент фиксации значения min
    uint32_t minV;     // Плавающий (распадающийся) минимум — базовая задержка канала
    uint32_t max;      // Абсолютный максимум за период (сбрасываемый)
    uint32_t maxT;     // Uptime в момент фиксации значения max
    uint32_t maxV;     // Плавающий (распадающийся максимум) — пиковые всплески
    uint32_t avg;      // Накопитель суммы для среднего значения (хранится как Среднее * 16)
    uint32_t cur;      // Текущее отфильтрованное среднее значение для вывода на графики
    uint32_t sum;      // Накопитель "сырой" суммы для измерения скорости (stat_reset() сбрасывает в 0)
    uint32_t count;    // Общий счетчик измерений (полезен для вычисления пакетов в секунду)
    bool first;        // Флаг первого запуска для корректной инициализации «на лету»
} valStat_t;

// Удобные и читаемые индексы для ваших измеряемых величин
typedef enum {
    MTR_OFF = 0,        // если переменная mtr_count == MTR_OFF, блок статистики отключается
    MTR_BCAST_GONE,     // Время нахождения широковещательных пакетов в буфере
    MTR_UCAST_GONE,     // Время нахождения unicast пакетов в буфере
    MTR_CHUNK_GONE,     // Время обработки чанка данных
    MTR_CHUNK_EMPTY,    // Интервалы пустых чанков
    MTR_CHUNK_TX,       // Время непосредственной передачи чанка в принтер
    MTR_ZERO_BUF,       // Время ожидания при занулении/нехватке буфера
    MTR_RAM_TOTAL,      // Общий размер доступной оперативной памяти
    MTR_RAM_BLOCK,      // Размер максимального блока памяти
    MTR_NET_BUF_SIZE,   // Размер данных на передачу в кольцевом сетевом буфере
    MTR_RX_BUF_SIZE,    // Размер прочитанных данных в кольцевом буфере UART RX
    MTR_RX_LOOP_TIME,   // время (мсек) отработки getMarlin()
    MTR_OK_WAIT_TIME,   // время (мсек) ожидания подтверждения от Марлин
    MTR_WS_SEND_TIME,   // время (мсек) отработки webSocket.sendBIN()
    MTR_WS_LOOP_TIME,   // время (мсек) отработки webSocket.loop()
    MTR_MQ_LOOP_TIME,   // время (мксек) отработки mqttSocketLoop()
    MTR_MQ_PKT_TIME,    // время (мксек) обработки пакета
    MTR_TOTAL_COUNT     // Автоматический подсчет общего количества метрик
                        // если переменная mtr_count == MTR_TOTAL_COUNT, блок статистики работает
} StatMetrics_t;

//// Экспортируем глобальный массив указателей для внешнего использования
//extern valStat_t* stats[MTR_TOTAL_COUNT];

//// Экспортируем прямые переменные, если нужно обращаться к ним напрямую по имени
//extern StatMetrics_t mtr_count
//extern valStat_t bcGone, ucGone, chGone, chEmpty, chTx, zWait, ram, mblk, netSize, rxSize, parseTime, okWTime, wsSendTime, wsLoopTime, mqLoopTime, mqPktTime;

StatMetrics_t mtr_count = MTR_OFF;

// Инициализация метрики по её индексу из enum
void mtr_init(StatMetrics_t metric_idx);

// Инициализация модуля (сброс всех флагов на старте)
void stat_init(void);

// Обновление метрики по её индексу из enum
void stat(StatMetrics_t metric_idx, uint32_t raw);

// Сброс абсолютных рекордов (min/max) к текущему среднему уровню
void stat_reset(void);

// 1. Физическое выделение памяти под отдельные структуры
valStat_t bcGone, ucGone, chGone, chEmpty, chTx, zWait, ram, mblk, netSize, rxSize, rxLoopTime, okWTime, wsSendTime, wsLoopTime, mqLoopTime, mqPktTime;

// 2. Формирование массива указателей. 
// Использование назначенных инициализаторов [INDEX] защищает от ошибок копипаста.
valStat_t* stats[MTR_TOTAL_COUNT] = {
  [MTR_OFF]           = &bcGone,  // фиктивный указатель вместо nullptr
  [MTR_BCAST_GONE]    = &bcGone,
  [MTR_UCAST_GONE]    = &ucGone,
  [MTR_CHUNK_GONE]    = &chGone,
  [MTR_CHUNK_EMPTY]   = &chEmpty,
  [MTR_CHUNK_TX]      = &chTx,
  [MTR_ZERO_BUF]      = &zWait,
  [MTR_RAM_TOTAL]     = &ram,
  [MTR_RAM_BLOCK]     = &mblk,
  [MTR_NET_BUF_SIZE]  = &netSize,
  [MTR_RX_BUF_SIZE]   = &rxSize,
  [MTR_RX_LOOP_TIME]  = &rxLoopTime,
  [MTR_OK_WAIT_TIME]  = &okWTime,
  [MTR_WS_SEND_TIME]  = &wsSendTime,
  [MTR_WS_LOOP_TIME]  = &wsLoopTime,
  [MTR_MQ_LOOP_TIME]  = &mqLoopTime,
  [MTR_MQ_PKT_TIME]   = &mqPktTime
};
