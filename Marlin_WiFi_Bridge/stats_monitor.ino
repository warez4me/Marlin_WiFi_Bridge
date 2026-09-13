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

#include "stats_monitor.h"

void mtr_init(StatMetrics_t mtr_idx) {
  uint32_t now = millis();
  stats[mtr_idx]->min   = 0; stats[mtr_idx]->minV  = 0;
  stats[mtr_idx]->minT  = now;
  stats[mtr_idx]->max   = 0; stats[mtr_idx]->maxV  = 0;
  stats[mtr_idx]->maxT  = now;
  stats[mtr_idx]->avg   = 0; stats[mtr_idx]->cur   = 0;
  stats[mtr_idx]->sum   = 0; stats[mtr_idx]->count = 0;
  stats[mtr_idx]->first = true; // флаг для захвата первого измерения
}

// Функция инициализации. Гарантирует, что при старте системы в структурах не окажется мусора.
void stat_init(void) {
  for (StatMetrics_t i = MTR_BCAST_GONE; i < MTR_TOTAL_COUNT; i = (StatMetrics_t)(((int)i) + 1))
    mtr_init(i);
}

// Основная функция расчета параметров «на лету»
void stat(StatMetrics_t metric_idx, uint32_t raw) {
  // Защита от выхода за границы массива, если передан неверный enum
  // Либо, если mtr_count == MTR_OFF, расчет статистики не производится, лишние затраты времени устраняются
  // Для работы блока статистики необходимо установить mtr_count == MTR_TOTAL_COUNT
  if (metric_idx >= mtr_count) return;
  valStat_t* val = stats[metric_idx];
  if (val->first) {
    uint32_t now = millis();
    // Логика первого запуска: привязываем базовые значения к первому измерению
    val->avg   = (uint32_t)raw << AVG_SHIFT; // Виртуальное умножение для точности
    val->cur   = raw;
    val->min   = raw; val->minV  = raw; val->minT = now;
    val->max   = raw; val->maxV  = raw; val->maxT = now;
    val->sum   = raw; val->count = 1;
    val->first = false;
    } 
   else {
    // --- 1. Расчет Максимумов ---
    // Абсолютный максимум (запоминает худший пик до момента сброса)
    if (raw > val->max) { val->max = raw; val->maxT = millis(); }
    // Плавающий максимум (мгновенно взлетает, но медленно "остывает" вниз на 1/128 долю)
    if (raw > val->maxV) val->maxV = raw;                   // Мгновенный взлет до нового рекорда
     else
      val->maxV = val->maxV - (val->maxV >> 7); // Медленное остывание (коэффициент подбирается по вкусу)
                                                // Например, уменьшаем на 1/128 часть на каждом шаге
    // --- 2. Расчет Минимумов ---
    // Абсолютный минимум (запоминает лучший результат до момента сброса)
    if (raw < val->min) { val->min = raw; val->minT = millis(); }
    // Плавающий минимум (мгновенно падает, но медленно подтягивается вверх к реальности на 1/128 долю)
    if (raw < val->minV) val->minV = raw;               // Мгновенный спад до нового рекорда
     else
      val->minV = val->minV + ((raw - val->minV) >> 7); // Медленное нагревание (коэффициент подбирается по вкусу)
                                                        // Например, увеличиваем на 1/128 часть на каждом шаге
    // --- 3. Расчет Экспоненциального Среднего (EMA) ---
    // Формула с накоплением суммы без потери остатка от деления целых чисел
    // Основная магия: вычли одну долю, добавили реальность
    val->avg = val->avg - (val->avg >> AVG_SHIFT) + raw;
    val->cur = val->avg >> AVG_SHIFT; // Возвращаем реальный масштаб
    // Общая сумма и инкремент счетчика общего количества измерений
    val->sum += raw; val->count++;
    }
}

// Функция периодического сброса абсолютных пиков (например, для вызова раз в час)
void stat_reset(void) {
  uint32_t now = millis();
  for (int i = MTR_BCAST_GONE; i < MTR_TOTAL_COUNT; i++) {
    valStat_t* val = stats[i];
    // Сбрасываем только если структура уже была инициализирована измерениями
    if (!val->first) {
      val->max = val->cur; val->maxT = now;  // Сбрасываем к текущему среднему
      val->min = val->cur; val->minT = now;  // Сбрасываем к текущему среднему
      val->sum = 0; val->count = 0;
      }
    }
}
