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

#include <EEPROM.h>
#include "WiFi_BFT.h"

void saveEEPROM() {
  // Заполняем CRC16 структуры без самого поля cfg.crc
  cfg.crc = calculateCRC16((const uint8_t*)&cfg, sizeof(Config) - sizeof(cfg.crc));
  // Бинарная запись put-операцией в последний сектор флэш-памяти
  EEPROM.begin(512);  //(sizeof(Config));
  EEPROM.put(0, cfg);
  EEPROM.commit();
  EEPROM.end();
}

// =========================================================================
// СБРОС НАСТРОЕК вызывается из парсера MQTT mqttSocketHandler() или Веб-терминала myBridgeCmd()
// =========================================================================
void clearConfigInEEPROM() {
  EEPROM.begin(512);  //(sizeof(Config));
  uint32_t zero_magic = 0;          // Затираем только магическое число нулями
  EEPROM.put(0, zero_magic);
  EEPROM.commit();
  EEPROM.end();
}

// =========================================================================
// РАСЧЕТ КОНТРОЛЬНОЙ СУММЫ CRC16 CCITT (Без таблиц — экономия IRAM)
// =========================================================================
uint16_t calculateCRC16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;                            // Стартовое значение
  for (size_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;  // Полином CCITT
       else crc <<= 1;
      }
    }
  return crc;
}

bool loadConfigFromEEPROM() {
  EEPROM.begin(512);  //(sizeof(Config));
  EEPROM.get(0, cfg);
  EEPROM.end();
  uint16_t valid_crc = ~cfg.crc;  // присвоение заведомо неравного значения
  // 1. Проверяем сигнатуру первого запуска
  if (cfg.magic == CONFIG_MAGIC)
    // 2. Считаем валидную CRC для прочитанных данных (без учета поля crc)
    valid_crc = calculateCRC16((const uint8_t*)&cfg, sizeof(Config) - sizeof(cfg.crc));
  // 3. Вернет true при равенстве того, что посчитали, с тем, что сохранено в конце структуры
  return (cfg.crc == valid_crc);
}

#define WIFI_PFX_LEN  9
#define WIFI_LIST_REC 42
#define WIFI_LIST_NUM 100
#define WIFI_LIST_LIM (WIFI_LIST_REC * WIFI_LIST_NUM)

