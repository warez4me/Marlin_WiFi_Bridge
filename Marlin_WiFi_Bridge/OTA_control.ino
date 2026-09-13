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

void onOTAStart() {
  //uartWxStop = false;
  //WITH_FLAG_OFF(UART_SEND, (uart_w((char*)PSTR("M117 OTA Update...\n"))));  // Сообщаем на LCD принтера
  //uartWxStop = true;
  netQuePut(NULL, 0, (char*)PSTR("L:,OTA FW update. Just wait..\n"));  // Сообщение в лог всем WS клиентам
  bridgeState = SYS_OTA;    // переключаем систему в полностью OTA-dedicated режим
}

void onOTAProgress(uint32_t current, uint32_t total) {
  static uint32_t lastP = 200;
  uint32_t p = ((total)?
                ((current < total)? ((uint32_t)((uint64_t)(current * 100) / total)): 100):
                0);
  if (p % 10 == 0 && p != lastP) {
    lastP = p;
    // Обновляем экран принтера каждые 10%, чтобы не спамить UART
    int32_t len = snprintf_P((char*)packet, sizeof(packet), PSTR("M117 OTA: %d%%\n"), p);
    uartWxStop = false;
    WITH_FLAG_OFF(UART_SEND, (uart_w((char*)packet, len)));
    uartWxStop = true;
    }
}

void writeFlash() {
  if (errCode != ERR_NO_ERRORS) return;
  static uint32_t endReqT = 0;
  uint32_t tNow = millis();
  if (ch_cnt) {
    // фиксируем время начала передачи этого чанка
    chunks[ch_out].txMark = ((tNow)? tNow: 0xFFFFFFFF);
    // 1. Прожигаем чанк из кольцевого буфера во флеш
    uint32_t written = Update.write(chunks[ch_out].data, chunks[ch_out].len);
    if (written == chunks[ch_out].len) {
      // 2. Раз флеш успешно записан — учет и запрос на сл.чанк
      countData(written);
      socket.progress += written;                   // Прибавляем к прогрессу (ЯВНО)
      fData.txTotal += written; fData.txSeq += 1;
      endReqT = tNow;
      //onOTAProgress(socket.progress, socket.fSize); // вывод прогресса на LCD принтера
      }
     else errCode |= ERR_OTA_FLASH_ERR;
    }
   else if (socket.progress == socket.fSize) {
    // на вс.случай после получения всего файла запрашиваем "N:xxx" -> "END:xxx"
    if (!(ch_req) && ((tNow - endReqT) > 50)) {
      endReqT = tNow;
      int32_t len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("N:%d\n"), (socket.clGroup_ID & 0xFFFF));
      netQuePut_cid(packet, -len, socket.clWS_ID);  // == urgent
    } }
}

bool setOTA() {
  int len;
  bool res = true;
  if (socket.fSize == 0) {
    res = false;
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                  "L:!OTA File is empty or not selected\n"));
    }
  // 1. Проверяем МИНИМАЛЬНО реалистичный размер файла
  // Берем 75% (3/4) от размера текущей запущенной программы
  uint32_t tsholdSize = ESP.getSketchSize();
  tsholdSize -= (tsholdSize >> 2);
  if (res && (socket.fSize < tsholdSize)) {
    res = false;
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                  "L:!OTA File size (%d b) is too small!\n"
                  "L:!System requires at least %d b.\n"), 
                  socket.fSize, tsholdSize);
    }
  // 2. Вычисляем доступное пространство во флеш-памяти (МАКСИМАЛЬНЫЙ порог)
  // в соответствии с принятыми схемами распределения памяти:
  // для 4М модулей : (((4096 - 16) - 2048) / 2) - 4, где 16 == SDK, 2048 == FS, 4 - EEPROM
  // для 1М модулей : (((1024 - 16) - 64) / 2) - 4, где 16 == SDK, 64 == FS, 4 - EEPROM
  tsholdSize = (ESP.getFlashChipRealSize() > (1024 * 1024))? ((1016 - 4) << 10): ((464 - 4) << 10);
  // Проверка 3: Защита от слишком большого файла
  if (res && (socket.fSize > tsholdSize)) {
    res = false;
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                  "L:!OTA File size (%d b) is too large!\n"
                  "L:!Up to %d b. is allowed.\n"), 
                  socket.fSize, tsholdSize); }
  if (res) {
    // Отключаем RX прерывания UART, loop() без getMarlin(), чтобы процессор не отвлекался от Wi-Fi
    uartOff();
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                    "L:,Ready to flash \"%s\"\n"
                    "L:,Tap \"Upload\" to start\n"),
                    socket.fName); }
  netQuePut(packet, -len); // 3. Сообщение в лог
  return res;
}
