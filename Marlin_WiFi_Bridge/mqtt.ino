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

#include "MQTT_topics.h"
#include "mqtt.h"

/*
 * Упаковывает длину пакета по правилам MQTT v3.1.1 и сдвигает указатель буфера.
 * Передача ptr по ссылке (&) позволяет изменять адрес указателя прямо из функции.
 */
inline void mqttPackLength(uint8_t* &ptr, uint32_t remainLen) {
  do {
    uint8_t d = remainLen & 0x7F; // Выделяем младшие 7 бит
    remainLen >>= 7;              // Сдвигаем число вправо на 7 бит
    if (remainLen > 0) d |= 0x80; // Если впереди еще есть данные — взводим 8-й бит
    *ptr++ = d;                   // Записываем байт и сдвигаем указатель
    } while (remainLen > 0);
}

/**
 * конструирует строку имени топика в соответствии с полями
 * структуры - члена массива pub_data с индексом i
 * и размещает ее в буфере (packet)
 * помимо прямого bool результата модифицирует переменные через параметры-указатели
 * - признак того, что этот топик - командный
 * - указатель на строку конфигурации сущности для механизма discovery HA
 */
bool buildMqttTopic(int i, bool* is_cmd, const char** payload) {
  // 0. Читаем магическое число (2 байта)
  uint16_t item_magic = pgm_read_word(&pub_data[i].magic);
  if (item_magic != MQTT_MAGIC) {
    errCode |= ERR_MQTT_MEM_ERR; return false; } // Жесткая аппаратная защита от мусора!
  // Остальной код выполняется, только если элемент item_magic валиден
  // 1. Читаем базовые поля структуры из PROGMEM (enum - 4 байта, bool - 1 байт)
  HA_entity_t entity_type = (HA_entity_t)pgm_read_dword(&pub_data[i].fn_id);
  *is_cmd = (bool)pgm_read_byte(&pub_data[i].isCmdTopic);
  *payload = (const char*)pgm_read_ptr(&pub_data[i].payload);
  // 2. Читаем адреса строк из таблиц в PROGMEM (указатели - по 4 байта)
  const char* entity_str = (const char*)pgm_read_ptr(&fn_id[entity_type]);
  const char* t_id = (const char*)pgm_read_ptr(&pub_data[i].topic_id);
  // 3. Сборка строки имени топика
  if (!(*is_cmd))
    // вид конфигурационного топика: "discovery_prefix/sensor/IoT_7_TH/config"
    snprintf_P((char*)packet, NET_DATA_MAX, PSTR("%s/%S/%s_%S/config"), 
                                      cfg.ha_prefix, entity_str, cfg.device_id, t_id);
   else
    // вид командного топика: "my_prefix/IoT_7_TH/cmd"
    snprintf_P((char*)packet, NET_DATA_MAX, PSTR(STRINGIZE(MY_PREFIX) "/%s_%S/cmd"), 
                                                                 cfg.device_id, t_id);
  return true;
}

/**
 * ЖЕСТКИЙ СБРОС И ИНИЦИАЛИЗАЦИЯ ВСЕГО MQTT-ДВИЖКА И ЛЕСТНИЦЫ ТОПИКОВ
 * Вызывается принудительно при потере Wi-Fi линка, а также по командам
 */
void mqttDisconnect(bool closeSocket) {
  // 0. СБРОС ПРИКЛАДНОЙ ЛЕСТНИЦЫ ТОПИКОВ ХОУМ АССИСТЕНТА
  // В цикле принудительно опускаем все элементы массива dState[]
  for (uint8_t i = 0; i < DISCOVERY_COUNT; i++)
    dState[i] = DISCOVERY_EXPIRED;
  gData.pubArea = 0;        // после discovery нужно отработать M115
  if (!closeSocket) return;
  // 1. Аппаратно рвем сокет и полностью очищаем его буферы в ОЗУ LwIP
  delay(1);
  if (mqttSock != nullptr) {
    mqttSock->stop();
    delete mqttSock;       // Полностью выгружаем TCP-контекст из ОЗУ
    mqttSock = nullptr; }  // Обнуляем указатель
  // 2. Сбрасываем логические состояния сокета и ошибки по шкале О'Лири
  mqttSockData.sockState = MQTT_SOCKET_DISCONNECTED;
  mqttSockData.lastRC = MQTT_DISCONNECTED;
  // 3. СБРОС ВСЕХ ПЕРЕМЕННЫХ ПОБАЙТОВОГО АВТОМАТА ЧТЕНИЯ (Инициализация "в ноль")
  mqttSockData.remainLen        = 0;
  mqttSockData.multiplier       = 0; // ВАЖНО: Стартовый сдвиг на 0 бит
  mqttSockData.topicLen         = 0;
  mqttSockData.payloadLen       = 0;
  mqttSockData.topicBytesRead   = 0;
  mqttSockData.packetType       = 0;
  mqttSockData.idLo             = 0;
  mqttSockData.idHi             = 0;
  // Сбрасываем таймер Keep-Alive и программного WDT пакета
  mqttSockData.lastPing         = 0;
  mqttSockData.packetStart      = 0;
  mqttSockData.waitStart        = 0;
}

/**
 * 1. ПОДКЛЮЧЕНИЕ С АВТОРИЗАЦИЕЙ
 * - Полностью всеядная функция для RAM и PROGMEM строк.
 */