// ОТДАЧА СЖАТОЙ СТРАНИЦЫ CONFIG (HTTP_GET)
void handleGetConfigPage() {
  // ЗАЩИТА: Если мы подключены к роутеру (STA) — ресурса /config для сети не существует!
  if (WiFi.getMode() == WIFI_STA) {
    httpServer.send(404, PSTR("text/plain"), PSTR("Not Found")); return; }
  if (httpServer.hasArg("l")) {
    // Если есть GET-аргумент "l"
    // Проверяем наличие списка сетей
    if ((gAnswer_idx > WIFI_LIST_LIM) || (anchorIdx != WIFI_LIST_LIM) || (gAnswer_buf[gAnswer_idx - 1] != '\x02'))
      gAnswer_idx = 0;
    // Нам нужно дописать параметры в конец готового списка сетей.
    // Используем текущий gAnswer_idx как точку старта.
    int len = gAnswer_idx; 
    // Дописываем системный Anti-CSRF токен безопасности из глобального sessionID (он уже 100% готов)
    len += snprintf_P((char*)gAnswer_buf + len, GANSWER_BUF_SIZE - len, PSTR("x""\x01""%u""\x02"), sessionID);
    // Если конфигурация уже существует, дописываем остальные поля
    if (cfg.magic == CONFIG_MAGIC) {
      len += snprintf_P((char*)gAnswer_buf + len, GANSWER_BUF_SIZE - len, PSTR("s""\x01""%s""\x02"), cfg.wifi_ssid);
      if (*cfg.mqtt_ip) {
        len += snprintf_P((char*)gAnswer_buf + len, GANSWER_BUF_SIZE - len, PSTR("i""\x01""%s:%u""\x02""d""\x01""%s""\x02"), 
                          cfg.mqtt_ip, cfg.mqtt_port, cfg.device_id); // Для MQTT - "IP:PORT" и device_ID всегда идут в паре
        if (*cfg.mqtt_user) len += snprintf_P((char*)gAnswer_buf + len, GANSWER_BUF_SIZE - len, PSTR("u""\x01""%s""\x02"), cfg.mqtt_user);
        if (*cfg.ha_prefix) len += snprintf_P((char*)gAnswer_buf + len, GANSWER_BUF_SIZE - len, PSTR("h""\x01""%s""\x02"), cfg.ha_prefix);
      } }
    len -= 1;  // исключаем из передачи ненужный '\x02' в конце буфера
    // Отдаем весь буфер (Сети + Системные параметры в конце) в ТЕЛЕ ответа
    httpServer.send(200, (const char*)PSTR("text/plain"), (const char*)gAnswer_buf, len);
    return;
    }
  // Обычный первый запрос — выдаем саму сжатую Zopfli-страницу
  // Говорим браузеру, что контент сжат с помощью GZIP
  httpServer.sendHeader(F("Content-Encoding"), F("gzip"));
  // Отправляем код страницы конфигурации напрямую из PROGMEM
  httpServer.send(200, (const char*)PSTR("text/html"), (const char*)CONFIG_HTML, sizeof(CONFIG_HTML));
  apTime = 900; // продлеваем до ~900 сек. интервал, выделенный для переконфигурации в режиме AP
}

// ПРИЕМ И ПАРСИНГ ДАННЫХ CONFIG (HTTP_POST)
// =========================================================================
// УЛЬТРА-БЫСТРЫЙ НИЗКОУРОВНЕВЫЙ ПАРСЕР-ВАЛИДАТОР IP И ПОРТА
// =========================================================================
uint16_t parseIP(char* ip_str) {
  uint32_t strVal, i;
  for(i = 0; i < 4; i++) {
    if (!getNum(&strVal, &ip_str, 255, false)) break; // считываем октет IP адреса с проверкой диапазона значений
    if (i < 3)
      if (*ip_str++ != '.') break;                    // проверяем разделители октетов
    }
  strVal = 0;
  if (i == 4) {                                       // если успешно прочитали все 4 октета
    strVal = 1883;                                    // значение порта по умолчанию
    if (*ip_str == ':') {                             // проверяем наличие разделителя перед номером порта
      *ip_str++ = '\0';                               // фиксируем конец строки IP и переходим к номеру порта
      getNum(&strVal, &ip_str, 65535, false); }       // считываем номер порта с проверкой диапазона значений
     else *ip_str = '\0';                             // фиксируем конец строки IP и отсекаем возможный мусор
    }
  return (uint16_t)(strVal & 0xFFFF);
}

