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

///---- UART1 function -----///
/*void beep(uint32_t freq) {
  //clear tx fifo,not ready
  SET_PERI_REG_MASK(UART_CONF0(UART1), UART_TXFIFO_RST);      // reset TX FIFO
  CLEAR_PERI_REG_MASK(UART_CONF0(UART1), UART_TXFIFO_RST);    // return to work state
  uart_set_freq(UART1, freq);
  for (uint32_t i = 0; i < ((beep_dur < UART_FIFO_MAX_LEN)? beep_dur: UART_FIFO_MAX_LEN); i++)
    WRITE_PERI_REG(UART_FIFO(UART1), beep_ptrn);  // +1 start_bit='0', +1 stop_bit='1'
}
*/
///---- UART functions -----///
void IRAM_ATTR uart0_intr_handler(void *arg_0, void *arg_1) {
  (void)arg_0, (void)arg_1;
	uint32 uart_intr_status = READ_PERI_REG(UART_INT_ST(UART0)); // get uart intr status
  //do {
    if (uart_intr_status & (UART_RXFIFO_FULL_INT_ST | UART_RXFIFO_TOUT_INT_ST)) {
      uint32_t buf_beg = uart_rx_buf_beg; // переходим на регистры
      uint32_t buf_end = uart_rx_buf_end;
      uint32_t i, fifo_cnt;
      bool need_sync = false;
      do {
        fifo_cnt = ((READ_PERI_REG(UART_STATUS(0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT);
        for (i = 0; ((fifo_cnt != 0) && ((buf_end - buf_beg) < UART_BUF_SIZE)); fifo_cnt--, buf_end++, i++)
          uart_rx_buf[buf_end & UART_BUF_RING_MASK] = (char)(READ_PERI_REG(UART_FIFO(0)) & 0xff);
        if (i != 0)
          need_sync = true ;
        } while ((i > 0) && ((buf_end - buf_beg) < UART_BUF_SIZE)); // Повторяем, если данные продолжают лететь и место еще есть
      if (fifo_cnt)
        // Если данные в FIFO остались, а места в RAM нет -
        // ВЫКЛЮЧАЕМ прерывания приема (Backpressure), пока основной цикл не освободит место
        CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA);
      WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR); // Очищаем флаги. 
      if (need_sync) uart_rx_buf_end = buf_end;      // Синхронизируем индекс
      }
     else if (uart_intr_status & UART_RXFIFO_OVF_INT_ST) {
      // Marlin прислал слишком много, а мы спали. Чистим FIFO полностью.
      SET_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST);
      CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST);
      WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_OVF_INT_CLR);
      errCode |= ERR_UART_RX_OVF; //глобальный признак ошибки переполнения Rx FIFO
      }
     else if (uart_intr_status & UART_FRM_ERR_INT_ST)
      WRITE_PERI_REG(UART_INT_CLR(UART0), UART_FRM_ERR_INT_CLR);
     else if (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST) {
      uint32_t buf_beg = uart_tx_buf_beg; // переходим на регистры
      uint32_t buf_end = uart_tx_buf_end;
      uint32_t i, tx_fifo_len, tx_fifo_space;
      bool need_sync = false;
      do {
        tx_fifo_len = ((READ_PERI_REG(UART_STATUS(UART0)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT);
        tx_fifo_space = UART_FIFO_MAX_LEN - tx_fifo_len;
        for (i = 0; ((i < tx_fifo_space) && (buf_beg != buf_end)); i++, buf_beg++)
          WRITE_PERI_REG(UART_FIFO(UART0), uart_tx_buf[buf_beg & UART_BUF_RING_MASK]); // keep ring behavior
        if (i != 0)
          need_sync = true ;
        } while (i);  // повторяем, может часть Tx FIFO успела освободиться
      // Если в кольцевом буфере больше нет данных для отправки - выключаем это прерывание.
      // Иначе оно будет долбить CPU вечно, так как FIFO всё еще "пустоват" (ниже порога).
      if (buf_beg == buf_end)
        CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);  // disable tx_empty intr if ring tx buffer is empty
      WRITE_PERI_REG(UART_INT_CLR(UART0), UART_TXFIFO_EMPTY_INT_CLR);         // clear txfifo_empty intr flag
      if (need_sync) uart_tx_buf_beg = buf_beg;      // Синхронизируем индекс
      }
	//	uart_intr_status = READ_PERI_REG(UART_INT_ST(UART0));                     // update intr status
	//	}	while (uart_intr_status);                                               // while intr status is not cleared
  //MEMW() синхронизация и ожидание отработки fifo-write на шине CPU
  asm volatile("memw" : : : "memory");    // __builtin_xtensa_rsync();
}