int mqttConnect(const char* host, uint16_t port, const char* clientId, const char* user, const char* pass) {
  mqttSockData.lastRC = MQTT_CONNECT_FAILED;          // Код ошибки для всех возвратов при неудаче операции
  if (!(host)) return MQTT_DISCONNECTED;              // Жесткая аппаратная защита от мусора!
  if (!(*host) || !(port)) return MQTT_DISCONNECTED;  // не сконфигурировано - возврат
  // поскольку мы вызываем эту функцию только при 
  // mqttSockData.sockState == MQTT_SOCKET_DISCONNECTED;
  // СОЗДАЕМ ОБЪЕКТ ТОЛЬКО ТУТ, КОГДА СЕТЬ УЖЕ ЕСТЬ:
  if (mqttSock == nullptr) mqttSock = new WiFiClient();
  // 1. Устанавливаем базовое TCP-соединение на уровне LwIP
  if (!mqttSock->connect(host, port)) {
    mqttDisconnect(); return MQTT_CONNECT_FAILED; }
  // СВЯЗЬ УСТАНОВЛЕНА: Отключаем задержку Нагла для этого конкретного соединения
  mqttSock->setNoDelay(true); 
  // физ.соединение есть - пробуем авторизоваться, генерируем пакет CONNECT
  snprintf_P((char*)netSendBuf, sizeof(netSendBuf), state_topic, cfg.device_id); // транслируем с учетом cfg.device_id
  const char* lwt_topic = (const char*)netSendBuf; 
  const char* lwt_payload = lwt_str;
  // "Умный" расчет длин через Си-макросы
  uint32_t clientLen = MQTT_GET_LEN(clientId);
  uint32_t userLen   = MQTT_GET_LEN(user);
  uint32_t passLen   = MQTT_GET_LEN(pass);
  uint32_t lwtTopLen = MQTT_GET_LEN(lwt_topic);
  uint32_t lwtPayLen = MQTT_GET_LEN(lwt_payload);
  // Расчитываем длину оставшейся части пакета (RemainLen)
  uint32_t remainLen = 10 + (2 + clientLen); 
  remainLen += (2 + lwtTopLen) + (2 + lwtPayLen);               // добавляем LWT блок
  if (userLen > 0) remainLen += (2 + userLen) + (2 + passLen);  // добавляем еще при наличии авторизации
  // готовим заголовок пакета типа CONNECT в svcBuf
  uint8_t* ptr = (uint8_t*)svcBuf;
  *ptr++ = 0x10;                                    // CONNECT
  mqttPackLength(ptr, remainLen);                   // упаковываем длину
  // Полный объем пакета = заголовок + remainLen
  uint32_t totalPacketSize = (ptr - (uint8_t*)svcBuf) + remainLen;
  // ЖЕСТКАЯ СКВОЗНАЯ ПРОВЕРКА БУФЕРА ДО ЗАПОЛНЕНИЯ И ОТПРАВКИ
  if (mqttSock->availableForWrite() < (int)totalPacketSize) {
    mqttDisconnect(); return MQTT_CONNECT_FAILED; } // Возвращаем -2, если буфер сети занят WebSockets
  // здесь уже можно начать заполнять буфер
  // Дописываем фиксированную часть заголовка MQTT v3.1.1
  *ptr++ = 0x00; *ptr++ = 0x04; *ptr++ = 'M'; *ptr++ = 'Q'; *ptr++ = 'T'; *ptr++ = 'T';
  *ptr++ = 0x04;                                    // Версия протокола MQTT (v3.1.1)
  *ptr++ = (uint8_t)((userLen > 0) ? 0xC6 : 0x06);  // Flags : [name/pass] + LWT + clean session
                                                    // - Bit_7 / Bit_6: Имя пользователя и пароль (0xC0)
                                                    // - Bit_5: Will Retain (0x20)
                                                    // - Bit_4 / Bit_3: Will QoS (0x00 для QoS 0)
                                                    // - Bit_2: Will Flag (0x04) — признак наличия LWT-сообщения.
                                                    // - Bit_1: Clean Session (0x02)
  *ptr++ = 0x00; *ptr++ = 0x3C;                     // KeepAlive (60 сек)
  // Длина Client ID
  *ptr++ = (clientLen >> 8) & 0xFF; *ptr++ = clientLen & 0xFF;
  // Сброс заголовка в сокет
  uint32_t finalHeaderSize = ptr - (uint8_t*)svcBuf;
  if (mqttSock->write((const uint8_t*)svcBuf, finalHeaderSize) != finalHeaderSize) {
    mqttDisconnect(); return MQTT_CONNECT_FAILED; } // Возвращаем -2, если буфер сети занят WebSockets
  // Текст Client ID (макрос сам выберет тип памяти)
  if (MQTT_SOCK_WRITE(clientId, clientLen) != clientLen) {
    mqttDisconnect(); return MQTT_CONNECT_FAILED; } // Возвращаем -2, если буфер сети занят WebSockets
  // --- ОТПРАВКА БЛОКА LWT ИЗ PROGMEM ---
  ptr = (uint8_t*)svcBuf;
  *ptr++ = (lwtTopLen >> 8) & 0xFF; *ptr++ = lwtTopLen & 0xFF;
  if (mqttSock->write((const uint8_t*)svcBuf, 2) != 2) {
    mqttDisconnect(); return MQTT_CONNECT_FAILED; }
  if (MQTT_SOCK_WRITE(lwt_topic, lwtTopLen) != lwtTopLen) {   // Отправка LWT топика из Flash
    mqttDisconnect(); return MQTT_CONNECT_FAILED; }
  ptr = (uint8_t*)svcBuf;
  *ptr++ = (lwtPayLen >> 8) & 0xFF; *ptr++ = lwtPayLen & 0xFF; 
  if (mqttSock->write((const uint8_t*)svcBuf, 2) != 2) {
    mqttDisconnect(); return MQTT_CONNECT_FAILED; }
  if (MQTT_SOCK_WRITE(lwt_payload, lwtPayLen) != lwtPayLen) { // Отправка полезной нагрузки из Flash
    mqttDisconnect(); return MQTT_CONNECT_FAILED; }
  // Если авторизация включена — последовательно досылаем логин и пароль
  if (userLen > 0) {
    ptr = (uint8_t*)svcBuf;
    *ptr++ = (userLen >> 8) & 0xFF; *ptr++ = userLen & 0xFF;
    if (mqttSock->write((const uint8_t*)svcBuf, 2) != 2) {
      mqttDisconnect(); return MQTT_CONNECT_FAILED; } // Возвращаем -2, если буфер сети занят WebSockets
    if (MQTT_SOCK_WRITE(user, userLen) != userLen) {
      mqttDisconnect(); return MQTT_CONNECT_FAILED; } // Возвращаем -2, если буфер сети занят WebSockets
    ptr = (uint8_t*)svcBuf;
    *ptr++ = (passLen >> 8) & 0xFF; *ptr++ = passLen & 0xFF;
    if (mqttSock->write((const uint8_t*)svcBuf, 2) != 2) {
      mqttDisconnect(); return MQTT_CONNECT_FAILED; } // Возвращаем -2, если буфер сети занят WebSockets
    if (MQTT_SOCK_WRITE(pass, passLen) != passLen) {
      mqttDisconnect(); return MQTT_CONNECT_FAILED; } // Возвращаем -2, если буфер сети занят WebSockets
    }
  // Сохраняем время начала ожидания ПАКЕТА CONNACK ОТ БРОКЕРА
  // для контроля таймаута CONNACK
  mqttSockData.waitStart = millis();                  // Стартуем таймаут ожидания CONNACK
  //mqttSockData.packetStart = micros(); // фиксируем время начала обработки пакета / Обнуляем WDT побайтового сборщика пакетов
  mqttSockData.lastPing    = mqttSockData.waitStart;  // Задаем начальную точку отсчета для Keep-Alive пингов!
// Переводим сокет на следующую ступеньку лестницы
  mqttSockData.sockState = MQTT_SOCKET_WAIT; 
  // Сама операция отправки завершилась полным успехом!
  mqttSockData.lastRC = MQTT_CONNECTED; // Результат операции: 0
  return MQTT_CONNECTED; // Возвращаем 0 (Успех отправки стартового пакета)
}

/**
 * 2. ПОДПИСКА НА ТОПИК С КОНТРОЛЕМ ОТПРАВКИ И БУФЕРА
 * Принимает указатель на строку топика из RAM или PROGMEM.
 * Возвращает true при успешной записи в сокет, false — при сбое или нехватке места.
 */