void handleSaveConfig() {
  apTime = 900; // продлеваем до ~900 сек. интервал, выделенный для переконфигурации в режиме AP
  const char* eMsg = PSTR("Error. Invalid configuration request.");
  // Проверяем, что запрос содержит ВСЕ 8 ключей нашей формы
  if (!httpServer.hasArg("o") || !httpServer.hasArg("s") ||
      !httpServer.hasArg("p") || !httpServer.hasArg("i") ||
      !httpServer.hasArg("u") || !httpServer.hasArg("w") ||
      !httpServer.hasArg("h") || !httpServer.hasArg("d")) {
      httpServer.send(400, (const char*)PSTR("text/plain"), eMsg);
      return; }
  // ПРОВЕРКА ANTI-CSRF ТОКЕНА БЕЗОПАСНОСТИ
  // Сравниваем прилетевшую из формы "o" строку токена с текущим sessionID
  if (sessionID != strtoul(httpServer.arg("o").c_str(), NULL, 10)) {
    httpServer.send(403, (const char*)PSTR("text/plain"), eMsg);
    return; }
  int len = 0;
  eMsg = PSTR("Error. Field ");
  struct { const char* sym; int fieldSeq; } prm[6] = {{"s", 1}, {"u", 4}, {"w", 5}, {"h", 6}, {"d", 7}};
  for (int i = 0; i < 5; i++) {
    if (httpServer.arg(prm[i].sym).length() >= ((i)? STR_LEN_SHORT: (STR_LEN_SHORT + WIFI_PFX_LEN))) {
      if (!(len)) len = snprintf_P((char*)packet, sizeof(packet), eMsg);
      len += snprintf_P((char*)(packet + len), sizeof(packet) - len, PSTR("%d,"), prm[i].fieldSeq);
    } }
  if (len) len += snprintf_P((char*)(packet + len - 1), sizeof(packet) - len + 1, PSTR(" too long (>%d)"), STR_LEN_SHORT - 1);
  if (httpServer.arg("p").length() >= ((STR_LEN_SHORT * 2) - 1)) {
    if (!(len)) len = snprintf_P((char*)packet, sizeof(packet), eMsg);
     else len += snprintf_P((char*)(packet + len), sizeof(packet) - len, PSTR("; "));
    snprintf_P((char*)(packet + len), sizeof(packet) - len, PSTR("2 too long (>%d)"), ((STR_LEN_SHORT * 2) - 1));
    }
  if (len) {
    httpServer.send(400, (const char*)PSTR("text/plain"), (char*)packet);
    uartWxStop = false;
    uart_w((char*)PSTR("M117 Fields oversize..\n"));
    return; }
  // ВАЛИДАЦИЯ КОНТЕНТА
  // SSID обязателен к заполнению всегда
  // из-за мороки с совместимостью возможен приход комбинированной строки SSID + prefix
  // поэтому нужно найти окончание строки SSID и отсечь prefix
  uint32_t sLen = httpServer.arg("s").length();
  if (sLen) {
    strncpy(svcBuf, httpServer.arg("s").c_str(), sLen);
    for (; (sLen); sLen--) {
      char c = svcBuf[sLen - 1];
      // Если уперлись в легитимный символ имени сети (не префикс и не пробел)
      if (c != ' ' && c != '#' && c != '.' && c != '[' && c != ']' && c != '>' && c != '-') {
        svcBuf[sLen] = '\0'; break; } // отсекаем хвост префикса и выходим из цикла
    } }
  if (!(sLen)) {
    httpServer.send(400, (const char*)PSTR("text/plain"), (const char*)PSTR("Error. Missing SSID."));
    uartWxStop = false;
    uart_w((char*)PSTR("M117 Missing SSID..\n"));
    return; }
  uint16_t mqtt_port = 0;
  // поля "i" и "d" для MQTT должны быть либо заполнены, либо пусты!
  if (!!(httpServer.arg("i").length()) ^ !!(httpServer.arg("d").length())) {
    httpServer.send(400, (const char*)PSTR("text/plain"),
                    (const char*)PSTR("Error. Fields 3,7: enter both or none."));
    uartWxStop = false;
    uart_w((char*)PSTR("M117 Bad MQTT setup..\n"));
    return; }
  if (httpServer.arg("i").length()) {   // Если блок MQTT был заполнен — запускаем парсер октетов и порта
    mqtt_port = parseIP((char*)httpServer.arg("i").c_str());
    if (!mqtt_port) {
      httpServer.send(400, (const char*)PSTR("text/plain"), (const char*)PSTR("Error. Invalid MQTT IP address."));
      uartWxStop = false;
      uart_w((char*)PSTR("M117 Bad MQTT IP..\n"));
      return;
    } }
  // НАДЁЖНАЯ ЗАПИСЬ СТРУКТУРЫ
  memset(&cfg, 0, sizeof(Config)); // Очищаем всё нулями
  // Копируем Wi-Fi параметры (безопасный strncpy)
  strncpy(cfg.wifi_ssid, svcBuf, sLen);
  strncpy(cfg.wifi_pass, httpServer.arg("p").c_str(), (STR_LEN_SHORT * 2 - 1));
  // Копируем MQTT, только если блок полностью валиден
  if (mqtt_port) {
    strncpy(cfg.mqtt_ip,   httpServer.arg("i").c_str(), STR_LEN_SHORT - 1); // Чистый IP без порта
    strncpy(cfg.mqtt_user, httpServer.arg("u").c_str(), STR_LEN_SHORT - 1);
    strncpy(cfg.mqtt_pass, httpServer.arg("w").c_str(), STR_LEN_SHORT - 1);
    strncpy(cfg.ha_prefix,        // "h" - по умолчанию подставляется homeassistant
            (httpServer.arg("h").length())? httpServer.arg("h").c_str(): fl2chr(PSTR("homeassistant")),
            STR_LEN_SHORT - 1);
    strncpy(cfg.device_id, httpServer.arg("d").c_str(), STR_LEN_SHORT - 1);
    cfg.mqtt_port = mqtt_port; }
  cfg.magic = CONFIG_MAGIC;       // Заполняем системный маркер
  saveEEPROM();                   // Записываем
  // Финал и уход в перезагрузку
  httpServer.send(200, (const char*)PSTR("text/plain"), (const char*)PSTR("Config saved. Trying new setup.."));
  uartWxStop = false;
  uart_w((char*)PSTR("M117 WiFi bridge reconfigured..\n"));
  delay(200);
  ESP.restart(); 
}