inline char uart_r_touch(void) {
  return ((uart_rx_buf_beg == uart_rx_buf_end)? '\x0': uart_rx_buf[uart_rx_buf_beg & UART_BUF_RING_MASK]);
}

char uart_r_chr(bool flush) {
//inline char __uart_r_chr(bool flush) {
  char res = '\x0';
  uint32_t buf_beg = uart_rx_buf_beg;
  uint32_t buf_end = uart_rx_buf_end; // снимок, т.к. прерывание может изменить end
  if (buf_beg != buf_end) {
    res = uart_rx_buf[buf_beg & UART_BUF_RING_MASK];
    uart_rx_buf_beg = buf_beg + 1;    // Мы прочитали байт
    if (flush)
      // Включаем прерывания только если это одиночный вызов, а не из цикла считывания строки
      SET_PERI_REG_MASK(UART_INT_ENA(UART0), (UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA)); 
    }
  return res;
}
/*
char uart_r_chr(void) {
  //ETS_UART_INTR_DISABLE(); // Запрещаем прерывания UART
  char res = __uart_r_chr();
  //ETS_UART_INTR_ENABLE();  // Разрешаем обратно
  return res;
}
*/
inline uint32_t uart_r_avail(void) {
  return (uart_rx_buf_end - uart_rx_buf_beg);
}
/*
uint32_t uart_r_str(char* dst, uint32_t len) {
  uint32_t i = 0;
  // Считаем доступное количество байт один раз для цикла
  uint32_t avail = uart_r_avail();
  if (len > avail) len = avail;
  for (; i < len; i++)
    *dst++ = __uart_r_chr(false); // Вызываем БЕЗ включения прерываний в каждой итерации
  if (i != 0)
    // Если хоть один байт был получен - "будим" прием в железе
    SET_PERI_REG_MASK(UART_INT_ENA(UART0), (UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA)); 
  return i;
}
*/
uint32_t uart_w(char* src, uint32_t len, bool flush) {
  if (uartWxStop) return 0;
  int sz = 0;
  bool srcFlash = ((uint32_t)src >= 0x40000000);
  if (src) {
    // Автоматическое определение длины строки, если len равен 0
    if (!len) len = (srcFlash) ? strlen_P((const char*)src) : strlen(src);
    // Определяем, нужно ли вести логирование для этой сессии
    bool doLog = ((ctrl & UART_SEND) && (len));
    char* logBuf = (char*)netSendBuf;
    int maxLogSz = 0;
    if (doLog)
      // Ограничиваем размер лога под сетевой пакет
      maxLogSz = (len <= (NET_DATA_MAX - 4)) ? len : (NET_DATA_MAX - 4); // strlen("L:,") = 3 + '\n'
    // ГЛАВНЫЙ СИНХРОННЫЙ ЦИКЛ ОПТИМИЗАЦИИ
    for (; sz < (int)len; sz++) {
      uint32_t buf_beg = uart_tx_buf_beg; 
      uint32_t buf_end = uart_tx_buf_end; 
      if ((buf_end - buf_beg) < UART_BUF_SIZE) {
        // Одиночное чтение байта из FLASH или RAM в регистр CPU
        char c = (srcFlash)? pgm_read_byte(src + sz): src[sz];
        // 1. Толкаем байт в кольцевой буфер UART
        uart_tx_buf[buf_end & UART_BUF_RING_MASK] = c;
        uart_tx_buf_end = buf_end + 1;
        // 2. ПАРАЛЛЕЛЬНО пишем этот же байт в лог-буфер (пока не превышен лимит пакета)
        if (doLog && (sz < maxLogSz)) logBuf[sz] = c;
        }
       else
        break; // Места в кольцевом буфере TX нет, Marlin-очередь полна
      } // for (; sz < len; sz++) {
    // 3. ФИНАЛИЗАЦИЯ И ОТПРАВКА СФОРМИРОВАННОГО ЛОГА
    if (doLog && (sz > 0)) {
      // При необходимости корректируем размер лога
      if (sz > maxLogSz) sz = maxLogSz;
      for (int i = 0; (i < sz); i++)
        if (logBuf[i] == '\n') logBuf[i] = '\\';    // визуализируем переводы строки
      logBuf[sz++] = '\n';
      // Передаем RAM-копию в сетевую очередь WebSockets (возможен отказ, если кольцевой буфер забит)
      if (!netQuePut((uint8_t*)logBuf, sz, (char*)PSTR("L:>"), socket.clWS_ID)) {
        char warn[] = "\n";
        netQuePut((uint8_t*)warn, 1, (char*)PSTR("L:,UART TX log skipped"), socket.clWS_ID);
    } } }
  // Если данные были добавлены или запрошен принудительный flush
  // Включаем прерывание на "пустоту" TX FIFO, чтобы начать/продолжить физическую отправку чипом
  // дополнительное условие - сброшенное состояние таймера задержки
  if (!(txTimer))
    if ((sz) || flush)
      SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
  return sz;
}

