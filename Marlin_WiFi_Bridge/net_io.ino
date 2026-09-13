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

inline uint32_t ringBufferPut(uint32_t beg_idx, const uint8_t* src, uint32_t size) {
  // Безопасность превыше всего: если копировать нечего, мгновенно возвращаем исходный индекс
  if (size == 0) return beg_idx;
  bool fromProgmem = IS_PROGMEM(src);
  uint32_t end_idx = beg_idx + size;
  uint32_t realStart = beg_idx & NET_BUF_RING;
  // проверка (по маске) пересечения границы буфера
  if ((beg_idx & ~NET_BUF_RING) == ((end_idx - 1) & ~NET_BUF_RING)) {
    // Пакет ляжет монолитно в физической памяти — заполняем одним memcpy
    if (fromProgmem) memcpy_P(&netBuf[realStart], src, size);
     else memcpy(&netBuf[realStart], src, size); }
   else {
    // Пакет ляжет раздельно в физической памяти — делим на 2 сегмента
    uint32_t seg1_size = NET_BUF_SIZE - realStart;
    uint32_t seg2_size = size - seg1_size;
    if (fromProgmem) {
      memcpy_P(&netBuf[realStart], src, seg1_size);
      memcpy_P(&netBuf, src + seg1_size, seg2_size); }
     else {
      memcpy(&netBuf[realStart], src, seg1_size);
      memcpy(&netBuf, src + seg1_size, seg2_size); }
    }
  return end_idx;
}

inline uint32_t ringBufferGet(uint32_t beg_idx, uint8_t* dst, uint32_t size) {
  // Вычисляем индекс конца чтения текущего пакета
  uint32_t end_idx = beg_idx + size;
  uint32_t realStart = beg_idx & NET_BUF_RING;
  // проверка (по маске) пересечения границы буфера
  if ((beg_idx & ~NET_BUF_RING) == ((end_idx - 1) & ~NET_BUF_RING))
    // Пакет лежит монолитно в физической памяти — забираем одним memcpy
    memcpy(dst, &netBuf[realStart], size);
   else {
    // Пакет разорван концом кольцевого массива — собираем из двух сегментов
    uint32_t seg1_size = NET_BUF_SIZE - realStart;
    uint32_t seg2_size = size - seg1_size;
    memcpy(dst, &netBuf[realStart], seg1_size);
    memcpy(dst + seg1_size, netBuf, seg2_size); }
  // возвращаем локальный курсор чтения
  return end_idx; 
}

bool netQueFlush(uint32_t flushSize, bool countOnly) {
  if ((NET_BUF_SIZE - (netBufEnd - netBufBeg)) < flushSize) {
    if (countOnly) return false;
    // 1. Пытаемся освободить место, если буфер забит
    netQueSend();
    if ((NET_BUF_SIZE - (netBufEnd - netBufBeg)) < flushSize) {
      netBufEnd = netBufBeg; errCode |= ERR_NET_BUSY;
      return false;
    } }
  return true;
}