void scanWiFi() {
  // Формируем список доступных WiFi сетей для передачи клиенту при конфигурации в режиме AP
  // Для хранения используем свободный при отключенном UART gAnswer_buf[8192]
  char* ssid = svcBuf;                  // сюда будем помещать получаемую строку SSID при сканировании
  int hasConfig = !!((cfg.magic == CONFIG_MAGIC) && (cfg.wifi_bset));                     // 0|1 , если знаем "свой" BSSID
  if (!(hasConfig)) hasConfig = !!((cfg.magic == CONFIG_MAGIC) && (cfg.wifi_ssid)) << 1;  // 0|2 , если ещё не успели сохранить
  // 3. ЗАБИВАЕМ РЕЗЕРВНУЮ ЗОНУ ПУСТЫШКАМИ "\x01\x02" ЧЕРЕЗ СКОРОСТНУЮ ЗАПИСЬ В LE UINT32_T
  // 11*2 штук по 2 байта = 44 байта.
  // сюда, в начало списка, будет помещена запись для текущей сконфигурированной сети
  // максимальная длина = 9 (префикс) + 32 (SSID) + 1 ('\x02') = 42 = WIFI_LIST_REC
  uint32_t* le_ptr = (uint32_t*)gAnswer_buf;
  for (anchorIdx = 0; anchorIdx < 11; anchorIdx++)
    le_ptr[anchorIdx] = 0x02010201;     // В память ляжет 0x01, 0x02, 0x01, 0x02
  anchorIdx = WIFI_LIST_REC;            // устанавливаем в конец резервной зоны
  // ставим терминаторы в конце резервной зоны и всего буфера для корректной работы последующих strchr
  gAnswer_buf[anchorIdx] = gAnswer_buf[GANSWER_BUF_SIZE - 1] = '\0'; gAnswer_idx = 0;
  // 4. ДВУХЦИКЛОВОЕ СКАНИРОВАНИЕ ЭФИРА
  for (uint8_t k = 0; ((k < 2) && (anchorIdx)); k++) {
    int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
    if (n > 0) {
      if (!(gAnswer_idx)) gAnswer_idx = anchorIdx;  // инициализируем при первом успехе сканирования
      for (int i = 0; ((i < n) && (anchorIdx)); i++) {
        // получаем из Flash-памяти комбинированный префикс из таблицы сигналов sigTable
        int32_t rssi = WiFi.RSSI(i);    // качество сигнала
        const char *prefix = NULL;
        for (uint8_t j = 0; ((j < 4) && !prefix); j++)
          if (rssi > (int8_t)pgm_read_byte(&(sigTable[j].threshold)))
            prefix = (WiFi.encryptionType(i) != ENC_TYPE_NONE)?
                      (const char*)pgm_read_ptr(&(sigTable[j].secured)):
                      (const char*)pgm_read_ptr(&(sigTable[j].openNet));
        if (!(prefix)) continue;        // foolproof
        uint8_t ssidLen;
        // сверяем с текущей сконфигурированной сетью по BSSID (если он сохранен в checkTimers())
        if ((hasConfig == 1) && (memcmp(WiFi.BSSID(i), cfg.wifi_bssid, 6) == 0)) {
          strcpy(ssid, cfg.wifi_ssid); ssidLen = strlen(ssid); hasConfig = 2; }
         else {
          // Получаем длину имени сети. Временный объект String создается и сразу уничтожается в этой строке.
          ssidLen = WiFi.SSID(i).length();
          // Проверяем границы: если имя сети пустое или длина превышает лимит — пропускаем
          if (!ssidLen || (ssidLen >= STR_LEN_SHORT)) continue;
          // Копируем имя сети. Временный объект String создается второй раз, .c_str() отдает байты, 
          // strncpy переносит их в svcBuf, и объект тут же уничтожается на точке с запятой.
          strncpy(ssid, WiFi.SSID(i).c_str(), ssidLen);
          }
        ssid[ssidLen] = '\0';           // гарантируем терминатор для строки имени сети
        if ((hasConfig > 0) && !(strcmp(ssid, cfg.wifi_ssid))) {
          if (hasConfig == 2) {
            // Если нашли текущую сконфигурированную сеть
            // Формируем строку ПРЕФИКС'\x01'SSID'\x02' в начале буфера
            // Она затрет ровно столько пустышек "\x01\x02", сколько займёт запись сети
            memcpy_P(gAnswer_buf, prefix, WIFI_PFX_LEN); memcpy(gAnswer_buf + WIFI_PFX_LEN, ssid, ssidLen);
            gAnswer_buf[WIFI_PFX_LEN + ssidLen] = '\x02'; hasConfig = 3; }
          continue; }
        // Для остальных сетей - сначала проверка на переполнение буфера
        if ((gAnswer_idx + ssidLen + (WIFI_PFX_LEN + 2)) >= WIFI_LIST_LIM) break;
        // Затем проверка на дубликаты Mesh (пропускаем резервную зону)
        // НАДЕЖНЫЙ КАСТОМНЫЙ ПАРСЕР ДУБЛИКАТОВ НА БАЗЕ STRNCMP И ЛИНЕЙНОГО ШАГА
        bool isDuplicate = true;
        // Начинаем сканирование буфера строго с конца резервной зоны ((WIFI_LIST_REC + 1)-й байт)
        anchorIdx = WIFI_LIST_REC; 
        while ((anchorIdx < gAnswer_idx) && (isDuplicate == (anchorIdx == WIFI_LIST_REC))) {
          // Ищем ограничитель '\x02', начиная от (текущей позиции + WIFI_PFX_LEN) байт, пропуская префикс
          char* endOfRecord = strchr((const char*)gAnswer_buf + anchorIdx + WIFI_PFX_LEN, '\x02');
          isDuplicate = (endOfRecord == NULL);
          if (isDuplicate) {
            anchorIdx = 0; break; }       // Защита: выходим полностью, если структура нарушена
          // Вычисляем длину записи, которая уже лежит в буфере
          uint8_t recLen = endOfRecord - ((const char*)gAnswer_buf + anchorIdx) + 1;
          // Если длины предполагаемой новой записи и текущей из буфера совпадают — сравниваем SSID через strncmp
          isDuplicate = ((ssidLen + (WIFI_PFX_LEN + 1)) == recLen);
          if (isDuplicate)
            isDuplicate = (strncmp((const char*)gAnswer_buf + anchorIdx + WIFI_PFX_LEN, ssid, ssidLen) == 0);
          // Смещаем индекс на длину текущей записи в буфере: 
          anchorIdx += recLen;
          } // while
        if (isDuplicate && (gAnswer_idx > WIFI_LIST_REC)) continue; // Уходим на следующую итерацию сканера эфира
        // Дописываем новую уникальную сеть в конец буфера gAnswer_buf
        gAnswer_idx += snprintf_P((char*)gAnswer_buf + gAnswer_idx, 
                                WIFI_LIST_LIM - gAnswer_idx,
                                PSTR("%S%s""\x02"), prefix, ssid);
        } // for (int i = 0; (i < n); ++i)
      } // if (n > 0)
    WiFi.scanDelete();                  // Очищаем кэш текущего круга перед следующим
    if (!(k)) delay(50);                  // Пауза между циклами сканирования для стабилизации АРУ приемника
    } // for k
  if (anchorIdx) {
    // маркируем, если был успешный цикл сканирования + на всякий случай терминируем строку
    gAnswer_buf[gAnswer_idx] = '\0'; anchorIdx = WIFI_LIST_LIM; }
   else gAnswer_idx = 0;
}