bool mqttSubscribe(const char* topic) {
  mqttSockData.lastRC = MQTT_DISCONNECTED;       // Код ошибки для всех возвратов при неудаче операции
  if (!topic) {
    errCode |= ERR_MQTT_MEM_ERR; return false; } // Жесткая аппаратная защита от мусора!
  // по идее вызов будет производиться, если только mqttSockData.sockState = MQTT_SOCKET_CONNECTED;
  // "Умный" расчет длины топика через наш Си-макрос (поддерживает RAM и Flash)
  uint32_t topicLen = MQTT_GET_LEN(topic);
  if (topicLen == 0) return false;
  // 1. Считаем чистый remainLen по спецификации MQTT v3.1.1:
  //    длина пакета без маркера 0x82 и упакованной длины
  //    2 байта (Packet ID) + 2 байта (длина строки топика) + сама строка + 1 байт (QoS)
  uint32_t remainLen = 2 + 2 + topicLen + 1;
  // 2. Сразу собираем стартовую часть пакета в svcBuf
  uint8_t* ptr = (uint8_t*)svcBuf;
  *ptr++ = 0x82;                            // SUBSCRIBE
  // Инлайн-упаковка длины пакета (сдвигает ptr на 1..4 байта)
  mqttPackLength(ptr, remainLen);
  // Узнаем, сколько байт в svcBuf заняли маркер 0x82 и упакованная длина
  uint32_t currentHeaderSize = ptr - (uint8_t*)svcBuf;
  // Полный физический объем пакета = тек. заполненная длина заголовка + remainLen
  uint32_t totalPacketSize = currentHeaderSize + remainLen;
  // 3. ЖЕСТКАЯ СКВОЗНАЯ ПРОВЕРКА БУФЕРА ДО ЗАПОЛНЕНИЯ И ОТПРАВКИ
  // Защищает общий loop() от фризов, если WebSockets забили общую память сокетов LwIP
  if (mqttSock->availableForWrite() < (int)totalPacketSize) return false;
  // 4. ЗАПОЛНЕНИЕ ОСТАВШЕЙСЯ ЧАСТИ ЗАГОЛОВКА ПАКЕТА В SVCBUF
  *ptr++ = 0x00; *ptr++ = 0x01;             // Packet ID (всегда 0x0001)
  *ptr++ = (topicLen >> 8) & 0xFF;          // Длина топика (High Byte)
  *ptr++ = topicLen & 0xFF;                 // Длина топика (Low Byte)
  // Точный финальный размер заголовка в svcBuf
  uint32_t finalHeaderSize = ptr - (uint8_t*)svcBuf;
  // 5. ПОСЛЕДОВАТЕЛЬНАЯ НЕБЛОКИРУЮЩАЯ ОТПРАВКА В СЕТЬ
  if (mqttSock->write((const uint8_t*)svcBuf, finalHeaderSize) != finalHeaderSize) {
    mqttDisconnect(); return false; }
  // Отправляем текст топика (макрос сам выберет write() для RAM или write_P() для PROGMEM)
  if (MQTT_SOCK_WRITE(topic, topicLen) != topicLen) {
    mqttDisconnect(); return false; }
  // Досылаем финальный байт QoS подписки
  if (mqttSock->write(0x00) != 1) {         // QoS = 0
    mqttDisconnect(); return false; }
  mqttSockData.lastPing = millis();         // Задаем начальную точку отсчета для Keep-Alive пингов!
  mqttSockData.lastRC = MQTT_CONNECTED;     // успех
  return true;                              // Пакет полностью и успешно ушел в сетевой стек
}

/**
 * 3. УНИВЕРСАЛЬНАЯ ПУБЛИКАЦИЯ НА ЧИСТОМ СИ С КОНТРОЛЕМ ОТПРАВКИ
 *    Автоматически разделяет RAM и PROGMEM указатели. 
 */
bool mqttPublish(const char* topic, const char* payload, bool retain) {
  mqttSockData.lastRC = MQTT_DISCONNECTED;  // Ошибка -2 для всех возвратов при неудаче операции
  if (!topic || !payload) {
    errCode |= ERR_MQTT_MEM_ERR; return false; } // Жесткая аппаратная защита от мусора!
  // по идее вызов будет производиться, если только mqttSockData.sockState = MQTT_SOCKET_CONNECTED;
  uint32_t topicLen = MQTT_GET_LEN(topic);
  if (topicLen == 0) return false;
  uint32_t payloadLen = MQTT_GET_LEN(payload);
  uint32_t remainLen = 2 + topicLen + payloadLen; 
  uint8_t* ptr = (uint8_t*)svcBuf;
  *ptr++ = ((retain)? 0x31: 0x30);          // PUBLISH
  mqttPackLength(ptr, remainLen);
  uint32_t totalPacketSize = (ptr - (uint8_t*)svcBuf) + remainLen;
  if (mqttSock->availableForWrite() < (int)totalPacketSize) return false;
  *ptr++ = (topicLen >> 8) & 0xFF;          // Длина топика (High Byte)
  *ptr++ = topicLen & 0xFF;                 // Длина топика (Low Byte)
  uint32_t finalHeaderSize = ptr - (uint8_t*)svcBuf;
  if (mqttSock->write((const uint8_t*)svcBuf, finalHeaderSize) != finalHeaderSize) {
    mqttDisconnect(); return false; }
  if (MQTT_SOCK_WRITE(topic, topicLen) != topicLen) {
    mqttDisconnect(); return false; }
  if (MQTT_SOCK_WRITE(payload, payloadLen) != payloadLen) {
    mqttDisconnect(); return false; }
  mqttSockData.lastPing = millis();         // Задаем начальную точку отсчета для Keep-Alive пингов!
  mqttSockData.lastRC = MQTT_CONNECTED;     // успех
  return true;                              // Пакет полностью и успешно ушел в сетевой стек
}

/**
 * 4. ОСНОВНОЙ ЦИКЛ ОБСЛУЖИВАНИЯ ФИЗ.СВЯЗИ И ОФФСЕТ-ПАРСЕР ВХОДЯЩИХ СООБЩЕНИЙ
 */