inline void uartOff() {
  // 1. Программный ключ для TX, чтобы блокровать uart_w()
  uartWxStop = true;
  // 2. Полностью отключаем любые RX-прерывания в маске UART0
  // Прерывание пустого буфера TX (UART_TXFIFO_EMPTY_INT_ENA) НЕ трогаем!
  CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), 
      UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA |
      UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA);
  // 3. Отключаем функцию аппаратного тайм-аута приема
  CLEAR_PERI_REG_MASK(UART_CONF1(UART0), UART_RX_TOUT_EN);
  // 4. Очищаем накопившиеся флаги прерываний по RX, чтобы спать спокойно
  WRITE_PERI_REG(UART_INT_CLR(UART0), 
      UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR |
      UART_RXFIFO_OVF_INT_CLR | UART_FRM_ERR_INT_CLR);
  // ВНИМАНИЕ: Строку со сбросом FIFO (UART_RXFIFO_RST) мы отсюда НАВСЕГДА УБИРАЕМ.
  // Пускай буфер переполняется входящим трафиком, нам это больше не мешает.
  gAnswer_idx = 0; anchorIdx = 0;           // сбрасываем буфер полностью
}

inline void uartOn() {
  // 1. МЫ ПРОСНУЛИСЬ. На пине RX и в FIFO сейчас гарантированно скопился мусор.
  // Только СЕЙЧАС мы кратковременно «дергаем» сброс FIFO, чтобы мгновенно его очистить.
  SET_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST);
  CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST); 
  // 2. Сбрасываем также программный кольцевой буфер RAM, так как данные были утеряны
  uart_rx_buf_beg = 0; uart_rx_buf_end = 0;
  // 3. Возвращаем обратно функцию аппаратного тайм-аута приема
  SET_PERI_REG_MASK(UART_CONF1(UART0), UART_RX_TOUT_EN);
  // 4. Превентивно гасим флаги ошибок и переполнения, которые взвелись от мусора
  WRITE_PERI_REG(UART_INT_CLR(UART0), 
      UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR |
      UART_RXFIFO_OVF_INT_CLR | UART_FRM_ERR_INT_CLR);
  // 5. Включаем маску RX прерываний обратно в работу
  SET_PERI_REG_MASK(UART_INT_ENA(UART0), 
      UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA |
      UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA);
  uartWxStop = false;
  gAnswer_idx = 0; anchorIdx = 0;           // сбрасываем буфер полностью
}

void clear_uart(void) {
  ETS_UART_INTR_DISABLE();                                                // global disable uart interrupts
  for (uint32_t i = 0; i < 1; i++) {                                      // 0=UART0, 1=UART1
    uart_on[i] = false;                                                   // up-flag: set off
    WRITE_PERI_REG(UART_INT_ENA(i), 0);                                   // disable all uart event interrupts
    WRITE_PERI_REG(UART_INT_CLR(i), 0xffff);                              // clear all uart event interrupt flags
    CLEAR_PERI_REG_MASK(UART_CONF1(i), UART_RX_TOUT_EN);                  // disable rx time-out function
    SET_PERI_REG_MASK(UART_CONF0(i), UART_RXFIFO_RST | UART_TXFIFO_RST);  // reset rx & tx FIFOs
    CLEAR_PERI_REG_MASK(UART_CONF0(i), UART_RXFIFO_RST | UART_TXFIFO_RST);// return FIFOs to work state
    }
  ETS_UART_INTR_ATTACH((int_handler_t)(nullptr),  NULL);                  // detach interrupt handler, set (= nullptr)
                                           
  uart_rx_buf_beg = uart_rx_buf_end = 0;                                  //clear rx buffer
  uart_tx_buf_beg = uart_tx_buf_end = 0;                                  //clear tx buffer
}