void sta2ap(const char* msg) {
  // переключает WiFi в режим AccessPoint (WIFI_AP)
  uartOff(); uartWxStop = false; // запрещаем прием UART RX, разрешаем передачу UART TX
  if (mqttSockData.sockState != MQTT_SOCKET_DISCONNECTED)
    mqttDisconnect();         // Жестко закрываем сокет MQTT, очищая буферы
  if (msg) {
    // Здесь переключаемся из полностью рабочего состояния по MQTT или терминальной команде
    int32_t len = 0;
    for (int i = 0; i < 3; i++)
      len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("H:%u:%d:%d:%d\n"),
                        sessionID, HB_WAIT, SID_SYSTEM,  // сбрасываем текущего активиста
                        ((ctrl & CLIENT_LOG) >> 4));
    len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("%S"), msg);
    netQuePut(packet, -len);  // urgent HB
    netQueSend(); }
   else WiFi.mode(WIFI_STA);  // Гарантируем режим клиента для успешного сканирования
  delay(50);
  webSocket.disconnect();     // Закрываем WS соединения
  wsMap = 0; socket.clGroup_ID = 0; // очищаем данные о подключенных WS клиентах
  WiFi.persistent(false);
  delay(1);
  while (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();        // Только разрываем связь с роутером
    delay(10); }
  scanWiFi();                 // Формируем список доступных WiFi сетей для передачи клиенту
  WiFi.disconnect(true);      // Полностью выключаем клиентский WiFi модуль
  delay(10);                  // Даем lwIP 10 мс на очистку таблиц сокетов
  IPAddress local_IP(192,168,9,1);
  IPAddress gateway(192,168,9,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(1);
  // Переключаем железо в чистый AP
  WiFi.mode(WIFI_AP);
  delay(1);                   // UART RX отключен, переполнения буфера не боимся
  // Разрешаем подключение только 1 устройству на 1-м Wi-Fi канале):
  WiFi.softAP(fl2chr(PSTR("Marlin-Bridge-Setup")), NULL, 1, false, 1);
  delay(1); 
  httpServer.begin();         // Пересаживаем веб-сервер на 192.168.9.1
  wifi_timer = -50; wifiLastCheck = 0; wifiState = WIFI_STATE_AP; // переводим loop() в режим WiFi_AP
  apTime = 120;               // ограничиваем время ожидания коннекта в режиме AP до 120 сек.
}