bool netQuePut(uint8_t* msg, int32_t msgSize, char* preMsg, uint8_t c) {
  if (wsMap == MAP_NOONE) {
    netBufBeg = netBufEnd; return true; } // сбрасываем очередь, если клиентов не осталось
  if (c == CID_NOONE) return true;         // если безадресный пакет
  if ((c != CID_ALL) && !(wsMap & (1 << c))) return true; // если вдруг адресат отключился
  uint32_t mSize = ((msg)? ((msgSize >= 0)? msgSize: -msgSize): 0);
  uint32_t preSize = 0;
  if (preMsg && *preMsg) preSize = IS_PROGMEM(preMsg) ? strlen_P(preMsg) : strlen(preMsg);
  if ((preSize + mSize) == 0) return true;
  if ((preSize + mSize) > NET_DATA_MAX) return false;
  uint32_t putSize = sizeof(netData_t) + T_STR_SIZE + preSize + mSize;
  // 1. Пытаемся освободить место, если буфер забит
  if (!netQueFlush(putSize, ((msg >= netSendBuf) && (msg < (netSendBuf + SEND_BUF_SIZE))))) return false;
  netData_t header;
  header.time = millis();
  header.size = T_STR_SIZE + preSize + mSize;
  header.cMap = ((c == CID_ALL)? CID_ALL: (1 << c));
  header.cMap |= ((msgSize >= 0)? 0: ~CID_ALL);        // помечаем срочный пакет
  // в начале каждого сообщения будет метка времени
  // получаем текущее время NTP и распечатываем в буфер
  struct timeval tv;
  gettimeofday(&tv, nullptr); // Получаем секунды tv.tv_sec и микросекунды tv.tv_usec
  time_t now = tv.tv_sec;
  struct tm* timeinfo = localtime(&now);
  if (timeinfo->tm_year > (2020 - 1900)) {
        // Печатаем чч:мм:сс в буфер
    size_t t_size = snprintf_P(tBuf, sizeof(tBuf), PSTR("%02d:%02d:%02d"), 
                              timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        // Печатаем микросекунды в тот же буфер сразу после строки времени
    snprintf_P(tBuf + t_size, sizeof(tBuf) - t_size, PSTR(".%06lu"), tv.tv_usec);
    }
   else // Резервный Uptime до синхронизации, ровно 12 символов текста
    snprintf_P(tBuf, T_STR_SIZE, PSTR("%-12lu"), header.time);
  // === КОНЕЦ БЛОКА РЕАЛЬНОГО ВРЕМЕНИ NTP ===
  tBuf[T_STR_SIZE - 1] = ':'; // ставим разделитель, ограничиваем 3-мя символами миллисекунд
  uint32_t putIdx;
  if (msgSize >= 0)           // срочное помещаем перед началом, обычное - после конца тек. содержимого
    putIdx = netBufEnd;
   else {
    netBufBeg -= putSize; putIdx = netBufBeg; }
  // 1. Копируем бинарную структуру заголовка (размер фиксирован)
  putIdx = ringBufferPut(putIdx, (uint8_t*)&header, sizeof(netData_t));
  // 2. Копируем строку метки времени (длина фиксирована)
  putIdx = ringBufferPut(putIdx, (uint8_t*)tBuf, T_STR_SIZE);
  // 3. Копируем префикс сообщения, если он есть (из PROGMEM или RAM)
  if (preSize > 0)
    putIdx = ringBufferPut(putIdx, (uint8_t*)preMsg, preSize);
  // 4. Копируем само тело сообщения (основной payload)
  if (mSize > 0)
    putIdx = ringBufferPut(putIdx, msg, mSize);
  // Фиксируем конец очереди
  if (msgSize >= 0) netBufEnd = putIdx;
  return true;
}

void netQueSend() {
  if (wsMap == MAP_NOONE) {
    netBufBeg = netBufEnd; return; } // сбрасываем очередь, если клиентов не осталось
  uint32_t getIdx = netBufBeg, endIdx = netBufEnd;
  int32_t queSize = (int32_t)(endIdx - getIdx);
  // входим в цикл выгрузки накопленных сообщений только если очередь не пустая
  uint32_t yieldCounter = 0;                              // Счетчик пакетов для вызова yield()
  bool statOk = false;
  while (queSize > 0) {
    if (!statOk) {
      stat(MTR_NET_BUF_SIZE, queSize); statOk = true; }   // статистика в начале цикла выгрузки
    if (queSize < (int32_t)(sizeof(netData_t))) {
      // ошибка, значения индексов не соответствуют объему данных
      netBufEnd = netBufBeg; errCode |= ERR_NET_QUE_ERR;  // Сбрасываем всё, чтобы не уйти в бесконечный цикл
      return;
      }
    netData_t header;
    uint8_t* h_ptr = (uint8_t*)&header;
    // Побайтовое извлечение заголовка (безопасно от Alignment Exception)
    for (size_t i = 0; i < sizeof(netData_t); i++, getIdx++)
      h_ptr[i] = netBuf[getIdx & NET_BUF_RING];
    header.cMap &= (wsMap | ~CID_ALL);             // обрезаем по тек.общей карте, сохраняем флаг срочности
    if (queSize < (int32_t)(sizeof(netData_t) + header.size)) {
      // (маловероятно , но..) буфер "испортился" - сбрасываем
      netBufEnd = netBufBeg; errCode |= ERR_NET_QUE1_ERR;
      return;
      }
    if (((header.cMap & CID_ALL) == MAP_NOONE) || (header.size > sizeof(netSendBuf))) {
      //нет подключенных клиентов или нет адресата или превышение размера - пропускаем сообщение, очищаем очередь
      getIdx += header.size; netBufBeg = getIdx; queSize -= (sizeof(netData_t) + header.size);
      continue;
      }
    getIdx = ringBufferGet(getIdx, netSendBuf, header.size);  // копируем строку времени и сообщение во вр.буфер
    uint32_t goneTime = millis() - header.time;
    int active = ((socket.clGroup_ID & 0xFFFF)? 2: 0);
    uint8_t sendMap = header.cMap, i = ((active)? socket.clWS_ID: 0); // при наличии активиста начинаем с него
    while ((i < WEBSOCKETS_SERVER_CLIENT_MAX) && ((sendMap & CID_ALL) != MAP_NOONE)) {
      if (sendMap & (1 << i)) {               // Если бит клиента установлен в общей карте и в карте передачи
        sendMap &= ~(1 << i);                 // каждый установленный бит в карте отрабатывается только раз за цикл
        // ПОДКАЧКА WDT И СТЕКА: Точка максимальной сетевой нагрузки
        // Сбрасывает таймеры и дает LwIP протолкнуть буферы прямо во время итерации
        // вызываем yield() раз в 4 пакета
        if (++yieldCounter >= 4) {
          ESP.wdtFeed(); yield(); 
          yieldCounter = 0;
          // Защита: проверяем, не сбежали ли клиенты за время yield()
          if (wsMap == MAP_NOONE) {
            netBufBeg = netBufEnd; return; }
          }
        uint32_t wsSendT = millis();
        bool wsRC = webSocket.sendBIN(i, netSendBuf, header.size);
        stat(MTR_WS_LOOP_TIME, (millis() - wsSendT));
        //if (webSocket.sendBIN(i, netSendBuf, header.size)) { // если сообщение было успешно отправлено по адресу (i)
        if (wsRC) { // если сообщение было успешно отправлено по адресу (i)
          header.cMap &= ~(1 << i);           // убираем адрес из карты передачи
          if (active == 2) {
            // сюда можем попасть только в начале цикла, когда i == socket.clWS_ID
            (header.cMap & ~CID_ALL)? stat(MTR_UCAST_GONE, goneTime): stat(MTR_BCAST_GONE, goneTime);
            active -= 1; }
           else if (active == 0) {
            // сюда попадаем после первой удачной отправки, когда нет активиста
            stat(MTR_BCAST_GONE, goneTime); active = -1; }
        } }
      if (active > 0) { // активиста отработали в первую очередь: =1 после усп.отправки, =2 если отправки не было
        active -= 2; i = ((i)? 0xFF: 0); } // продолжаем цикл через (0xFF -> 0), если активист был ненулевой
      i += 1;
      }
    if (header.cMap & CID_ALL) {
      // еще не всем клиентам передано
      if (header.cMap & ~CID_ALL) {
        // это срочное сообщение :
        // - просто возврат без сдвига индексов до следующего loop()
        // - если более 100 мсек не получается передать - сброс индексов, возврат с ошибкой
        if (goneTime > 100) {
          netBufBeg = netBufEnd; errCode |= ERR_NET_FREEZE; }
        return;
        }
      // это обычное сообщение :
      // < 20 мсек для всех и < 100 мсек для активиста - возврат до сл. loop()
      // > 100 мсек - сбрасываем с подсчетом, проходим дальше к следующему сообщению
      if ((goneTime < 20) || ((goneTime < 100) && (socket.clGroup_ID & 0xFFFF) && (header.cMap & (1 << socket.clWS_ID))))
        return;
      if ((socket.clGroup_ID & 0xFFFF) && (header.cMap & (1 << socket.clWS_ID)))
        netDrops += 1;
      } // if (header.cMap & CID_ALL)
    netBufBeg = getIdx; queSize -= (sizeof(netData_t) + header.size);
    } // while (queSize > 0)
}

void netQueClean() {
  if (wsMap == MAP_NOONE) netBufBeg = netBufEnd;              // сбрасываем очередь, если клиентов не осталось
  if (netBufBeg == netBufEnd) return;
  uint32_t queIdx = netBufBeg, endIdx = netBufEnd;
  netData_t header;
  uint8_t* h_ptr = (uint8_t*)&header;
  const uint32_t mapOffset = offsetof(netData_t, cMap);       // Вычисляем смещение cMap внутри структуры netData_t
  int32_t cleanSize = (int32_t)(endIdx - queIdx);
  while (cleanSize > 0) {
    // 1. актуализируем cMap в буфере по известному смещению наложением актуальной общей карты подключений
    netBuf[(queIdx + mapOffset) & NET_BUF_RING] &= (wsMap | ~CID_ALL);  // header.cMap &=
    // 2. вычитываем структуру заголовка целиком
    for (uint32_t i = 0; i < sizeof(netData_t); i++, queIdx++)
      h_ptr[i] = netBuf[queIdx & NET_BUF_RING];
    // переходим к следующему сообщению
    cleanSize -= (sizeof(netData_t) + header.size);
    // Предохранитель от "бесконечного" цикла при порче данных
    if ((cleanSize < 0) || (header.size > sizeof(netSendBuf))) {
      // (маловероятно , но..) буфер "испортился" - сбрасываем очередь
      netBufEnd = netBufBeg; errCode |= ERR_NET_QUE1_ERR;
      break;
      }
    queIdx += header.size;
    } // while (cleanSize > 0) {
}