void mqttSocketHandler() {
  uint32_t tNow = millis();
  static uint32_t trustTime = 0;
  static bool counTrusTime = false;
  while (mqttSockData.sockState <= MQTT_SOCKET_CONNECTED) {
    // для статичных логических состояний проверяем физику сокета
    if (!mqttSock->connected()) {
      if (mqttSockData.sockState == MQTT_SOCKET_CONNECTED) {
        // если физика "плохая" - будем "помнить" коннект некоторое "доверительное" время
        if (!counTrusTime) { counTrusTime = true; trustTime = tNow; }
        if ((tNow - trustTime) <= MQTT_TRUST_TIME) break; // пока еще "помним" коннект
        mqttDisconnect(); }                               // время вышло - полный сброс
      return; }
     else counTrusTime = false;   // "физика" хорошая - сброс режима счета времени "доверия"
    break;
    }
  /*
  // WDT НА ЗАЛИПАНИЕ В ОДНОМ АКТИВНОМ СОСТОЯНИИ ---
  // Контролируем только активные сетевые фазы
  if (mqttSockData.sockState > MQTT_SOCKET_WAIT) {
    static uint8_t  lastState = 0;
    static uint32_t stateChangedTime = 0;
    // Если на этом витке loop состояние ИЗМЕНИЛОСЬ — фиксируем новую точку отсчета
    if (mqttSockData.sockState != lastState) {
      lastState = mqttSockData.sockState;
      stateChangedTime = tNow;
    }
    // Если автомат ЗАСТРЯЛ в одном и том же активном состоянии (например, READ_REMAIN_LEN)
    // непрерывно дольше 500 миллисекунд — это гарантированный завис побайтовой сборки.
    else if ((tNow - stateChangedTime) > 500) {
      mqttDisconnect(); // Принудительный сброс зависшего автомата
      lastState = 0; stateChangedTime = 0;
      return;
    }
  }
  */
  // Таймер Keep-Alive пингов
  if ((tNow - mqttSockData.lastPing) > MQTT_PING_INTERVAL) {
    // Если в сокете нет места для записи 2 байт без блокировки процессора - выходим до сл.раза
    if (mqttSock->availableForWrite() >= 2) {         // Суммарно мы должны успешно передать ровно 2 байта
      size_t sentBytes = mqttSock->write(0xC0);       // Тип пакета PINGREQ
      sentBytes += mqttSock->write(0x00);             // маркер = 0
      if (sentBytes == 2)                             // Пинг успешно ушел в сетевой стек
        mqttSockData.lastPing = tNow;                 // фиксируем время отправки
       else {
        // Если записалось меньше 2 байт — сеть повреждена, сокет завис.
        // Принудительно закрываем соединение.
        mqttDisconnect(); return; }                   // возврат в общий loop()
    } }
  if (mqttSockData.sockState == MQTT_SOCKET_WAIT)
    // Ждем отправленный CONNECT не дольше MQTT_CONNACK_INTERVAL
    if ((tNow - mqttSockData.waitStart) > MQTT_CONNACK_INTERVAL) {
      mqttDisconnect();
      mqttSockData.lastRC = MQTT_CONNECTION_TIMEOUT;  // Результат операции: ошибка -4
      return; }                                       // возврат в общий loop()
  // ПРОГРАММНЫЙ WDT СБОРКИ И ОЧИСТКИ ПАКЕТА (ЗАЩИТА ОТ ЗАВИСАНИЯ)
  // под контроль попадают ВСЕ промежуточные шаги (от READ_REMAIN_LEN до DISCARD):
  // READ_REMAIN_LEN(3), READ_LEN(4), READ_TOPIC(5), READ_PAYLOAD(6), DISCARD(7)
  if (mqttSockData.sockState > MQTT_SOCKET_WAIT) {
    uint32_t packetTime = micros() - mqttSockData.packetStart;
    if (packetTime > 1000000) {                       // Жесткий таймаут — 1 секунда (в мксек)
      stat(MTR_MQ_PKT_TIME, packetTime);              // статистика обработки пакета
      mqttDisconnect();         // Мгновенно рвем сокет, LwIP сам вычистит все буферы ОЗУ
      mqttSockData.lastRC = MQTT_CONNECTION_TIMEOUT;  // Результат операции: ошибка -4
      return;                                         // возврат в общий loop()
    } }
  if (!mqttSock->available()) return;                 // если сокет пуст - возврат в общий loop()
  // СТАРТОВЫЙ ДИСПЕТЧЕР ПАКЕТОВ (РЕЖИМ ПОКОЯ И ОЖИДАНИЯ СВЯЗИ)
  if (mqttSockData.sockState <= MQTT_SOCKET_WAIT) {   // MQTT_SOCKET_DISCONNECTED исключается на входе
    // здесь режим MQTT_SOCKET_CONNECTED или режим MQTT_SOCKET_WAIT (пока ждем CONNACK)
    mqttSockData.packetStart = micros();              // фиксируем время начала обработки пакета
    mqttSockData.packetType = mqttSock->read();       // Читаем самый первый управляющий байт прилетевшего пакета
    // Инициализируем переменные для побитовой неблокирующей распаковки длины
    mqttSockData.remainLen = 0;
    mqttSockData.multiplier = 0; // Начинаем сдвиг с 0 бит
    mqttSockData.sockState = MQTT_SOCKET_READ_REMAIN_LEN; // Включаем пошаговый сбор длины
    }
  switch (mqttSockData.sockState) {
    case MQTT_SOCKET_DISCARD:                         // неблокирующий слив мусора
      while ((mqttSock->available() > 0) && (mqttSockData.remainLen)) {
        mqttSock->read();                             // Побайтово выкидываем мусор из ОЗУ сети
        mqttSockData.remainLen--; }
      if (mqttSockData.remainLen == 0) {
        stat(MTR_MQ_PKT_TIME, (micros() - mqttSockData.packetStart)); // статистика обработки пакета
        mqttSockData.sockState = MQTT_SOCKET_CONNECTED; } // Очистка завершена, готовы к работе
      break;
    case MQTT_SOCKET_READ_REMAIN_LEN: {               // Пошаговый сбор байт длины пакета (Remaining Length)
      while (mqttSock->available() > 0) {
        uint8_t encodedByte = mqttSock->read();
        mqttSockData.remainLen += ((encodedByte & 0x7F) << mqttSockData.multiplier);
        mqttSockData.multiplier += 7;
        if (mqttSockData.multiplier > 21) {           // Защита от битого/бесконечного заголовка (> 4 байт результат)
          stat(MTR_MQ_PKT_TIME, (micros() - mqttSockData.packetStart)); // статистика обработки пакета
          mqttDisconnect(); return; }
        if (encodedByte & 0x80) continue;
        // Если старший бит равен 0 — сбор байт длины пакета успешно окончен!
        mqttSockData.multiplier = 0;
        mqttSockData.sockState = MQTT_SOCKET_DISCARD; // если пакет не наш - сливаем remainLen
        if (mqttSockData.packetType == 0x20 && mqttSockData.remainLen == 2) {
          // контекст обработки ответа на авторизацию (CONNACK = 0x20)
          mqttSockData.topicBytesRead = 0;            // Сброс счетчика под чтение ответа
          mqttSockData.idLo = 0;                      // Сюда примем флаги сессии
          mqttSockData.idHi = 0;                      // Сюда примем Connect Return Code
          mqttSockData.sockState = MQTT_SOCKET_READ_LEN; }    // Переиспользуем шаг чтения 2 байт
         else if (((bridgeState == SYS_IDLE) || (bridgeState == SYS_PRE_PRINT) || (bridgeState == SYS_PRINT) ||
                   (bridgeState == SYS_WAIT_MOVE) || (bridgeState == SYS_WAIT_TEMP)) &&
                  ((mqttSockData.packetType & 0xF0) == 0x30)) // маскируем флаги в младшем ниббле
          // контекст обработки пакета с командой (PUBLISH = 0x30)
          // отрабатываем только при неактивных состояниях bridgeState
          mqttSockData.sockState = MQTT_SOCKET_READ_LEN;      // переходим к чтению 2 байт длины топика
         else if (mqttSockData.packetType == 0xC0 || mqttSockData.packetType == 0xD0)
          // контекст PINGRESP (0xD0) — сбрасываем Keep-Alive таймер, уходим в DISCARD (remainLen == 0)
          mqttSockData.lastPing = tNow;                       // Пинг подтвержден брокером
        break; }  // while (mqttSock->available() > 0)
      break; }    // Если байты длины еще летят по воздуху — выходим в loop(), не блокируя цикл
    case MQTT_SOCKET_READ_LEN: {                      // Пошаговый сбор 2 байт (длины топика или ответа CONNACK)
      if (mqttSock->available() < 2) break;
      mqttSockData.idLo = mqttSock->read();           // Флаги сессии CONNACK или High-байт длины топика
      mqttSockData.idHi = mqttSock->read();           // Код ответа CONNACK или Low-байт длины топика
      mqttSockData.remainLen -= 2;                    // актуализируем
      // Как только 2 байта гарантированно собрались из сети, вспоминаем контекст
      if (mqttSockData.packetType == 0x20) {
        // Если мы обрабатываем ответ авторизации CONNACK от брокера
        stat(MTR_MQ_PKT_TIME, (micros() - mqttSockData.packetStart)); // статистика обработки пакета
        mqttSockData.lastRC = mqttSockData.idHi;      // 4-й байт пакета (Connect Return Code для массива строк PROGMEM)
        if (mqttSockData.lastRC == MQTT_CONNECTED) {
          mqttSockData.lastPing = tNow;
          mqttSockData.sockState = MQTT_SOCKET_CONNECTED; } // ЦЕЛЕВОЙ ФЛАГ: Сокет полностью готов!
         else
          mqttDisconnect();                           // Брокер отклонил нас (коды ошибок 1..5)
        }
       else if ((mqttSockData.packetType & 0xF0) == 0x30) {
        // Если мы ждали длину топика входящей команды PUBLISH
        mqttSockData.topicLen = (mqttSockData.idLo << 8) | mqttSockData.idHi;
        mqttSockData.payloadLen = mqttSockData.remainLen - mqttSockData.topicLen;
        if ((mqttSockData.payloadLen < 1) || (mqttSockData.payloadLen > (SVC_BUF_SIZE - 1)))
          mqttSockData.sockState = MQTT_SOCKET_DISCARD;     // Пакет аномальный — автоматически в мусор ("-299.99" макс.кмд.строка)
         else {
          mqttSockData.topicBytesRead = 0; mqttSockData.idLo = 0; mqttSockData.idHi = 0;  // Сброс под чтение текста топика
          mqttSockData.sockState = MQTT_SOCKET_READ_TOPIC;  // Переходим к чтению самого топика
        } }
      break; }
    case MQTT_SOCKET_READ_TOPIC: {                    // Пошаговый сбор и оффсет-парсинг текста топика
      // cmd_prefix_len (смещение к идентификатору) вычисляется одноразово в setup() с учетом cfg.device_id
      while ((mqttSock->available() > 0) && (mqttSockData.topicBytesRead < mqttSockData.topicLen)) {
        char c = mqttSock->read();
        // ловим 2 байта идентификатора сущности
        if (mqttSockData.topicBytesRead == cmd_prefix_len)     mqttSockData.idLo = c;
        if (mqttSockData.topicBytesRead == cmd_prefix_len + 1) mqttSockData.idHi = c;
        mqttSockData.topicBytesRead++; mqttSockData.remainLen--;
        }
      if (mqttSockData.topicBytesRead == mqttSockData.topicLen) {
        mqttSockData.topicBytesRead = 0;                    // Использовать как индекс записи в mqttSockData.buf
        memset(mqttSockData.buf, 0, SVC_BUF_SIZE);          // Очистка буфера
        mqttSockData.sockState = MQTT_SOCKET_READ_PAYLOAD;  // Шагаем к чтению тела команды
        }
      break; }
    case MQTT_SOCKET_READ_PAYLOAD: {                        // Пошаговый сбор чистых данных (payload) команды
      int gLen = strlen((char*)mqttSockData.buf);
      while ((mqttSock->available() > 0) && (mqttSockData.topicBytesRead < mqttSockData.payloadLen)) {
        uint8_t s = mqttSock->read();
        if ((s == '\r') || (s == '\n')) {
          if ((gLen) && (mqttSockData.buf[gLen - 1] == '\n')) gLen -= 1;
          s = '\n'; }                                       // убираем дубликаты конца строки
        mqttSockData.buf[gLen++] = s;
        mqttSockData.topicBytesRead += 1; mqttSockData.remainLen -= 1; }
      if (mqttSockData.topicBytesRead < mqttSockData.payloadLen) break; // дочитаем, когда появятся данные
      mqttSockData.sockState = MQTT_SOCKET_CONNECTED;                   // Возвращаем сокет в режим покоя
      stat(MTR_MQ_PKT_TIME, (micros() - mqttSockData.packetStart));     // статистика обработки пакета
      uint32_t topicCommandID = (uint32_t)((mqttSockData.idHi << 8) | mqttSockData.idLo);
      if (ctrl & MQTT_SDATA) {
        int len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:>%s[0x%04lX] Len=%u \""),
                                            (char*)&topicCommandID, topicCommandID, gLen);
        memcpy(packet + len, mqttSockData.buf, gLen);
        int i = 0;
        for (; i < (len + gLen); i++)                                   // покажем все переводы строки
          if (packet[i] == '\n') packet[i] = '~';                       // заменим их на '~'
        packet[i++] = '\"'; packet[i++] = '\n'; packet[i] = '\0';
        netQuePut_cid(packet, i, socket.clWS_ID);
        }
      bool validGcode = true;
      if ((topicCommandID == 0x2F47) || (topicCommandID == 0x2F67)) {   // "g/" или "G/"
        if ((bridgeState == SYS_PRINT) || (bridgeState == SYS_WAIT_MOVE) || (bridgeState == SYS_WAIT_TEMP)) break;
        // пакет должен быть длиннее минимальной команды G-code (3 байта) + 12 байт маркера (';mid=${this.card_id}:HAM2\n')
        if (gLen < 15) break;
        // 1. Валидатор для G-КОДА
        // 1.1 Контроль первого символа
        if (!memchr_P(PSTR("GgMmTt"), mqttSockData.buf[0], sizeof("GgMmTt") - 1)) break;
        // 1.2 Стандартная проверка остальных байт на печатный ASCII
        for (int i = 0; (validGcode && (i < gLen)); i++)
          validGcode = (isprint(mqttSockData.buf[i]) || (mqttSockData.buf[i] == '\n'));
        if (!validGcode) break;
        uint8_t* m_ptr = (uint8_t*)memchr(mqttSockData.buf, ';', gLen);   // ищем начало комментария
        if (!m_ptr) break;                                                // если коментарий не найден - игнор и возврат в loop()
        uint32_t markerLE = m_ptr[1] | (m_ptr[2] << 8) | (m_ptr[3] << 16) | (m_ptr[4] << 24); // str -> Little Endian
        if (markerLE != 0x3D64696D) break;                                // проверяем : "mid=" -> 0x3D64696D ('=' 'd' 'i' 'm')
        *m_ptr = 0; m_ptr[1] = 0;   m_ptr += 5;                           // обрезаем комментарий (обнуляем 2 байта)
        uint32_t mqtt_ID;
        if (!getNum(&mqtt_ID, (char**)&m_ptr, 0xFFFF0000, false)) break;  // считываем идентификатор клиента - источника команды
        // канал входящих MQTT-команд "зарезервирован" для socket.mqttCtrl_ID на время mqttBusyTime
        if (socket.mqttCtrl_ID)
          if (((tNow - timeMQTTCtrl) < mqttBusyTime) && (socket.mqttCtrl_ID != mqtt_ID)) break;
        if (!isDigit(m_ptr[4])) break;                                    // Считываем символ числа пропусков "OK" и переводим в число
        uint8_t skipNum = m_ptr[4] & 0xF;                                 // Получаем чистые 0, 1, 2 и т.д.
        markerLE = *m_ptr | (m_ptr[1] << 8) | (m_ptr[2] << 16) | (m_ptr[3] << 24); // считываем маркер для сравнения Little Endian
        // Числовые эквиваленты строк в формате Little-Endian (разворот байт на ESP8266):
        // ":HAM" -> 0x4D41483A  ( 'M' 'A' 'H' ':' )
        // ":HAT" -> 0x5441483A  ( 'T' 'A' 'H' ':' )
        // ":HAG" -> 0x4741483A  ( 'G' 'A' 'H' ':' )
        switch (markerLE) {
          case 0x4D41483A:        // Обнаружен маркер движения ":HAM"
            // "резервируем" канал входящих MQTT-команд для socket.mqttCtrl_ID на время mqttBusyTime
            socket.mqttCtrl_ID = mqtt_ID; timeMQTTCtrl = tNow; mqttBusyTime = 25000 + 10000;
            ok.setState = bridgeState; bridgeState = SYS_WAIT_MOVE;
            ok.wdLoad = (25 * TICKS_PER_SEC); // Тайм-аут 25 секунд
            break;
          case 0x5441483A:        // Обнаружен маркер температуры ":HAT"
            // "резервируем" канал входящих MQTT-команд для socket.mqttCtrl_ID на время mqttBusyTime
            socket.mqttCtrl_ID = mqtt_ID; timeMQTTCtrl = tNow; mqttBusyTime = 35000 + 10000;
            ok.setState = bridgeState; bridgeState = SYS_WAIT_TEMP;
            ok.wdLoad = (35 * TICKS_PER_SEC); // Тайм-аут 35 секунд
            break;
          case 0x4741483A:        // Обнаружен маркер G-макросов ":HAG"
            // "резервируем" канал входящих MQTT-команд для socket.mqttCtrl_ID на время mqttBusyTime
            socket.mqttCtrl_ID = mqtt_ID; timeMQTTCtrl = tNow; mqttBusyTime = 10000;
            // ПОЛНОЕ ИГНОРИРОВАНИЕ ОБРАТНОЙ СВЯЗИ:
            // Не меняем bridgeState
            // обнуляем параметры ожидания OK
            ok.skip = 0; ok.waiting = false; ok.wdTimer = 0;
            break;
          default:
            break;
          }
        if (socket.mqttCtrl_ID != mqtt_ID)
          break;                  // валидный маркер не обнаружен - игнорируем
        gLen = strlen((char*)mqttSockData.buf);
        if (mqttSockData.buf[gLen - 1] != '\n') mqttSockData.buf[gLen++] = '\n';  // гарантируем терминатор '\n'
        if (markerLE != 0x4741483A) {
          ok.skip = skipNum; ok.waiting = true; ok.wdTimer = ok.wdLoad; busyMarkerTime = tNow; }
        uart_w((char*)mqttSockData.buf, gLen, true);
        gData.progress = 0; pubProgress = true; // флаг для публикации socket.mqttCtrl_ID
        if (ctrl & MQTT_SUB) {
          int i = 0;
          for (; i < gLen; i++)             // покажем все переводы строки, заменим их на '\'
            if (mqttSockData.buf[i] == '\n') mqttSockData.buf[i] = '\\';
          // весь текст возьмем в кавычки. здесь добавим закрывающую кавычку и символ конца строки
          mqttSockData.buf[i++] = '\"'; mqttSockData.buf[i++] = '\n'; mqttSockData.buf[i] = '\0';
          // открывающую кавычку добавим к маркеру типа строки
          netQuePut(mqttSockData.buf, i, (char*)PSTR("L:\""), socket.clWS_ID); }
        } // if (topicCommandID == 0x2F47 || topicCommandID == 0x2F67)
       else if (topicCommandID == 0x2F55 || topicCommandID == 0x2F75) { // "U/" или "u/"
        if (mqttSockData.payloadLen != 1) break;
        static uint32_t lastMqttCmdTime = 0;                            // Локальный таймер контроля частоты следования mqtt команд
        if ((tNow - lastMqttCmdTime) < 2000) break;                     // Разрешаем отрабатывать сервисные команды не чаще раза в 2 сек
        lastMqttCmdTime = tNow;
        int len;
        if (ctrl & MQTT_SUB) {
          len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("Control code \"%c\"\n"), mqttSockData.buf[0]);
          netQuePut(packet, len, (char*)PSTR("L:"), socket.clWS_ID); }
        // Для сервисного топика Uptime проверяем одиночные символы
        switch (mqttSockData.buf[0]) {
          case '0':
            doReboot(); break;                                          // рестарт сервера
          case '1':
            mqttDisconnect(false);                                      // запрос перерегистрации всех сущностей Home Assistant, сокет не закрываем
            break;
          case '3':
            gData.pubArea = 0; validGcode = false;                      // запрос на генерацию M115 для получения габаритов принтера
            break;
          case '2':                                                     // экстренный сброс всех активных состояний принтера
            if ((bridgeState == SYS_WAIT_MOVE) || (bridgeState == SYS_WAIT_TEMP)) {
              // прерывание обработки MQTT G-команды перемещения или установки температуры
              bridgeState = (((ok.setState == SYS_PRE_PRINT) || (ok.setState == SYS_IDLE))? ok.setState: SYS_IDLE);
              ok.skip = 0; ok.waiting = false; ok.wdTimer = 0; }
            timeMQTTCtrl = tNow; mqttBusyTime = 10000;
            socket.mqttCtrl_ID = 0x10001;                               // блокируем канал MQTT команд на 10 сек "невозможным" для клиентов ID
            // M410 — тормозим моторы и чистим буфер движения.
            // M108 — прерываем зависшие циклы ожидания нагрева, если они были.
            uart_w((char*)PSTR("M410\nM108\n"));
            break;
          case '4':                                                     // переключение в режим WiFi AP для получения новой конфигурации и ее записи в EEPROM 
            if (bridgeState == SYS_IDLE) sta2ap(PSTR(
                                                "L::: MQTT reconfigure command received.\n"
                                                "L:,1.Connect AP Marlin-Bridge-Setup\n"
                                                "L:,2.Browse 192.168.9.1/config\n"));
            break;
          default:
            validGcode = false;
            break;
          }
        if (validGcode) gData.progress = 0; 
        } // if (topicCommandID == 0x2F55 || topicCommandID == 0x2F75) {
      break; }  // case MQTT_SOCKET_READ_PAYLOAD: {
    default:
      break;
    } // switch (mqttSockData.sockState) {
}