// ОТДАЧА иконки (HTTP_GET)
void handleGetIcon() {
  httpServer.send(200, PSTR("image/svg+xml"),
                        (WiFi.getMode() == WIFI_AP)?
                          (PSTR("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>"
                                "<text x='50' y='55' font-size='85' text-anchor='middle' dominant-baseline='central'>⚙️</text></svg>")):
                          // здесь отдаем иконку экструдера (mdi:printer-3d-nozzle)
                          (PSTR("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='Purple'>"
                                  "<path d='M7,2H17V8H19V13H16.5L13,17H11L7.5,13H5V8H7V2M10,22H2V20H10A1,1 0 0,0 11,19V18H13V19A3,3 0 0,1 10,22Z'/>"
                                  "</svg>")) );
}

void handleMainPage() {
  // Блокируем главную страницу, если сервер ушел в режим настройки AP:
  if (WiFi.getMode() == WIFI_AP) {
    httpServer.send(404, PSTR("text/plain"), PSTR("Not Found")); return; }
  httpServer.sendHeader(F("Content-Encoding"), F("gzip"));
  httpServer.send(200, (const char*)PSTR("text/html"), (const char*)INDEX_HTML, sizeof(INDEX_HTML));
}

void syncMarlin() {
  // пытаемся определить текущее состояние принтера путем прослушивания потока UART от Марлин
  // при обнаружении характерных сообщений может быть включено bridgeState = SYS_PRINT
  // учитывается свойство Марлин при выполнении длительных операций отправлять сообщение каждые 2 сек.
  // соответственно, ищется пауза в потоке сообщений длиннее 2 сек.
  // на процесс синхронизации выделено общее время 20 сек
  // первые 6 сек пауза в потоке сообщений не отслеживается
  srvSync = true;                             // флаг - признак работы в режиме синхронизации
  uint32_t syncStart = 0, tNow = 0;
  static uint32_t pauseStart = 0;
  do {
    tNow = millis();
    if (!(syncStart)) syncStart = tNow;       // фиксируем время начала всего цикла
    getMarlin();                              // пробуем читать и обрабатывать RX поток
    // (getMarlinRXTime != 0) - флаг наличия RX данных
    // если были RX данные, либо uptime < 6 сек - сбрасываем время начала отсчета паузы
    if ((getMarlinRXTime) || ((tNow - syncStart) < 6000)) pauseStart = tNow;
    ESP.wdtFeed();                            // сбрасываем watchdog таймер
                                              // проверяем условия продолжения цикла
    } while (((tNow - syncStart) < 20000) &&  // общее время выполнения менее 20 сек
              (errCode == ERR_NO_ERRORS)  &&  // после getMarlin() нет ошибок
              srvSync                     &&  // после getMarlin() флаг не сбросился
              ((tNow - pauseStart) < 2500));  // "молчание" Марлин более 2.5 сек не зафиксировано
  // при обнаружении ошибок на этапе синхронизации - блокировка всей дальнейшей работы
  if (errCode != ERR_NO_ERRORS) bridgeState = SYS_SYNC_ERROR ;  // при ошибках блокируем систему
   else if ((bridgeState == SYS_PRINT) || (pubXYZ && !samePos))
    prnFix(0, true);                          // Включаем фиолетовый UI (состояние печати)
}