void serial2uart(bool enaInt) {
  if (Serial) {
    Serial.flush();
    Serial.end();
    while(Serial) {};
    }
  clear_uart();
  uart_div_modify(UART0, (APB_FREQ / UART0_BAUDRATE));                            // set baudrate prescaler
  system_set_os_print(0);
  WRITE_PERI_REG(UART_CONF0(UART0), (UART_PARITY_EN & 0x00) |                         // DISABLE PARITY
                                (UART_PARITY & 0x00) |                                // PARITY NONE
                                ((0x01 & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S) | // 1 STOP BIT
                                ((0x03 & UART_BIT_NUM) << UART_BIT_NUM_S));           // 8 BIT DATA
  //clear rx and tx fifo,not ready
  SET_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST | UART_TXFIFO_RST);      // reset FIFOs
  CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST | UART_TXFIFO_RST);    // return to work state
  //set rx_full rx_tout tx_empty triggers, enable rx_tout function
  WRITE_PERI_REG(UART_CONF1(UART0),
                 ((100 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) | // set 100 (0..127)
                 (2 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |             // set 2   (0..127)
                 UART_RX_TOUT_EN |                                              // enable rx time-out function
                 ((MY_TXFIFO_EMPTY_THRHD & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S)); // set 20   (0..127)
  ETS_UART_INTR_ATTACH(uart0_intr_handler,  NULL);                              // attach the handler
  //--- SET GPIO1/GPIO3 FUNCTION TO TX/RX ---
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);               // set PULLUP to OFF
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);  //GPIO 3 -> (RX) (for swap the pin to a GPIO -> pinMode(3, FUNCTION_3)?)
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);               // set PULLUP to OFF
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);  //GPIO 1 -> (TX) (for swap the pin to a GPIO -> pinMode(1, FUNCTION_3)?)
  //uart_on[0] = true;                                                            // set up-flag
  if (enaInt)
    WRITE_PERI_REG(UART_INT_ENA(UART0),                                         // enable interrupts for 4 types of uart rx events
                      UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA |
                      UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA);
   else
    // Если запрещено (для безопасного старта) — пишем ЖЕСТКИЙ НУЛЬ. 
    // Все маски прерываний UART0 закрыты, ядро ESP полностью защищено!
    WRITE_PERI_REG(UART_INT_ENA(UART0), 0);
  // Очищаем текущие триггеры флагов, чтобы убрать фантомные зацепки
  WRITE_PERI_REG(UART_INT_CLR(UART0), 0x1ff);

  ///*** UART1 init for TX only ***/
  ///* UART1 will be used for generating beep sounds only */
  ///* interrupts doesn't needed */
  //CLEAR_PERI_REG_MASK(UART_CONF0(UART1), UART_TXFIFO_RST);    // return to work state
  //uart_div_modify(UART1, UART_CLK_FREQ / 2048);
  //uart_set_freq(UART1);                                       // def == 2048
  //WRITE_PERI_REG(UART_CONF0(UART1), (UART_PARITY_EN & 0x00) |                         //DISABLE PARITY
                                //(UART_PARITY & 0x00) |                                // PARITY NONE
                                //((0x01 & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S) | // 1 STOP BIT
                                //((0x03 & UART_BIT_NUM) << UART_BIT_NUM_S));           // 8 BIT DATA
  //PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
  //PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD);  // UART1_TXD
  //uart_on[1] = true;

  // Разрешаем прерывания глобально на уровне процессора
  ETS_UART_INTR_ENABLE();                                                       // global enable uart interrupts
  //log_write("\nUART_0 is **UP** at 9600\n");
      //#if defined (DBG_LOG)
      //  uart_w(pgm2chr(UART_MSG_0), 0, true);
      // #endif
}
//--------------------------///
