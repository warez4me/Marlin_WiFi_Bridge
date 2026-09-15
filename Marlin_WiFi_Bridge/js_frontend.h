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

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<link rel="icon" href="data:,"><title>Marlin Bridge</title>
<style>
  /* --- 1. ОБЩИЕ НАСТРОЙКИ --- */
  * { box-sizing: border-box; } /* Размеры включают внутренние отступы */

  body {
    font: 14px monospace; background: #000; color: #eee; /* Темный фон всей страницы, Светло-серый текст */
    // padding: 10px;
    margin: 0; text-align: center;
  }

  /* Заголовок с небольшим разрядом букв */
  h3 { letter-spacing:.5px; }

  /* Контейнер */
  .card {
    background: #223;
    padding: 15px;
    border-radius: 2px;
    //border: 1px solid #0f0;
    display: inline-block;
    width: 100%; max-width: 500px; /* Чтобы не растягивалась на весь экран ПК */
    box-shadow: 0 10px 30px #0008; /* Тень для объема (8 в конце - это 50% прозрачности) */
    margin-top: 10px;
  }

  /* --- 2. БАЗОВЫЙ КИРПИЧ (.c) --- */
  /* Применяем ТОЛЬКО к классу .c. Теперь радиокнопки в безопасности */
  .c { 
    height: 44px; /* Стандарт под палец */
    margin: 10px 0;
    border-radius: 2px;
    //font-size: 14px; 
    display: flex; align-items: center; justify-content: center; 
    position: relative; overflow: hidden; border: 1px solid #000;
    pointer-events: none; user-select: none; font-weight: bold;
    transition: .2s;  /* Плавная смена всех состояний */
  }

  /* --- 3. ПАЛИТРА (UC 0-7) --- */
  /* Добавить !important, чтобы цвета всегда перебивали базу */
  /* типа { background: #27ae60 !important; color: #fff; } */
  /* 0. UPLOAD START (Маджента - Импульс) */
  .c0{ background: #94b; color: #eee; border-color: #fff6; }  /* Magenta */
  /* 1. UPLOADING (Голубой - Данные) */
  .c1 { background: #28b; color: #eee; border-color: #fff6; } /* Blue */
  /* 2. PRINT START (Зеленый - Позитив) */
  .c2 { background: #2a6; color: #eee; border-color: #fff6; } /* Green */
  /* 3. PRINTING (Фиолетовый - Технология) */
  .c3 { background: #516; color: #eee; border-color: #fff6; } /* Purple */
  /* 4. IDLE / SELECT (Темно-синий - Покой) */
  .c4 { background: #235; color: #abc; border-color: #fff6; } /* Dark Blue */
  /* 5. CONFIRM / CANCEL (Изумрудный - Диалог в нижнем баре) */
  .c5 { background: #1a8; color: #eee; border-color: #fff6; }  /* Emerald */
  /* 6. STARTUP / WAIT (Серый - Заглушка до первого HB) */  
  .c6 { background: #223; color: #888; border-color: #223; }  /* "Hidden" BG */
  /* 7. ERROR (Красный - Тревога) */
  .c7 { background: #c32; color: #fff; border-color: #fff6; } /* Red */

  /* --- 4. МОДИФИКАТОРЫ : ФИЗИКА И ЭФФЕКТЫ --- */
  .on { pointer-events: auto !important; cursor: pointer; }
  .on:hover { filter: brightness(1.2); box-shadow: 0 0 12px #fff6; transform: scale(1.02); animation: none !important; }
  .on:active { transform: scale(0.98); border-color: #fff6 !important; }

  @keyframes zz { 0%, 100% { filter: brightness(1); } 50% { filter: brightness(1.3); box-shadow: 0 0 12px #fff6; } }
  .bl { animation: zz 3s infinite ease-in-out; }
  .cd { 
    opacity: 0.6 !important; 
    pointer-events: none !important; /* <--- ОБЯЗАТЕЛЬНО !important */
    cursor: default !important;     /* Убираем палец на замке */
  }
  .cb {
    border-color: #000 !important;
  }

  /* --- 5. СПЕЦИФИКА (Инпут, Лог, Радио) --- */
  input.c {
    text-align: left;
    padding: 0 10px;
    font-size: 16px;
    background: #000;
    color: #0d0;  /* "матричный" зеленый */
    /* pointer-events и cursor ЗДЕСЬ УДАЛЯЕМ! */
  }
  /* Специально для активного инпута возвращаем "палочку" */
  input.on { cursor: text !important; }
  
  #l {
    height: 440px; //220px;
    background: #000;
    text-align: left; padding: 10px;
    font: 13px/1.4 monospace;
    margin: 12px 0;
    //border: 1px solid #111;
    // border-radius: 4px;
    overflow-y: auto; white-space: pre-wrap; // pointer-events: auto;
  }
  #l div { border-bottom: 1px solid #000; padding: 2px 0; }

  #rc { margin: 15px 0; display: flex; justify-content: center; gap: 20px; pointer-events: auto; }
  #rc label { display: flex; align-items: center; gap: 5px; cursor: pointer; }

  /* ПРОГРЕСС-БАР: Прозрачная шторка */
  #g { position: absolute; left: 0; top: 0; height: 100%; width: 0; background: #fff6; transition: .3s; z-index: 1; } // background: rgba(255, 255, 255, 0.25)

  #t { position: relative; z-index: 2; }

  #n {
    transition: opacity .4s; display: inline-block;
    //background: #0005;
    padding: 2px 8px;
    //border-radius: 4px;
  }

  .online { color: #0d0; font-weight: bold; }  // #2ecc71
  .offline { color: #c32; font-weight: bold; } // #e74c3c

  #fi { display: none; }

  //@media(min-width: 600px) { #l { height: 60vh; } }

</style>
</head>
<body>
  <div class="card">
    <h3>Bridge: <span id="n" class="offline">OFFLINE</span></h3>
    <div id="rc">
      <label><input type="radio" name="m"> Text</label>
      <label><input type="radio" name="m"> BFT</label>
    </div>
    <input type="file" id="fi" onchange="fSel()">
    
    /*<!-- Кнопки всегда имеют класс .c -->*/
    <div id="b" onclick="mBtn()" class="c"></div>
    <div id="x" onclick="qBtn()" class="c"><div id="g"></div><span id="t"></span></div>
    
    <div id="l"></div>

    <div style="display:flex; gap:10px;">
      /*<!-- Инпут и SEND тоже с классом .c -->*/
      <input id="i" class="c" placeholder="G-Code or ?" style="flex:3;" enterkeyhint="send">
      <div id="s" onclick="gCmd()" class="c" style="flex:1;">SEND</div>
    </div>
  </div>

<script>
  /*---  Таблица значений кода состояния Z  ---
  ===================================================================================
  - биты с весом 2 и 1 : код состояния системы из HeartBeat
                  >  0 : свободно
                  >  1 : идет загрузка файла
                  >  2 : файл загружен и готов к печати
                  >  3 : идет печать [файла]
  - бит с весом 4 >  0 : - имя файла в HeartBeat -
                  >  1 : + имя файла в HeartBeat +
  - бит с весом 8 >  0 : наш cid (clientID) != sid (serverID) -> мы наблюдатель
                  >  1 : наш cid (clientID) == sid (serverID) -> мы активист
  ===================================================================================
  Карта битов управляющей маски m
  Каждое число в M[16] (Uint16) — это «пульт управления» элементами rb, b, t, g, x, i.
  Ниббл  Бит  Вес     Элемент       Логика (0 / 1)
  L      0-2	0x0007	BT Index     Индекс в массиве BT (0-7)
          3   0x0008  BT Mode      0: BT[i]         /  1: BT[i] + sf
  M1 	   4-6  0x0070	ST Index     Индекс в массиве ST (0-7)
          7   0x0080  ST Mode      0: ST[i]         /  1: ST[i] + sf
  M2     8-10	0x0700	color        Индекс в "массиве" UC (0-7), объявлены в CSS как c0..c7
          11  0x0800	b.blink      0: Disabled      /  1: Active
  H       12 	0x1000	g.visible    0: 0%            /  1: width: sp%
          13 	0x2000	x.class      0: c = UC        /  1: on + blink
          14  0x4000	i.class      0: cd            /  1: on + blink
          15  0x8000	wait React   0: ---           /  1: wait for ext. event
  ===================================================================================
  */
  const
    BT = ["UPLOAD ", "FILE SELECT: ", "PRINT ", "WAIT!", "", "UPLOADING ", "PRINTING ", "ERROR!"],
    ST = ["Syncing", "Click to cancel", "Click to print", "Cancel",  "Ready", "Remote: ", "", "Continue"],
    M = [
      // - PASSIVE  (Z 0-7): Наблюдатель -  sticky i.ena x.ena g.vis b.pulse UC:(unit_color) ST_fn   ST      BT_fn    BT   
      0x0C61, // 0000:  0000 1100 0110 0001   -     -     -     -       +     4 : dark_blue    -   6:""        -   1:"SELECT"
      0xA777, // 0001:  1010 0111 0111 0111   +     -     +     -       -     7 : red          -   7:"Cont"    -   7:"ERROR!"
      0x0603, // 0010:  0000 0110 0000 0011   -     -     -     -       -     6 : gray\steel   -   0:"Sync"    -   3:"WAIT!"
      0x1366, // 0011:  0001 0011 0110 0110   -     -     -     +       -     3 : purple       -   6:"" sp%    -   6:"Pr..ing"
      0x0CD1, // 0100:  0000 1100 1101 0001   -     -     -     -       +     4 : dark_blue    +   5:"R:f_n"   -   1:"SELECT"
      0x116D, // 0101:  0001 0001 0110 1101   -     -     -     +       -     1 : blue         -   6:"" sp%    +   5:"U-ng f_n"
      0x02D2, // 0110:  0000 0010 1101 0010   -     -     -     -      (+?)   2 : green        +   5:"R:f_n"   -   2:"PRINT"
      0x136E, // 0111:  0001 0011 0110 1110   -     -     -     +       -     3 : purple       -   6:"" sp%    +   6:"P-ng f_n"
      //  - ACTIVE  (Z 8-15): Активист --
      0x4C41, // 1000:  0100 1100 0100 0001   -     +     -     -       +     4 : dark_blue    -   4:"Ready"   -   1:"SELECT"
      0xA777, // 1001:  1010 0111 0111 0111   +     -     +     -       -     7 : red          -   7:"Cont"    -   7:"ERROR!"
      0x0603, // 1010:  0000 0110 0000 0011   -     -     -     -       -     6 : gray\steel   -   0:"Sync"    -   3:"WAIT!"
      0x1366, // 1011:  0001 0011 0110 0110   -     -     -     +       -     3 : purple       -   6:"" sp%    -   6:"P..ing"
      0x6838, // 1100:  0110 1000 0011 1000   -     +     +     -       +     0 : magenta      -   3:"Cancel"  +   0:"UP f_n"
      0x116D, // 1101:  0001 0001 0110 1101   -     -     -     +       -     1 : blue         -   6:"" sp%    +   5:"U-ng f_n"
      0x6A3A, // 1110:  0110 1010 0011 1010   -     +     +     -       +     2 : green        -   3:"Cancel"  +   2:"PN f_n"
      0x136E  // 1111:  0001 0011 0110 1110   -     -     -     +       -     3 : purple       -   6:"" sp%    +   6:"P-ng f_n"
    ],
  // радиокнопки активны только для UC[0]
  // для ST[1]..ST[3] назначаем cd класс для в.кнопки
  // для нижн.кнопки бит 13 задает класс bl+can_cancel (==1) , либо класс ci (==0)
  // Тактические патчи (ненулевой tm):
  // при ненулевом ниббле (tm & 0xf) - подменяем биты 0-3 (текст BT), (tm & 0xf00) - биты 8..11 (цвет BT)
  //  T_INIT = 0x0604  // ST:0 «Syncing», BT:4 "", 12=13=14=sticky=0, pulse=0, UC:3 cd
  //  T_WAIT = 0x0603  // ST:0 «Syncing», BT:3 «WAIT!», 12=13=14=sticky=0, pulse=0, UC:3 cd
  //  T_CFRM = 0xA020  // ST:2 «Click to print», BT:0 (Прозрачно), 12=14=0, 13=sticky=1, pulse=0, UC:0
  //  T_CNCL = 0xA010  // ST:1 «Click to cancel», BT:0 (Прозрачно), 12=14=0, 13=sticky=1, pulse=0, UC:0
  //  T_FILE = 0x8009  // ST:0 «Syncing», BT:9 ("F_SEL + f_name"), 12=13=14=0, sticky = 1, pulse=0, UC:0
  //v0  T_LOAD = 0x8008  // ST:0 «Syncing», BT:8 ("UPLOAD + f_name"), 12=13=14=0, sticky = 1, pulse=0, UC:0
  //v1  T_LOAD = 0x860D  // ST:0 «Syncing», BT:5 ("UP-ING + f_name"), 12=13=14=0, sticky = 1, pulse=0, UC:6

  /* 
  ss   (heartbeatState) :	Состояние сервера (0, 1, 2, 3)   { READY_TO_SEL: 0, UPLOADING: 1, READY_TO_PRN: 2, PRINTING: 3 }
  sid  (heartbeatID)    :	ID активного пользователя на сервере
  cid  (myID)           :	Мой сгенерированный ID
  sp   (rProgress)
  f    (file)           : Объект выбранного файла
  o    (offset)         : Смещение (сколько байт передано)
  xt   (resetTimer)     :	Таймер подтверждения отмены (дескриптор, м.б. ===null)
  at   (actTimer)       : Таймер ожидания NEXT запроса
  ct   (reconnectTimer)
  ir   (isReading)      : идет чтение файла
  wn   (pendingNext)    : признак ожидающего запроса NEXT
  wt   (pendingTicks)   : количество HeartBeats где может подтвердиться наша регистрация
  sm   (rMode)          : подтвержденный режим записи файла на Марлин из HeartBeat сообщения
  sf   (rFile)          : подтвержденное имя текущего файла из HeartBeat сообщения
  sp   (rProgress)      : подтвержденный прогресс в % из HeartBeat сообщения
  */
  /*
    to_Marlin:"#28b" /lazy-blue/,   from_Marlin:"#2a6" /lazy-green/,
    srvError:"#a10"  /lazy-red/,     clientError:"#c32" /light-red/,
    srvServ:"#b80"   /dark-yellow/,    clientServ:"#fa5" /light-brown/,
    srvDebug:"#eee"  /light-gray/
    ">":"#28b", "<":"#2a6", ",":"#b80", "!":"#a10"
  */
  // const $ = q => document.getElementById(q),  // Поиск элемента !!удалено как хак-трюк с учетом особенностей движка
  // let b, t, i, x, g, n, rb, fi, l,  // указатели на объекты DOM !!удалено как хак-трюк с учетом особенностей движка

  r = new FileReader(); // const для всего жизненного цикла страницы

  /////// ### debug
  /*let*/ /* hS, hm = 0, *//*nn = 0, lw = [];*/  // в консоли отладчика браузера нужно ввести console.log(lw.join(''));

  let lw = [],
  /*let*/ ws, ts = f = sf = lK = ""  /*имя файла, выбранный файл, контекст UX()*/, 
    nn = o = xt = at = ct = ir = wn = cid = sx = ss = sid = sc = sm = sp = pt = wt = Z = tt = 0,
    tm = 0x0604, /*T_INIT : init system state*/
    E = (v, c) => {
      tm = cid = 0; f = null; v && L(v, c || "#c32"); };   //lc.cEr); };

  r.onload =(e)=> {
    if (!S(e.target.result)) {  // пробуем отправить чанк
      initUp("Upload aborted: WS Lost"); return; } // если неудача
    ir = 0;                     // Освобождаем "замок" вызова
    // Если за время чтения прилетал еще один N:, 
    // не ждем нового события от сети, а сразу идем за следующим куском
    if (wn) { wn -= 1; upNext(); }
  };

  r.onerror =()=> {
    initUp("File Access Error");
    // Информируем сервер, что передача сорвана, и рвем связь
    S("L:" + cid + ":!File Access Error"); 
    ws.close(); 
  };

  function S(v) { //  safeSend
    if (ws?.readyState == 1) { // (ws.readyState == WebSocket.OPEN)
      at = clearTimeout(at);
      if (v instanceof ArrayBuffer) {
        if (sc & 2) {   // hb_разрешение на BIN отправку
          ws.send(v);
          // поскольку отправлены бин.данные - включаем Watchdog (ждем следующего N:) 12 сек (!?смотреть и корректировать)
          at = setTimeout(() => { E("ch_REQ TO!"); ws.close(); }, 12000);
          o += v.byteLength;
              // vvvvvv ### debug vvvvv
              L(`-SS-\x20chunk\x20${++nn}\x20sent\x20OK\x20[${o}/${f.size}]`);
              // ^^^^^ ### debug ^^^^^
        } else L(`-SS-\x20chunk\x20${++nn}\x20ignored`);
      } else { ws.send(v); L(v + " sent OK");}
      return 1; // Успешно отправлено
    }
    L(v + " | Error", "#c32");  // lc.cEr);
    return 0; // Отправить не удалось, по желанию логируем проблему
  }

  function fSel(p = 0) {
    let v = fi.files && fi.files[0];
    // 1. Если файл выбран — обновляем локальный объект
    if (v) f = v;
    // 2. Если мы еще не являемся активистом
    if (Z < 8) cid = (Date.now() & 0xffff) || 1;  // newID
    // 3. Формируем сообщение серверу
    if (p)
      // формирование команды печати подразумевает наличие f
      v = "P:" + cid;
    else {
      // формирование команды регистрации активного файла
      v = "F:" + cid;
      // Если f есть — полный формат, иначе — короткий "F:ID"
      if (f?.size) v += ":" + f.size + ":" + f.name;
      }
    if (S(v)) { tm = 0x8009; wt = 0; }  // Отправляем и при успехе взводим sticky T_FILE tm = 0x8009
    // Сброс инпута для повторного выбора того же файла
    fi.value = ""; 
    UX();
  }

  // Обработка верхней кнопки (SELECT, UPLOAD, PRINT)
  function mBtn() {
    nn = o = 0;
    //if ((Z & 7) == 6) { // запуск печати
    if (Z == 14) { // запуск печати
      tm = 0xA020;      // T_CFRM: "PRINT <f_name>" сверху, "Click to print" снизу
      xt = setTimeout(() => tm = xt = 0, 9000);
    } else if (Z == 12) {
      S("S:" + (rb[0].checked ? 0 : 1));
      tm = 0x860D;      // T_LOAD: "UPLOAD <f_name>" сверху, "Syncing" снизу
      at = setTimeout(() => { E("ch_REQ TO!"); ws.close(); }, 12000);
    } else fi.click();
    UX();
  }

  // Обработка нижней кнопки (Confirm, Cancel, Continue)
  function qBtn() {
    // Если таймер уже запущен — это ВТОРОЙ клик
    if (xt) {
      clearTimeout(xt); xt = 0;
      if ((tm & 0xf0f0) == 0xA020) fSel(1);
      else {
        // 1. Убираем признак наличия файла
        f = null; fi.value = ""; // Сброс системного инпута
        L("Selection cleared");
        // (теперь Z станет "Активист без файла")
        fSel(); 
      }
    }
    // Если таймера нет — это ПЕРВЫЙ клик
    else if (tm == 0xA777) tm = 0; // если внизу "Continue", то сброс "залипания"
    else {
      tm = 0xA010;  // для "Cancel" изменяем tm = T_CNCL
      // Разрешаем ожидание подтверждения на 6 секунд
      xt = setTimeout(() => tm = xt = 0, 9000); // Возвращаем обычный текст, когда время вышло
      }
    // Сразу обновляем UI
    UX();
  }

  function UX() {
    let c, v = M[Z];         // Базовая маска из массива
    //let c, v0 = v = M[Z];  // v0 - for ### debug only
    // накладываем тактический патч
    // ненулевая маска "липнет" всегда, кроме ситуации ошибки
    // маска м.б. "прозрачная" для текста и\или цвета в.кнопки
    if (tm && v != 0xA777) v = (v & ~((tm & 0xf ? 0xf : 0) | (tm & 0xf00 ? 0xf00 : 0) | 0xf0f0)) | tm;
    let nK = sx + sp + (f?.name || "") + v + Z + tm; // текущий "контекст"
    if (lK == nK) return;    // "контекст" не изменился - перерисовывать нечего
    lK = nK;
                              //vvvvvvvv ### debug vvvvvvvv
    // //let bTxt = BT[v & 7] + (v >> 3 & 1 ? (f?.name || sf || "") : "");
    // //let sTxt = v >> 12 & 1 ? sp + "%" : ST[v >> 4 & 7] + (v >> 7 & 1 ? (sf || "") : "");
    //L(`-UX-\x20Z:${Z}\x20\x20M:0x${v0.toString(16)}\x20\x20tm:0x${tm.toString(16)}\x20\x20v:0x${v.toString(16)}`);
    // //L(`-UX-\x20BT[${v & 7}]:"${bTxt}"`);      // для кнопок
    // //L(`-UX-\x20ST[${v >> 4 & 7}]:"${sTxt}"`); // для статуса
                              // ^^^^^^^^^ ### debug ^^^^^^^^^
    tm = v;
      // L-ниббл управляет текстом верхней кнопки
      // Биты 0..2 - индекс текста в массиве BT
      // Если бит_3=1 — добавляем имя файла к тексту из массива BT
    c = (Z < 8) ? ((f && f.name) || sf || "") : (sf || (f && f.name) || "");
    b.innerText = BT[v & 7] + (v & 8 ? c : "");
    v >>= 4;    // сдвигаем : L-ниббл управляет текстом нижней кнопки
      // Если бит_7=1 — добавляем имя файла к тексту статуса из массива ST
    let l = ST[v & 7] + (v & 8 ? (sf || ""): ""), // текст статуса l сохраняем для %
          // Порядок операторов и дублирование v & 7 и v & 8 оставлены умышленно!
          // В таком виде Zopfli строит более эффективное дерево Хаффмана (-7 байт в GZIP).
        u = v & 7,  // биты 4..6 - индекс текста в массиве ST
                    // если (!u) - неактивны р-кнопки. приглушен UC, если (!u) и sticky = 1
        p = v & 8;  // бит_7 - если бит_7=1 — добавляем имя файла к тексту из массива ST
    v >>= 4;    // сдвигаем : L-ниббл управляет цветом верхней кнопки (UC[]) и радиокнопками
    //радиокнопки кликабельны только при UC[0] + ST[3] -> !(v & 7) && (u == 3)
    c = (v & 7) || (u != 3);  // UC[] != magenta || ST[] != "Cancel" : для disabled
    rb.forEach ((r, j) => { r.disabled = c; if (c && sf) r.checked = sm == j; });
    c = v & 7;  // номер цвета в.кнопки : биты 8..10
    p = v & 8;  // кликабельность (мерцание) : бит 11
    v >>= 4;    // сдвигаем : L-ниббл управляет отображением прогресса и классами кнопок
        // ВЕРХНЯЯ КНОПКА :
        // Цвет = с
        // приглушение (+ " cd"), когда ST[0]..ST[2] (u<3) и цвет в.кнопки != c6
        // акт.мерцание (+ " on bl"), бит_13=1 без Sticky
        //b.className = `c c${v & 7} ${u < 3 ? "cd" : "on"} ${u & 8 ? "" : " bl"}`;
          //vvvvvvvv # debug vvvvvvvvvv
    //  let cN = "c c" + c + ((v < 8) && p ? " on bl" : (u < 3 && (c != 6) ? " cd" : ""));
    //  L("-UX- BC: '" + cN + "'", "#2a6"); //lc.frM);
    //  b.className = cN;
          //^^^^^^^^ ### debug ^^^^^^^^^
    b.className = "c c" + c + ((v < 8) && p ? " on bl" : (u < 3 && (c != 6) ? " cd" : ""));
        // НИЖНЯЯ КНОПКА :
    g.style.width = sp + "%"; // ширина наложения шкалы прогресса
        // текст : Если бит_12 = 1, то пишем только %, иначе берем полученное ранее l = ST[]
    t.innerText = v & 1 ? sp + "%" : l;
        // Цвет = "c с5 on bl" если бит_13=1 либо выбор между цветом верхней кнопки (c), если бит_12=1
        // ??приглушение (+ " cd"), если бит_12=1
          //vvvvvvvv # debug vvvvvvvvvv
    //  cN = v & 2 ? "c c5 on bl" : ("c c" + (v & 1 ? c : ("6" + (u != 6 ? " cb" : "")))) ; // если бит_13=0 - для прогресса берем цвет верхней кнопки
    //  L("-UX- XС: '" + cN + "'", "#2a6"); //lc.frM);
    //  x.className = cN;
          //^^^^^^^^ ### debug ^^^^^^^^^
        // если бит_13=0 - для прогресса берем цвет верхней кнопки
    x.className = v & 2 ? "c c5 on bl" : ("c c" + (v & 1 ? c : ("6" + (u != 6 ? " cb" : "")))) ;
          //vvvvvvvv # debug vvvvvvvvvv
    //  cN = v & 4 ? "c c3 on bl" : "c cd";  // кнопка "SEND"
    //  L("-UX- SС: '" + cN + "'", "#2a6"); //lc.frM);
    //  s.className = cN ;
          //^^^^^^^^ ### debug ^^^^^^^^^
    s.className = v & 4 ? "c c3 on bl" : "c cd";  // кнопка "SEND"
          //vvvvvvvv # debug vvvvvvvvvv
    //  cN = v & 4 ? "c on bl" : "c cd";          // поле ввода G-команд
    //  L("-UX- IС: '" + cN + "'", "#2a6"); //lc.frM);
    //  i.className = cN;          // поле ввода G-команд
          //^^^^^^^^ ### debug ^^^^^^^^^
    i.className = v & 4 ? "c on bl" : "c cd";     // поле ввода G-команд
    // фиксируем sticky эффект (Бит 15)
    if (v < 8) tm = 0;  // если 1 -> tm сбросится только в соотв. обработчике события
  }
        
  function L(m, c = "#fa5") {   // Лог (Log)
    let v = new Date();
    if ((sc & 1) && Z > 7) {  // только активист может сохранять лог
            //vvvvvvvv # debug vvvvvvvvvv
      // сообщения клиента идут без метки времени помещения в буфер сервера
      if (c == "#c32" || c == "#fa5") pt = "";
      // Добавляем штамп времени перед сообщением
      // Формат hh:mm:ss.ms
      ts = v.toTimeString().slice(0, 8) + '.' + v.getMilliseconds().toString().padStart(3, '0');
      lw.push(`${ts}\x20${nn}\x20${pt}\x20${m}`);
          //^^^^^^^^ ### debug ^^^^^^^^^
      }
     else { lw = []; if (c == "#fa5") return; }
    v = document.createElement('div');
    v.style.color = c;
    v.innerText = m ;
    //let v = document.createElement('div'); v.style.color = c; v.innerText = m;
    if (c == "#a10") v.style.fontWeight = "bold"; // lc.sEr
    //if (c == lc.cEr || c == lc.cSv) v.style.fontStyle = "italic";
    if (c == "#c32" || c == "#fa5") v.style.fontStyle = "italic";
    l.appendChild(v);
    if (l.childNodes.length > 200) l.removeChild(l.firstChild);
    // Внутри функции L(m, c) после l.appendChild(v)
    // Проверяем: если юзер отмотал вверх более чем на 60px - не скроллим (даем читать)
    // Иначе - всегда прижимаем к низу.
    if (l.scrollHeight - l.clientHeight <= l.scrollTop + 60) l.scrollTop = l.scrollHeight;  //isAtBottom
    //v.scrollIntoView({ behavior: 'smooth', block: 'end' }); // альтернативный , но ресурсоемкий способ
  }

  function initUp(v = null, c = "#c32") { // lc.cEr
    ir = wn = /*tm =*/ 0;
    //if (tm != 0xA777) tm = 0;
    if (v) L(v, c);
  }

  function connect() {
    if (ws) { ws.onclose = null; ws.close(); }
    clearTimeout(ct);
    ws = new WebSocket('ws://' + location.hostname + ':81'); ws.binaryType = 'arraybuffer';
    
    ws.onopen=()=> { 
      n.innerText = "ONLINE"; n.className = "online"; n.style.opacity = "1";
      initUp("Connected", "#eee");     //lc.sDb цветом чтобы точно напечатало
      // initUp("Connected", "#fa5");  //lc.cSv);
      // Авто-заявка после реконнекта
      if (f && cid) fSel();
    };

    ws.onclose=()=> { 
      n.innerText = "OFFLINE"; n.className = "offline";
      pt && initUp("WS closed");  // [if pt] чтобы избежать скроллинга при длительном дисконнекте
      sc = pt = tt = sid = sp = 0; sf = "";
      if (Z == 10) f = null;
      if (tm != 0xA777) tm = 0x0603; // T_WAIT если нет ошибки
      UX();
      ct = setTimeout(connect, 2000); 
    };

    ws.onmessage = e => {
      // Универсальная трансформация в строку:
      // Если это ArrayBuffer (есть byteLength), декодируем. Иначе берем как есть.
      let d = e.data.byteLength >= 0 ? new TextDecoder().decode(e.data) : e.data;
      // Теперь d либо строка, либо (вдруг) Blob/объект. 
      // 1. Чтобы split не упал, проверяем, что d — это строка.
      // формат пакета - <N>:payload_message
      // 2. мин. размер пакета = 16 = 13 (time) + 2 (hc) + 1 (min data)
      if (typeof d != "string" || d.length < 16) return;
      pt = d.slice(0, 13);          // "12:34:56.789:" метка времени помещения пакета в буфер
      for (let hb of d.slice(13).split("\n")) {
        //// формат Heartbeat - H:RndNum:State[:ActiveID[:Progress:Mode:File]]
        let h = hb.split(":"), hc = h[0], v = hb.slice(2); // hc - это "H", "N" или "L"
        if (hc == "H") {
              // vvvvvv ### debug vvvvv
              //let hl = "";
              //if (v != hS | tm != hm) {
              //  L(`${pt}\x20-HB-\x20"${hb}"`); //lc.sDb);
              //  hl = "-HB- tm : 0x" + tm.toString(16) + " >> ";
              //}
              // ^^^^^ ### debug ^^^^^
          if (h.length > 2) {
            // Ручное присваивание для плавающего формата (Zopfli-стайл)
            sx  = +h[1] || 0; // id сессии сервера для контекста UX
            ss  = +h[2] || 0; // состояние системы
            sid = +h[3] || 0; // id текущего активиста
            sc  = +h[4] || 0; // ctrl : бит_0 - лог вкл/выкл, бит_1 - BIN передача вкл/выкл
            sm  = +h[5] || 0; // mode : 0 = ASCII, 1 = BFT
            sp  = +h[6] || 0; // progress : 0..100
            sf  = h[7] || ""; // f_name
            // Формируем 4-битный глобальный индекс состояния Z (0-15)
            Z = (cid == sid & !!cid) << 3 | !!sf << 2 | ss & 3;
            if (sc < 2 && at) at = clearTimeout(at); // hb ctrl запрещает BIN отправку - сбрасываем таймер
            // Пульс ONLINE
            n.className == "online" && (n.style.opacity = n.style.opacity == .5 ? 1 : .5);
            // Тактика сессий
            if ((tm & 0xf0ff) == 0x8009) {
              if (Z > 7)  tm = 0;               // авторизовались, сбрасываем sticky tm
              else if (++wt > 2) E("Conflict"); // нашу заявку отклонили, отползаем, сбрасываем sticky tm
            }
            if (tm == 0 && Z < 8) {             // !isPending && мы не активист
              if (!cid || !sid) f && fSel();    // если хоть один из нас свободен и
                                                // если есть файл, но нет ID - пытаемся автоматом авторизоваться
              else E("Role lost");              // Active role was lost : Takeover
            }
                  // vvvvvv ### debug vvvvv
                  //if (hl) {
                  //  L(hl + "0x" + tm.toString(16)); //lc.sDb);
                  //  hS = v; hm = tm;
                  //}
                  // ^^^^^ ### debug ^^^^^
            // --- ТАКТОВЫЙ ГИСТЕРЕЗИС И ОТПРАВКА СИНХРОПАКЕТА ВРЕМЕНИ ---
            // --- СЧЕТЧИК ТАКТОВ НА ОСНОВЕ HB (стартовая пауза 4 сек, затем повтор каждые 5 минут) ---
            // --- При условии : либо мы активист и нет BIN передачи, либо сейчас нет активиста ---
            if (tt++ == 4) (Z > 7 ? sc < 2 : !sid) && S("T:" + Math.floor(Date.now() / 1000 - new Date().getTimezoneOffset() * 60)); // Даем синхронизацию времени
            tt > 303 && (tt = 4);               // (304 - 4) - 1 = ровно 300 секунд (5 минут) между синхропакетами
            UX();                               // отрисовка
          }
        } else if (hc == "L") {
          // Принудительный скролл, если терминальная команда
          v[0] == ':' && (l.scrollTop = l.scrollHeight);
          // Изящный маппинг: если первый символ в ключе - берем цвет и режем, иначе дефолт
          let c = {">":"#28b", "<":"#2a6", ",":"#b80", "!":"#a10"} [v[0]];
          L(c ? v.slice(1) : v, c || "#eee");
        } else if (hc == "N" && cid == v) upNext();        // Запрос следующего чанка
      }
    };

    ws.onerror=()=> { ws.close(); }; //чтобы скрипт не «вешался», перезапуск через 2с
  }

  function upNext() {
                  // vvvvvv ### debug vvvvv
    L(pt + "\x20-NX-\x20CHUNK_REQ");
                  // ^^^^^ ### debug ^^^^^
    if (tm == 0x860D) tm = 0;        // UI будет обновлен ближайшим HeartBeat
    if (!f) return;
    // СЛУЧАЙ А: Мы заняты чтением. Просто запоминаем наличие запроса.
    if (ir) { wn += 1; return; }
    // СЛУЧАЙ Б: Мы свободны. Запускаем "конвейер"
    if (o < f.size) {
      ir = 1;     // флаг занятости
      let v = f.slice(o, o + 1230); // 984); // 1230); // 1024);
      r.readAsArrayBuffer(v);
    } else
      S("E:" + f.size);
  }

  function gCmd() {
    let v = i.value.trim();
    if (v && cid && sid == cid) {
      S(v.toUpperCase()); i.value = "";
      //if (window.innerWidth > 600) i.focus(); else i.blur();
      ('ontouchstart' in window) ? i.blur() : i.focus();
    }
  };

  // Регистрация элементов DOM и событий. Запуск при загрузке
  document.addEventListener('DOMContentLoaded', () => {
    /* --- весь "захват" удален как хак-трюк с учетом особенностей движка ---
       Нам НЕ НУЖНО писать b = $('upBtn'), n = $('netStat') и т.д.
       Браузер уже создал глобальные переменные: upBtn, netStat, statText, fi, log, cmdInp...
    // 1. "Захватываем" элементы в кэш один раз при старте
    fi = $('fi');                         // file selector
    n = $('netStat');                     // link state info
    b = $('upBtn');                       // Button (select\start)
    t = $('statText');                    // Text (status)
    x = $('statBox');                     // Box (Cancel area)
    g = $('progFill');                    // Gauge (Progress)
    l = $('log');                         // Log
    i = $('cmdInp');                      // Input (Terminal)
    */
    // Единственное, что нужно захватить вручную (т.к. у них name, а не id):
    rb = document.getElementsByName('m'); // radio-buttons ([0] - Text, [1] - BFT)
    // 2. Вешаем событие на терминал (через уже готовую переменную i)
    i.onkeypress = (e) => { if (e.key == 'Enter') gCmd(); };
    // УНИВЕРСАЛЬНЫЙ СЕТЕВОЙ UX-МОСТ ДЛЯ ЛЮБЫХ УСТРОЙСТВ И HA
    i.onblur = () => { window.scrollTo(0, 0); }; // Возврат экрана на смартфонах
    if (window != window.top) document.body.style.overflow = "hidden"; // Защита iframe в HA
    UX();
    // 4. Запускаем связь
    connect(); 
  });
  /*
                  // vvvvvv ### debug vvvvv
                  // можно вызвать в консоли отладчика для получения ссылки загрузки лога lw
                  // но он требует https://
                  function saveLog() {
                    const b = new Blob(lw, {type: 'text/plain'});
                    const a = document.createElement('a');
                    a.href = URL.createObjectURL(b);
                    a.download = `log_${Date.now()}.txt`;
                    a.click();
                  }
                  // ^^^^^ ### debug ^^^^^
  */
  // выгрузка лога по дв.клику по самому окну лога
  l.ondblclick = () => {
      const a = document.createElement('a');
      // Кодируем содержимое lw в Base64 или просто через encodeURIComponent
      a.href = 'data:text/plain;charset=utf-8,' + encodeURIComponent(lw.join('\n'));
      a.download = 'debug.log';
      a.click();
  };
</script>
</body></html>
)=====";