void setup() {
  //system_update_cpu_freq(160);
  WiFi.persistent(false); 
  WiFi.setAutoConnect(false);                 // На случай, если в памяти модуля есть чужие "хвосты" 
  // Получаем информацию о последнем сбросе
  // и помещаем в в конец буфера gAnswer_buf, где она сохранится до первого вызова init_chunks()
  char* buf = &gAnswer_buf[GANSWER_BUF_SIZE - sizeof(packet)];  // до первого запуска передачи файла размер gAnswer_buf -= sizeof(packet)
  rst_info* rInfo = ESP.getResetInfoPtr();
  chunks[0].len = snprintf_P(buf, NET_DATA_MAX, PSTR(
                                "L:Reset reason is %s.\n"
                                "L:Reason code: %d, Exception cause: %d\n"
                                "L:-------\n"),
                                ESP.getResetReason().c_str(), rInfo->reason, rInfo->exccause);
  // Детальный код причины
  // REASON_WDT_RST = 1 (Превышение времени выполнения / Soft WDT)
  // REASON_EXCEPTION_RST = 2 (Аппаратный краш: деление на ноль, неверный адрес)
  // REASON_SOFT_RESTART = 3
  // REASON_HARD_RESTART = 4 (Hardware WDT / Питание / кнопка Reset)
  // Если упало по исключению, выводим адреса регистров (EPC)
  if (rInfo->reason == REASON_EXCEPTION_RST) {
    chunks[0].len += snprintf_P(buf + chunks[0].len, NET_DATA_MAX - chunks[0].len, PSTR(
                                  "L:Fatal exception:\n"
                                  "L:  epc1=0x%08x, epc2=0x%08x, epc3=0x%08x\n"
                                  "L:  excvaddr=0x%08x, depc=0x%08x\n"
                                  "L:-------\n"),
                                  rInfo->epc1, rInfo->epc2, rInfo->epc3,
                                  rInfo->excvaddr, rInfo->depc);
    }

  // Инициализация UART, включение прерываний RX
  serial2uart(true);  // flush&close Arduino's Serial, init UART0&UART1 for interrupt-driven operations
  //WiFi.setOutputPower(14);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Постоянные обработчики для страницы /config (доступ программно разрешаем только в режиме WIFI_AP)
  httpServer.on(PSTR("/config"), HTTP_GET,  handleGetConfigPage);
  httpServer.on(PSTR("/config"), HTTP_POST, handleSaveConfig);
  httpServer.on(PSTR("/favicon.ico"), HTTP_GET, handleGetIcon);
  // Читаем память. Если данных НЕТ (первый запуск) — реализуем цикл WIFI_AP
  if (!loadConfigFromEEPROM()) {
    // Если сигнатура или CRC не совпали — данные повреждены!
    while (sessionID == 0) sessionID = os_random(); // создаем токен
    sta2ap();                         // поднимаем AP для доступа к /config
    while (apTime > 0) {              // цикл первой настройки ~120 сек
      int len = snprintf_P((char*)packet, sizeof(packet), PSTR("M117 Setup mode AP: %d s.\n"), apTime);
      uart_w((char*)packet, len);
      for (int i = 0; i < 10000; i++) {
        httpServer.handleClient(); delay(1); }
      apTime -= 10; }                 // декремент таймера на 10 сек
    ESP.restart();                    // перезагружаемся через 2 минуты, если не было коннекта
    }
  // вычислим смещение в строках командных топиков до идентификатора-селектора ("G/" или "U/")
  cmd_prefix_len = snprintf_P((char*)packet, sizeof(packet), PSTR(STRINGIZE(MY_PREFIX) "/%s_"), cfg.device_id);
  
  // 0,125 second interval soft timer
  os_timer_disarm(&os_125ms_timer);
  os_timer_setfn(&os_125ms_timer, (os_timer_func_t*)timer_125ms_func, NULL);
  os_timer_arm(&os_125ms_timer, 125, 1);   // load os_timer counter for 0,125 second period

  syncMarlin();
  uartOff();
  
  // создаем обработчики веб-страницы WS клиента
  httpServer.on("/", HTTP_GET, handleMainPage);
  httpServer.on("/ha", HTTP_GET, handleMainPage); // для iFrame карточки Home Assistant

  httpServer.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  stat_init();                        // инициализируем блок статистики
}