/**
 * 5. диспетчер операций MQTT. Вызывается из основного loop().
 */
void mqttLoop() {
  if (wifi_timer <= 0) return; 
  uint32_t now = millis();
  // 0. Дежурный таймер вызова (2 мс / 20 мс)
  static uint32_t lastMQTTLoop = 0;
  if ((now - lastMQTTLoop) < ((mqttSockData.sockState > MQTT_SOCKET_WAIT)? MQTT_LISTEN_LOOP : MQTT_REGULAR_LOOP)) return;
  lastMQTTLoop = now;
  bool mqttActions = (mqttSockData.sockState <= MQTT_SOCKET_WAIT); // флаг разрешения опроса параметров принтера
  // 1. если сокет полностью отключен, запускаем инициализацию коннекта с брокером
  if (mqttSockData.sockState == MQTT_SOCKET_DISCONNECTED) {
    static uint32_t lastMQTTConnect = 0;
    static uint32_t waitMQTTconnect = 0;
    if ((now - lastMQTTConnect) < waitMQTTconnect) return;
    mqttConnect(cfg.mqtt_ip, cfg.mqtt_port, cfg.device_id, cfg.mqtt_user, cfg.mqtt_pass);
    lastMQTTConnect = now;
    // при неудаче попытки коннекта - задаем интервал повторов, чтобы не спамить
    waitMQTTconnect = ((mqttSockData.sockState != MQTT_SOCKET_DISCONNECTED) ? 0 : MQTT_CONNECT_WAIT);
    return; 
    }
  // 2. Турбо-петля чтения сокета
  if (mqttSock == nullptr) return;      // защита на всякий случай
  uint32_t listenStart = micros(), listenTime = 0, listenBreak = 0;
  uint32_t lastAvailable = listenStart; // инициализируем таймер ожидания сетевых данных
  do {
    mqttSocketHandler();                // читаем из сокета и парсим пришедшие команды MQTT
    if (mqttSock == nullptr) break;     // произошел вызов mqttDisconnect(), выходим из петли
    listenBreak = micros();
    if (mqttSock->available() > 0)
      lastAvailable = listenBreak;      // еще есть сетевые данные, обновляем таймер ожидания
    listenTime = listenBreak - listenStart;
    } while ((mqttSockData.sockState > MQTT_SOCKET_WAIT) &&
              // продолжаем читать из сокета, пока он в активном состоянии с учетом ограничителей
              // ОГРАНИЧИТЕЛЬ №1: <= 3000 микросекунд (3 мсек) общей продолжительности цикла чтения.
              // За 3 мсек в кольцевой буфер UART RX прилетит всего ~150 байт из 1024 допустимых.
              // Это дает 6-кратный запас безопасности для принтера!
             (listenTime < 3000) &&
              // ОГРАНИЧИТЕЛЬ №2: <= 200 микросекунд на ожидание отсутствующих данных из сети.
             ((listenBreak - lastAvailable) < 200));
  stat(MTR_MQ_LOOP_TIME, listenTime);   // статистика времени одного прохода mqttSocketHandler()
  // Проверяем условия возврата в общий loop()
  if (!mqttActions) return;             // сокет еще "горячий"
  if (listenTime > 2000) return;        // на чтение ушло слишком много времени
  if (mqttSockData.sockState != MQTT_SOCKET_CONNECTED) return; // сокет в активном состоянии
  // здесь, когда "логика" подключена (подразумевает также наличие подключения "физики")
  // проходим к контролю топиков, когда сокет в статичном состоянии (sockState == MQTT_SOCKET_CONNECTED)
  // 3. Диспетчер операций с топиками
  /*--- 3.1 публикация данных ---*/
  if (dState[DISCOVERY_COUNT - 1] == DISCOVERY_ANNOUNCED) {
    // здесь полный коннект с брокером - публикуем текущие значения параметров
    static uint32_t pubTime = 0, pollTime = 0, tbPollTime = 0, xyzPollTime = 0, pgsPollTime = 0;
    int jsonLen = 0;
    // Отправляем данные через 50 мсек после обновления;
    // выдерживаем интервал 50 мсек между публикациями;
    // запрещаем опрос параметров, если принтер занят, либо активирован "ручной" командный режим;
    // при этом публикация не происходит, поскольку не поступают данные от Марлин;
    // upTime и прогресс (upload progress) могут поступать без опроса
    // пакет с upTime отправляем автоматически раз в 5 секунд;
    // интервалы автоопроса prnFix() короче ручных, это предотвращает дублирование, если автоопрос поддерживается
    if (((now - busyMarkerTime) < 2500) ||      // ждем 2.5 сек, если Марлин сообщил, что занят
        ((bridgeState != SYS_IDLE) && (bridgeState != SYS_PRE_PRINT) && (bridgeState != SYS_PRINT)) ||
        (cmdMode)) {                            // если отрабатываются ручные команды отладки или идет листинг файлов
      // запрещаем опрос, сбрасываем таймеры интервалов по всем параметрам
      tbPollTime = now; xyzPollTime = now; pgsPollTime = now; }
    if (pubTB && ((now - timeTB) >= 50) && ((now - pubTime) >= 50)) {
      // 3.1.1. ПАКЕТ ТЕМПЕРАТУР (Текущие + Целевые)
      pubTB = false; pubTime = now; tbPollTime = now;
      // Собираем плоский JSON для HA
      jsonLen = snprintf_P((char*)packet, NET_DATA_MAX - 3,
                  PSTR("{\"th\":%.1f,\"tht\":%.1f,\"tb\":%.1f,\"tbt\":%.1f}"), 
                  gData.currentHotend, gData.targetHotend, gData.currentBed, gData.targetBed);
      } 
    if (((now - pollTime) > 50) && ((now - tbPollTime) > 7400)) {
      uart_w((char*)PSTR("M105\n")); tbPollTime = now; pollTime = now; }
    if (pubXYZ && ((now - timeXYZ) > 50) && ((now - pubTime) >= 50)) {
      // 3.1.2. ПАКЕТ КООРДИНАТ И ШАГОВ ОСЕЙ
      pubXYZ = false; pubTime = now; xyzPollTime = now;
      // Собираем плоский JSON для HA
      jsonLen = snprintf_P((char*)packet, NET_DATA_MAX - 3,
                  PSTR("{\"x\":%.2f,\"xs\":%ld,\"y\":%.2f,\"ys\":%ld,\"z\":%.2f,\"zs\":%ld,\"e\":%.2f,\"s\":%lu}"),
                  gData.xPos, gData.xStep, gData.yPos, gData.yStep, gData.zPos, gData.zStep, gData.ePos, (unsigned long)gData.sxyzCode);
      } 
    if (((now - pollTime) > 50) && ((now - xyzPollTime) > 3400)) {
      uart_w((char*)PSTR("M114\n")); xyzPollTime = now; pollTime = now; }
    uint32_t fmt_sec = 0;                                     // если изменится, то это сообщает о публикации пакета системного статуса
    if (((pubProgress && ((now - timeProgress) >= 50)) || ((now - timeProgress) >= 5000)) && ((now - pubTime) >= 50)) {
      // 3.1.3. ПАКЕТ СИСТЕМНОГО СТАТУСА (State + HA_ID + uf + Progress + Файл) или (State + HA_ID + uf + Progress + Area)
      //                             HA_ID - id клиента, полученный из MQTT канала управления (топик G/cmd)
      //                                uf - форматированный аптайм (HH:MM:SS)
      //      - публикуется через >=50 мсек после изменения прогресса в updIntValue()
      //      - автоматически публикуется через >=5 сек после предыдущей публикации такого пакета
      //      - публикуется не ранее чем через 50 мсек после любой предыдущей публикации
      if (pubProgress) pgsPollTime = now;                     // публикация новых данных по требованию
       else if (!(gData.pubArea & 0x03300000))                // автопубликация и мы не в цикле запроса M115
        if (!(ok.wdTimer) && !fListMode && !fListWait) {      // Марлин не занят
          static uint32_t m115ReqTime = 0;
          if ((now - m115ReqTime) >= 10000) {                 // не чаще, чем через 10 сек
            gData.pubArea = 0x01100000; m115ReqTime = now;    // ставим требование M115
          } }
      pubProgress = false; pubTime = now; timeProgress = now;
      fmt_sec = now / 1000;                                   // переводим в секунды
      uint32_t hr = fmt_sec / 3600; fmt_sec %= 3600;
      uint32_t min = fmt_sec / 60; fmt_sec %= 60;
      int gState;
      if (bridgeState == SYS_WAIT_OTA) gState = PUB_OTA_MODE;
       else if ((bridgeState == SYS_OTA) || (bridgeState == SYS_OTA_END)) gState = PUB_OTA_UP;
       else gState = ((pubState >= HB_IDLE) && (pubState <= PUB_WAIT))? pubState: PUB_UNKNOWN;
      if ((now - timeMQTTCtrl) >= mqttBusyTime) socket.mqttCtrl_ID = 0; // "забываем" ID после истечения mqttBusyTime
      // gData.progress - прогресс загрузки/печати вычисляется при обновлении в gAnswer()
      if ((gData.pubArea & 0xFFFF0000) != 0x02200000) {       // пока в младших 2-х байтах нет сохраненного индекса
        // Собираем плоский JSON для HA
        const char* fNameStr = (*socket.fName)? socket.fName : fl2chr(PSTR("No file selected"));
        jsonLen = snprintf_P((char*)packet, NET_DATA_MAX - 3, 
                    PSTR("{\"g\":\"%S\",\"u\":%lu,\"uf\":\"%02lu:%02lu:%02lu\",\"rssi\":%ld,\"p\":%lu,\"pf\":\"%s\"}"),
                    (const char*)pgm_read_ptr(&pubState_id[gState]), socket.mqttCtrl_ID, hr, min, fmt_sec, WiFi.RSSI(),
                    gData.progress, fNameStr); }
       else {                                                 // если есть сохраненный индекс начала ответа на M115
        gData.pubArea &= 0x0000FFFF;                          // выделяем сохраненный индекс начала json строки "area:{.."
        jsonLen = snprintf_P((char*)packet, NET_DATA_MAX - 3, 
                    PSTR("{\"g\":\"%S\",\"u\":%lu,\"uf\":\"%02lu:%02lu:%02lu\",\"rssi\":%ld,\"p\":%lu,\"dim\":\"{%s}\"}"),
                    (const char*)pgm_read_ptr(&pubState_id[gState]), socket.mqttCtrl_ID, hr, min, fmt_sec, WiFi.RSSI(),
                    gData.progress, &gAnswer_buf[gData.pubArea]);
        gAnswer_idx = anchorIdx = gData.pubArea; gData.pubArea = 0x03300000; // восстанавливаем индексы, ставим флаг "M115 отработано"
        if (cmdMode) cmdMode -= 1; }                          // восстановление опроса параметров
      }
    if (((now - pollTime) > 50) && ((now - pgsPollTime) > 11400)) {
      uart_w((char*)((gData.pubArea == 0x01100000)? PSTR("M115\n"): PSTR("M27\n")));
      if (gData.pubArea == 0x01100000) {
        ok.waiting = true; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; } // 5 сек 
      pgsPollTime = now; pollTime = now; }
    // Жесткая проверка безопасности глобального буфера
    if ((jsonLen > 0) && (jsonLen < 256)) {
      snprintf_P((char*)netSendBuf, sizeof(netSendBuf), state_topic, cfg.device_id); // транслируем с учетом cfg.device_id
      mqttPublish((char*)netSendBuf, (char*)packet, false);
      if (ctrl & MQTT_PUB) {
        jsonLen = ((jsonLen > (NET_DATA_MAX - 3))? (NET_DATA_MAX - 3): jsonLen);
        packet[jsonLen] = '\n';
        netQuePut(packet, jsonLen, (char*)PSTR("L:"), socket.clWS_ID);
      } }
     else if ((ctrl & MQTT_PUB) && (jsonLen >= 256)) {
      jsonLen = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:mqttPub msgSize %d\n"), jsonLen);
      netQuePut_cid(packet, jsonLen, socket.clWS_ID); }
    // для OTA - перезагрузка после публикаци системного статуса и прогресса 100%
    if ((fmt_sec) && (bridgeState == SYS_OTA_END)) doReboot();
    return; // Уступаем квант времени Marlin и WebSockets
    } // if (dState[DISCOVERY_COUNT - 1] == DISCOVERY_ANNOUNCED)
  /*--- 3.2 механизм DISCOVERY ---*/
  // если попали сюда - нужно изменить состояние членов массива dState, т.е. анонсировать сущности для HA
  // в исходном состоянии и при дисконнектах значение для всех членов устанавливается равным DISCOVERY_EXPIRED
  // Общий алгоритм действий такой - сканируется массив dState и по очереди весь массив приводится к состоянию,
  // когда все члены примут одинаковое общее значение, не равное DISCOVERY_EXPIRED
  // причем, в текущем loop() допускается только одно обращение к брокеру
  // 1-й цикл переводит все DISCOVERY_EXPIRED   -> DISCOVERY_CONNECTED  ( + уничтожение сущностей по строкам конфигураций)
  // 2-й цикл :             DISCOVERY_CONNECTED -> DISCOVERY_ANNOUNCED  ( + публикация конфигураций и подписка cmd топиков)
  // Когда все члены массива примут состояние DISCOVERY_ANNOUNCED - можно публиковать значения параметров в топик /state
  static bool skipLoop = true;
  skipLoop = !skipLoop; if (skipLoop) return;                   // пропускаем каждый 2-й проход, чтоб не спамить брокер
  for (int i = 0; i < DISCOVERY_COUNT; i++) {
    if (dState[i] != dState[DISCOVERY_COUNT - 1]) continue;     // пропускаем уже отработанные
    // здесь оказываемся, если текущий равен последнему, но еще не равен DISCOVERY_ANNOUNCED
    const char* payload;
    bool isCmdTopic, announced = false;
    if (buildMqttTopic(i, &isCmdTopic, &payload)) {             // конструируем строку в соответствии с полями массива pub_data
      char* topic = (char*)packet;                              // получаем указатель на строку топика и на шаблон payload из PROGMEM
      if (dState[DISCOVERY_COUNT - 1] == DISCOVERY_EXPIRED) {   // 1-й цикл
        if (isCmdTopic) {                                       // если командный топик, брокер не трогаем,
          dState[i] = DISCOVERY_CONNECTED;  continue; }         // просто поднимаем статус и делаем еще итерацию
         else if (mqttPublish(topic, (const char*)(""), true))  // если не командный топик - чистим конфигурацию (публикуем пустую строку с retain = true)
          dState[i] = DISCOVERY_CONNECTED;                      // и поднимаем статус, если успешно опубликовали
        }    // if (dState[DISCOVERY_COUNT - 1] == DISCOVERY_EXPIRED)
       else {                                                   // 2-й цикл
        // здесь (dState[DISCOVERY_COUNT - 1] == DISCOVERY_CONNECTED)
        if (isCmdTopic)
          announced = mqttSubscribe(topic);                     // Подписка на командный топик
         else {
          if (netBufEnd != netBufBeg) netQueSend();             // нужно очистить сетевой буфер, при неудаче пропуcкаем до следующего loop()
          if (netBufEnd == netBufBeg) {                         // конструируем строку конфигурации в сетевом буфере
            snprintf_P((char*)netBuf, NET_BUF_SIZE, payload, cfg.device_id, cfg.device_id, cfg.device_id, cfg.device_id, cfg.device_id, cfg.device_id);
            announced = mqttPublish(topic, (char*)netBuf, true);  // Анонс конфига
          } }
        if (announced) dState[i] = DISCOVERY_ANNOUNCED;         // поднимаем статус
      } }
    break;                                                      // Строго ОДНА сетевая операция за проход
    }   // for (int i = 0; i < DISCOVERY_COUNT; i++)
}
