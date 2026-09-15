/* marlin-move-control-card.js_v22.0.0 */
class PrinterControlCard extends HTMLElement {
  setConfig(config) {
    this.mqtt_prefix = "marlin_bridge";
    if (!config.device_id) throw new Error("?? device ID string");
    this.config = config;
    this.device_id = config.device_id;
    // 1. Сохраняем оригинальный регистр для отправки команд в топики MQTT (например, "IoT_7")
    this.device_id = config.device_id;
    // 2. Создаем специальную переменную в нижнем регистре для имен сущностей Home Assistant ("iot_7")
    this.entity_id_prefix = config.device_id.toLowerCase();
    this.currentStep = 1;
    this.isMoving = false;
    this.secondsElapsed = 0;
    this.timerInterval = null;
    this.moveTimeout = null;
    this.targetCoords = { X: null, Y: null, Z: null };
    this._motionEndTime = null;
    this._showBarTimeout = null;
    this.softEndstopsEnabled = true;
    this._dimensionsInterval = null; 
    // Параметры накопления кликов джойстика
    this._accumulateTimeout = null;
    this._accumulatedDist = 0;
    this._activeAccumulateAxis = null; 
    this._accumulateDelay = config.accumulate_delay || 1500;       
    this._maxAccumulateLimit = config.max_accumulate_limit || 50;     

    // Генерируем свой персональный ID
    this.card_id = (((Date.now() & 0xffff) || 1) << 16) >>> 0;

    // Настройка Т-пресетов из YAML
    this.temp_presets = config.temp_presets || [
      { h: 215, b: 60 },
      { h: 245, b: 100 },
      { h: 235, b: 80 }
    ];

    // Настройка пользовательских макросов из YAML с безопасным дефолтом
    this.macro_buttons = config.macro_buttons || [
      { name: "Steppers Off", gcode: "M84\n" },
      { name: "All Off", gcode: "M104 S0\nM140 S0\nM107\n" }
    ];
  }

  set hass(hass) {
    this._hass = hass;
    if (!this.content) {
      this.innerHTML = `
        <ha-card style="max-width: 324px; margin: 0 auto; box-shadow: var(--ha-card-box-shadow, none); border-radius: var(--ha-card-border-radius, 12px);">
          <style>
            .pad-container { display: flex; flex-direction: column; align-items: center; padding: 12px; font-family: sans-serif; box-sizing: border-box; }
            .card-title { font-size: 1.2em; font-weight: 500; margin: 4px 0 12px 0; text-align: center; color: var(--primary-text-color); }
            
            .triple-row-grid { 
              display: grid; 
              grid-template-columns: repeat(3, 94px); 
              grid-gap: 4px; 
              width: 290px; 
              justify-content: center;
              box-sizing: border-box;
            }
            
            .frame-btn-copy { 
              width: 94px; 
              height: 36px; 
              background-color: var(--card-background-color); 
              color: var(--primary-text-color); 
              border: 1px solid var(--divider-color); 
              border-radius: 4px; 
              display: flex; 
              align-items: center; 
              justify-content: center; 
              font-family: monospace; 
              font-size: 1.05em; 
              padding: 0; 
              margin: 0;
              cursor: pointer;
              box-sizing: border-box;
              transition: background-color 0.2s, color 0.2s; 
            }
            .frame-btn-copy:hover { background-color: var(--secondary-background-color, #eceff1); color: var(--accent-color); }
            
            .frame-btn-copy.active { 
              background-color: var(--accent-color) !important; 
              color: white !important; 
              border-color: var(--accent-color);
            }

            .frame-btn-copy.active-macro {
              background-color: var(--accent-color) !important;
              color: white !important;
              border-color: var(--accent-color);
              animation: macro-pulse 1.8s ease-in-out infinite;
            }
            
            @keyframes macro-pulse {
              0% {
                opacity: 1;
                box-shadow: 0 0 0 0 rgba(var(--rgb-accent-color, 33, 150, 243), 0.4);
              }
              50% {
                opacity: 0.65; /* Мягкое приглушение яркости в середине цикла */
                box-shadow: 0 0 8px 2px rgba(var(--rgb-accent-color, 33, 150, 243), 0.2);
              }
              100% {
                opacity: 1;
                box-shadow: 0 0 0 0 rgba(var(--rgb-accent-color, 33, 150, 243), 0);
              }
            }
            
            .control-container-matrix { 
              display: grid; 
              grid-template-columns: repeat(6, 48px); 
              grid-gap: 3px; 
              width: 303px; 
              margin: 10px 0 8px 0; 
              box-sizing: border-box; 
              justify-content: center;
            }
            
            .xy-grid-cell { display: grid; grid-template-columns: repeat(3, 48px); grid-gap: 3px; grid-column: 1 / span 3; }
            .z-grid-cell { display: grid; grid-template-columns: repeat(2, 48px); grid-gap: 3px; grid-column: 5 / span 2; }
            
            .center-axis-divider {
              grid-column: 4;
              grid-row: 1 / span 3;
              border-left: 1px dashed var(--divider-color);
              width: 0;
              height: 100%;
              justify-self: center;
              pointer-events: none;
            }
            
            button.move-btn { width: 48px; height: 40px; background-color: var(--primary-color); color: var(--text-primary-color); border: none; border-radius: 4px; cursor: pointer; font-weight: bold; text-align: center; line-height: 40px; transition: all 0.2s; padding: 0; margin: 0; box-sizing: border-box; }
            button.move-btn:active { background-color: var(--accent-color); }
            
            .diag-btn { background-color: var(--secondary-background-color, #eceff1); color: var(--primary-text-color); font-size: 1.1em; }
            
            button.home-btn { width: 48px; height: 40px; background-color: var(--card-background-color); color: var(--secondary-text-color); border: 1px solid var(--divider-color); border-radius: 4px; cursor: pointer; font-weight: bold; text-align: center; line-height: 38px; font-size: 0.85em; transition: all 0.2s; padding: 0; margin: 0; box-sizing: border-box; }
            button.home-btn:active { background-color: var(--accent-color); color: white; }
            
            .z-double-span { grid-column: 1 / span 2; width: 48px !important; justify-self: center; }
            
            .steps-container { width: 300px; min-height: 46px; display: flex; justify-content: center; align-items: center; margin-bottom: 4px; border-bottom: 1px solid var(--divider-color); padding-bottom: 10px; box-sizing: border-box; }
            
            .cancel-bar { display: none; width: 290px; background-color: var(--warning-color, #ff9800); color: white; border: none; padding: 10px; border-radius: 4px; font-weight: bold; cursor: pointer; text-align: center; box-shadow: 0 0 6px var(--warning-color, #ff9800); height: 36px; line-height: 16px; box-sizing: border-box; }
            
            .section-container-grid { width: 300px; display: flex; justify-content: center; margin-top: 12px; border-top: 1px solid var(--divider-color); padding-top: 12px; padding-bottom: 2px; box-sizing: border-box; }
            
            .moving { animation: blinker 1.5s linear infinite; }
            @keyframes blinker { 50% { opacity: 0.6; } }
          </style>
          
          <div class="pad-container">
            <div class="card-title">Marlin-WiFi ready</div>
            
            <div class="triple-row-grid" id="disp-panel">
              <button class="frame-btn-copy" id="click-x">X: <span id="val-x">---</span></button>
              <button class="frame-btn-copy" id="click-y">Y: <span id="val-y">---</span></button>
              <button class="frame-btn-copy" id="click-z">Z: <span id="val-z">---</span></button>
            </div>
            
            <div class="steps-container" style="margin-top: 12px;">
              <div id="steps-wrapper" class="triple-row-grid">
                <button id="step-01" class="frame-btn-copy">0.1 mm</button>
                <button id="step-1" class="frame-btn-copy active">1 mm</button>
                <button id="step-10" class="frame-btn-copy">10 mm</button>
              </div>
              <button id="cancel-move-bar" class="cancel-bar">Moving... (0s) [Cancel]</button>
            </div>

            <style>
              .settings-row { width: 300px; display: flex; align-items: center; justify-content: flex-start; margin: 4px 0 8px 12px; font-size: 0.9em; color: var(--secondary-text-color); box-sizing: border-box; }
              .settings-row input[type="checkbox"] { margin-right: 8px; width: 16px; height: 16px; cursor: pointer; accent-color: var(--accent-color); }
              .settings-row label { cursor: pointer; user-select: none; }
            </style>

            <div class="settings-row">
              <input type="checkbox" id="soft-endstops-chk" checked>
              <label for="soft-endstops-chk">Use software endstops</label>
            </div>

            <div class="control-container-matrix">
              <div class="xy-grid-cell">
                <button class="move-btn diag-btn" id="move-x-minus-y-plus">↖</button>
                <button class="move-btn" id="move-y-plus">Y+</button>
                <button class="move-btn diag-btn" id="move-x-plus-y-plus">↗</button>
              </div>
              <div class="center-axis-divider"></div>
              <div class="z-grid-cell">
                <button class="move-btn z-double-span" id="move-z-plus">Z+</button>
              </div>
              
              <div class="xy-grid-cell">
                <button class="move-btn" id="move-x-minus">X-</button>
                <button class="home-btn" id="home-xy">XY</button>
                <button class="move-btn" id="move-x-plus">X+</button>
              </div>
              <div class="z-grid-cell">
                <button class="home-btn" id="zero-z">Z=0</button>
                <button class="home-btn" id="home-z">Z</button>
              </div>
              
              <div class="xy-grid-cell">
                <button class="move-btn diag-btn" id="move-x-minus-y-minus">↙</button>
                <button class="move-btn" id="move-y-minus">Y-</button>
                <button class="move-btn diag-btn" id="move-x-plus-y-minus">↘</button>
              </div>
              <div class="z-grid-cell">
                <button class="move-btn z-double-span" id="move-z-minus">Z-</button>
              </div>
            </div>
            
            <!-- Ряды Температур -->
            <div class="section-container-grid">
              <div class="triple-row-grid" id="temp-wrapper"></div>
            </div>

            <!-- НОВЫЙ БЛОК: Ряды пользовательских макросов -->
            <div class="section-container-grid">
              <div class="triple-row-grid" id="macro-wrapper"></div>
            </div>
          </div>
        </ha-card>
      `;
      this.content = true;
      this._attachEventListeners();
    }
    this._updateCoordinates();
  }

  _updateCoordinates() {
    const gObj = this._hass.states['text.' + this.entity_id_prefix + '_g'];
    let limits = {
      full: { min: { x: 0.00, y: 0.00, z: 0.00 }, max: { x: 0.00, y: 0.00, z: 0.00 } },
      work: { min: { x: 0.00, y: 0.00, z: 0.00 }, max: { x: 0.00, y: 0.00, z: 0.00 } }
    };
    // Если мост уже прислал габариты, заменяем дефолтные значения на реальные
    if (gObj && gObj.attributes && gObj.attributes.dimensions) {
      limits = gObj.attributes.dimensions.area;
    }

    const xObj = this._hass.states['sensor.' + this.entity_id_prefix + '_x'];
    const yObj = this._hass.states['sensor.' + this.entity_id_prefix + '_y'];
    const zObj = this._hass.states['sensor.' + this.entity_id_prefix + '_z'];
    
    if (xObj && yObj && zObj && xObj.state !== undefined && yObj.state !== undefined && zObj.state !== undefined) {
      const valX = parseFloat(xObj.state);
      const valY = parseFloat(yObj.state);
      const valZ = parseFloat(zObj.state);
      
      if (this.isMoving) {
        let xReached = (this.targetCoords.X === null) || (Math.abs(valX - this.targetCoords.X) < 0.02);
        let yReached = (this.targetCoords.Y === null) || (Math.abs(valY - this.targetCoords.Y) < 0.02);
        let zReached = (this.targetCoords.Z === null) || (Math.abs(valZ - this.targetCoords.Z) < 0.02);
        
        const timeIsUp = (this._motionEndTime === null) || (Date.now() >= this._motionEndTime);

        if (xReached && yReached && zReached && timeIsUp) {
          this._resetLockState();
        }
      }
      
      // ДОБАВЛЕНО: Принудительно дергаем диспетчер блокировок при каждом обновлении координат,
      // чтобы состояние кнопок на 100% соответствовало текущему флагу this.isMoving
      this._toggleButtons();

      const xSpan = this.querySelector('#val-x');
      const ySpan = this.querySelector('#val-y');
      const zSpan = this.querySelector('#val-z');
      
      if (xSpan) xSpan.innerText = valX.toFixed(2);
      if (ySpan) ySpan.innerText = valY.toFixed(2);
      if (zSpan) zSpan.innerText = valZ.toFixed(2);
    }
  }

  _clearActiveMacros() {
    this.querySelectorAll('#macro-wrapper .frame-btn-copy').forEach(b => {
      b.classList.remove('active-macro');
      b.dataset.isActiveMacro = 'false';
    });
  }

  _resetLockState() {
    this.isMoving = false;
    this._isAccumulating = false;
    this._activeAccumulateAxis = null;
    this._accumulatedDist = 0;
    
    if (this._accumulateTimeout) clearTimeout(this._accumulateTimeout);
    if (this._showBarTimeout) clearTimeout(this._showBarTimeout);
    if (this.moveTimeout) clearTimeout(this.moveTimeout);
    if (this.timerInterval) clearInterval(this.timerInterval);
    
    this.targetCoords = { X: null, Y: null, Z: null };
    this._motionEndTime = null;
    
    this._toggleButtons(); // Автоматический пересчет глобального состояния
    this.querySelector('#disp-panel').classList.remove('moving');
    this.querySelectorAll('#disp-panel span').forEach(span => { span.style.opacity = '1'; });
    
    this.querySelector('#steps-wrapper').style.display = 'grid'; 
    this.querySelector('#cancel-move-bar').style.display = 'none';
  }

  /* МАТРИЧНЫЙ ДИСПЕТЧЕР БЛОКИРОВОК (Центральный узел глобальной логики) */
  _toggleButtons() {
    const cardRoot = this.shadowRoot || this; 
    if (!cardRoot) return;
  
    const gObj = this._hass.states['text.' + this.entity_id_prefix + '_g'];
    const isOffline    = !gObj || gObj.state === 'OFFLINE' || gObj.state === 'unavailable';
    const isServerBusy = !isOffline && gObj.state !== 'IDLE' && gObj.state !== 'READY';
    const noLimits     = !gObj || !gObj.attributes || !gObj.attributes.dimensions;
    
    const uObj = this._hass.states['number.' + this.entity_id_prefix + '_u'];
    const currentActiveSid = uObj ? parseInt(uObj.state) : 0;
    
    const isChannelBusy = (currentActiveSid !== 0 && currentActiveSid !== this.card_id);
  
    if (isOffline || noLimits || isChannelBusy || isServerBusy) {
      cardRoot.style.pointerEvents = 'auto'; 
      this.querySelectorAll('.move-btn, .home-btn, .frame-btn-copy').forEach(btn => {
        btn.style.setProperty('pointer-events', 'none', 'important');
        btn.style.setProperty('opacity', '0.5', 'important');
      });
      const title = this.querySelector('.card-title');
      if (title) {
        if (isOffline) title.innerText          = 'Printer is OFFLINE';
        else if (isServerBusy) title.innerText  = 'Printer is busy...';
        else if (isChannelBusy) title.innerText = 'Line is busy...';
        else title.innerText                    = 'Getting dimensions...';
      }
      return; 
    }
  
    const title = this.querySelector('.card-title');
    if (title) title.innerText = 'Marlin-WiFi ready';

    let activeLoopMacro = this.querySelector('#macro-wrapper button[data-is-active-macro="true"][data-macro-mode="loop"]');
    if (activeLoopMacro) {
      cardRoot.style.pointerEvents = 'auto';
      this.querySelectorAll('.move-btn, .home-btn, .frame-btn-copy').forEach(btn => {
        if (btn !== activeLoopMacro) {
          btn.style.setProperty('pointer-events', 'none', 'important');
          btn.style.setProperty('opacity', '0.3', 'important');
        } else {
          btn.style.removeProperty('pointer-events');
          btn.style.removeProperty('opacity');
        }
      });
      return;
    }

    if (this.isMoving && !this._isAccumulating) {
      const cancelBar = this.querySelector('#cancel-move-bar');
      cardRoot.style.pointerEvents = 'auto'; 
      this.querySelectorAll('.move-btn, .home-btn, .frame-btn-copy').forEach(btn => {
        if (btn !== cancelBar) {
          btn.style.setProperty('pointer-events', 'none', 'important'); 
          btn.style.setProperty('opacity', '0.3', 'important');        
        } else {
          btn.style.removeProperty('pointer-events');
          btn.style.removeProperty('opacity');
        }
      });
      return;
    }

    if (this._isAccumulating) {
      cardRoot.style.pointerEvents = 'auto';
      this.querySelectorAll('.move-btn, .home-btn, .frame-btn-copy').forEach(btn => {
        if (btn.id !== this._activeAccumulateAxis) {
          btn.style.setProperty('pointer-events', 'none', 'important');
          btn.style.setProperty('opacity', '0.4', 'important');        
        } else {
          btn.style.removeProperty('pointer-events');
          btn.style.removeProperty('opacity');
        }
      });
      return;
    }

    // Сценарий 4: SYS_IDLE
    cardRoot.style.pointerEvents = 'auto';
    
    this.querySelectorAll('.move-btn, .home-btn, .frame-btn-copy').forEach(btn => {
      btn.style.removeProperty('pointer-events');
      btn.style.removeProperty('opacity');
    });

    if (this.softEndstopsEnabled) {
      const limits = gObj.attributes.dimensions.area;
      const xObj = this._hass.states['sensor.' + this.entity_id_prefix + '_x'];
      const yObj = this._hass.states['sensor.' + this.entity_id_prefix + '_y'];
      const zObj = this._hass.states['sensor.' + this.entity_id_prefix + '_z'];

      if (xObj && yObj && zObj) {
        const valX = parseFloat(xObj.state);
        const valY = parseFloat(yObj.state);
        const valZ = parseFloat(zObj.state);

        const blockRules = {
          'move-x-plus':       (valX >= limits.full.max.x),
          'move-x-minus':      (valX <= limits.full.min.x),
          'move-y-plus':       (valY >= limits.full.max.y),
          'move-y-minus':      (valY <= limits.full.min.y),
          'move-z-plus':       (valZ >= limits.full.max.z),
          'move-z-minus':      (valZ <= limits.full.min.z),
          'move-x-minus-y-plus':  (valX <= limits.full.min.x || valY >= limits.full.max.y),
          'move-x-plus-y-plus':   (valX >= limits.full.max.x || valY >= limits.full.max.y),
          'move-x-minus-y-minus': (valX <= limits.full.min.x || valY <= limits.full.min.y),
          'move-x-plus-y-minus':  (valX >= limits.full.max.x || valY <= limits.full.min.y)
        };

        for (const [btnId, shouldBlock] of Object.entries(blockRules)) {
          if (shouldBlock) {
            const btn = this.querySelector('#' + btnId);
            if (btn) {
              btn.style.setProperty('pointer-events', 'auto', 'important'); // Клик разрешен для анимации подсветки
              btn.style.setProperty('opacity', '0.25', 'important'); 
            }
          }
        }
      }
    }
  }

  _attachEventListeners() {
    const steps = { '01': 0.1, '1': 1, '10': 10 };
    Object.keys(steps).forEach(key => {
      this.querySelector('#step-' + key).addEventListener('click', (e) => {
        if (this.isMoving || this._isAccumulating) return;
        if (!this._hass.states['text.' + this.entity_id_prefix + '_g'] || !this._hass.states['text.' + this.entity_id_prefix + '_g'].attributes.dimensions) return;
        this.currentStep = steps[key];
        this.querySelectorAll('#steps-wrapper .frame-btn-copy').forEach(b => b.classList.remove('active'));
        const btn = e.target.closest('.frame-btn-copy');
        if (btn) btn.classList.add('active');
      });
    });

    this.querySelector('#cancel-move-bar').addEventListener('click', () => {
      if (this.isMoving) {
        this._resetLockState();
        this._publishMQTT(this.mqtt_prefix + '/' + this.device_id + '_U/cmd', '2');
      }
    });

    // Чекбокс "soft-endstops"
    const softChk = this.querySelector('#soft-endstops-chk');
    if (softChk) {
      softChk.addEventListener('change', (e) => {
        this.softEndstopsEnabled = e.target.checked;
      });
    }

    // Теперь безопасно привязываем кнопки Home (используем initLimits вместо limits)
    const initGObj = this._hass.states['text.' + this.entity_id_prefix + '_g'];
    let initLimits = {
      full: { min: { x: 0.00, y: 0.00, z: 0.00 } }
    };
    if (initGObj && initGObj.attributes && initGObj.attributes.dimensions) {
      initLimits = initGObj.attributes.dimensions.area;
    }
    
    // Динамическая парковка осей на основе физических концевиков Marlin
    this._bindHome('home-xy', 'G28 X Y\n', { X: initLimits.full.min.x, Y: initLimits.full.min.y });
    this._bindHome('home-z',  'G28 Z\n',   { Z: initLimits.full.min.z });
    this._bindHome('zero-z',  'G92 Z0\n',  { Z: 0.00 });
    
    this._bindAccumulatedMove('move-x-plus', { X: 1 });
    this._bindAccumulatedMove('move-x-minus', { X: -1 });
    this._bindAccumulatedMove('move-y-plus', { Y: 1 });
    this._bindAccumulatedMove('move-y-minus', { Y: -1 });
    this._bindAccumulatedMove('move-z-plus', { Z: 1 });
    this._bindAccumulatedMove('move-z-minus', { Z: -1 });
    this._bindAccumulatedMove('move-x-minus-y-plus', { X: -1, Y: 1 });
    this._bindAccumulatedMove('move-x-plus-y-plus', { X: 1, Y: 1 });
    this._bindAccumulatedMove('move-x-minus-y-minus', { X: -1, Y: -1 });
    this._bindAccumulatedMove('move-x-plus-y-minus', { X: 1, Y: -1 });

    const tempWrapper = this.querySelector('#temp-wrapper');
    if (tempWrapper) {
      tempWrapper.innerHTML = this.temp_presets.map((preset, idx) => {
        return '<button class="frame-btn-copy" id="temp-preset-' + idx + 
               '" data-is-active-mode="false"><span style="font-weight: bold;">' + 
               preset.h + '°</span><span style="margin: 0 4px; color: var(--disabled-text-color); opacity: 0.6;">/</span>' +
               '<span style="color: var(--secondary-text-color); font-size: 0.95em;">' + 
               preset.b + '°</span></button>';
      }).join('');

      this.temp_presets.forEach((preset, idx) => {
        this.querySelector('#temp-preset-' + idx).addEventListener('click', (e) => {
          if (this.isMoving || this._isAccumulating) return;
          const btn = e.target.closest('.frame-btn-copy');
          if (!btn) return;
          const isCurrentActive = btn.dataset.isActiveMode === 'true';
          let gcode = "";
          
          if (isCurrentActive) {
            btn.classList.remove('active-macro'); 
            btn.dataset.isActiveMode = 'false';
            gcode = 'M140 S0\nM104 S0\n;mid=' + this.card_id + ':HAG0\n'; 
          } else {
            this.querySelectorAll('#temp-wrapper .frame-btn-copy').forEach(b => {
              b.classList.remove('active-macro');
              b.dataset.isActiveMode = 'false';
            });
            btn.classList.add('active-macro'); 
            btn.dataset.isActiveMode = 'true';
            gcode = 'M140 S' + preset.b + '\nM104 S' + preset.h + '\n;mid=' + this.card_id + ':HAG0\n'; 
          }
          this._publishMQTT(this.mqtt_prefix + '/' + this.device_id + '_G/cmd', gcode);
        });
      });
    }

    const macroWrapper = this.querySelector('#macro-wrapper');
    if (macroWrapper) {
      macroWrapper.innerHTML = this.macro_buttons.map((macro, idx) => {
        const btnMode = macro.mode || 'pulse';
        return '<button class="frame-btn-copy" id="macro-btn-' + idx + 
               '" data-is-active-macro="false" data-macro-mode="' + btnMode + 
               '" style="font-size: 0.85em; text-overflow: ellipsis; overflow: hidden; white-space: nowrap; padding: 0 4px;">' + 
               macro.name + '</button>';
      }).join('');

      this.macro_buttons.forEach((macro, idx) => {
        this.querySelector('#macro-btn-' + idx).addEventListener('click', (e) => {
          if (this.isMoving || this._isAccumulating) return;
          
          const btn = e.target.closest('.frame-btn-copy');
          if (!btn) return;

          const isCurrentlyActiveMacro = btn.dataset.isActiveMacro === 'true';
          const currentMode = btn.dataset.macroMode;

          if (currentMode === 'loop' && isCurrentlyActiveMacro) {
            btn.classList.remove('active-macro');
            btn.dataset.isActiveMacro = 'false';
            this._toggleButtons(); // Возвращаем пульт в IDLE
            return;
          }

          this._clearActiveMacros();

          if (currentMode === 'loop') {
            btn.classList.add('active-macro');
            btn.dataset.isActiveMacro = 'true';
            this._toggleButtons();
          } else {
            btn.classList.add('active-macro');
            this.isMoving = true;
            this._toggleButtons();
            
            setTimeout(() => { 
              btn.classList.remove('active-macro');
              this.isMoving = false;
              this._toggleButtons();
            }, 300);
          }

          const finalGcode = macro.gcode.trim() + '\n;mid=' + this.card_id + ':HAG0\n';
          this._publishMQTT(this.mqtt_prefix + '/' + this.device_id + '_G/cmd', finalGcode);
        });
      });
    }

    ['x', 'y', 'z'].forEach(axis => {
      this.querySelector('#click-' + axis).addEventListener('click', () => {
        const entityId = 'sensor.' + this.entity_id_prefix + '_' + axis;
        this.dispatchEvent(new CustomEvent('hass-more-info', {
          detail: { entityId: entityId },
          bubbles: true, composed: true
        }));
      });
    });

    // автоматическая проверка и получение габаритов от принтера при необходимости
    // ОЧИЩАЕМ СТАРЫЙ ТАЙМЕР ПЕРЕД ПЕРЕЗАПУСКОМ (Защита от зомби-процессов)
    if (this._dimensionsInterval) clearInterval(this._dimensionsInterval);

    // Умный опрос габаритов без привязки к innerText
    this._dimensionsInterval = setInterval(() => {
      if (!this._hass) return;

      const gObj = this._hass.states['text.' + this.entity_id_prefix + '_g'];
      const isBridgeOnline = gObj && gObj.state !== 'OFFLINE' && gObj.state !== 'unavailable';
      const dimensionsMissing = !gObj || !gObj.attributes || !gObj.attributes.dimensions;

      // Шлем '3' только если мост онлайн, но габаритов еще физически нет в HA
      if (isBridgeOnline && dimensionsMissing) {
        this._publishMQTT(this.mqtt_prefix + '/' + this.device_id + '_U/cmd', '3');
      }
    }, 10000);
  }

  _startMotionLock() {
    this.isMoving = true;
    this.querySelector('#disp-panel').classList.add('moving');
    this.querySelectorAll('#disp-panel span').forEach(span => { span.style.opacity = '0.5'; });
    const stepsWrapper = this.querySelector('#steps-wrapper');
    const cancelBar = this.querySelector('#cancel-move-bar');
    this.secondsElapsed = 0;

    if (this._showBarTimeout) clearTimeout(this._showBarTimeout);
    this._showBarTimeout = setTimeout(() => {
      if (this.isMoving) {
        if (stepsWrapper) stepsWrapper.style.display = 'none';
        if (cancelBar) {
          cancelBar.style.display = 'block';
          cancelBar.style.pointerEvents = 'auto'; 
          cancelBar.innerText = 'Moving... (' + this.secondsElapsed + 's) [Cancel]';
        }
        if (this.timerInterval) clearInterval(this.timerInterval);
        this.timerInterval = setInterval(() => {
          this.secondsElapsed++;
          if (cancelBar) cancelBar.innerText = 'Moving... (' + this.secondsElapsed + 's) [Cancel]';
        }, 1000);
      }
    }, 600); 

    if (this.moveTimeout) clearTimeout(this.moveTimeout);
    this.moveTimeout = setTimeout(() => {
      if (this.isMoving) this._resetLockState();
    }, 25000);
  }

  _bindHome(elementId, gcodeCommand, targetHomeCoords) {
    const button = this.querySelector('#' + elementId);
    if (!button) return;
    button.addEventListener('click', () => {
      // Проверяем состояние только в момент физического клика по кнопке!
      const gObj = this._hass.states['text.' + this.entity_id_prefix + '_g'];
      if (!gObj || gObj.state === 'OFFLINE' || !gObj.attributes.dimensions) return;

      if (this.isMoving || this._isAccumulating) return;
      this.targetCoords = { X: null, Y: null, Z: null };
      Object.keys(targetHomeCoords).forEach(axis => { this.targetCoords[axis] = targetHomeCoords[axis]; });
      this._motionEndTime = null; 
      this._startMotionLock();
      this._toggleButtons();
      this._publishMQTT(this.mqtt_prefix + '/' + this.device_id + '_G/cmd', gcodeCommand.trim() + ';mid=' + this.card_id + ':HAM0\n');
    });
  }

  _bindAccumulatedMove(elementId, axesConfig) {
    const button = this.querySelector('#' + elementId);
    if (!button) return;

    button.addEventListener('click', () => {
      const gObj = this._hass.states['text.' + this.entity_id_prefix + '_g'];
      if (!gObj || gObj.state === 'OFFLINE' || !gObj.attributes.dimensions) return;

      if (this.isMoving && !this._isAccumulating) return;
      if (this._isAccumulating && this._activeAccumulateAxis !== elementId) return;

      const currentGObj = this._hass.states['text.' + this.entity_id_prefix + '_g'];
      let limits = {
        full: { min: { x: -100.0, y: -100.0, z: 0.0 }, max: { x: 400.0, y: 400.0, z: 400.0 } },
        work: { min: { x: -100.0, y: -100.0, z: 0.0 }, max: { x: 400.0, y: 400.0, z: 400.0 } }
      };
      
      if (currentGObj && currentGObj.attributes && currentGObj.attributes.dimensions) {
        limits = currentGObj.attributes.dimensions.area;
      }

      if (this.softEndstopsEnabled) {
        let allowedDist = this._accumulatedDist + this.currentStep;
        let isBlockedTotal = false;

        for (const [axisName, localDirection] of Object.entries(axesConfig)) {
          const axisKey = axisName.toLowerCase(); 
          const axisEntityId = 'sensor.' + this.entity_id_prefix + '_' + axisKey;
          const axisObj = this._hass.states[axisEntityId];
          if (!axisObj) continue; 

          const axisVal = parseFloat(axisObj.state);
          const axisMax = limits.full.max[axisKey]; 
          const axisMin = limits.full.min[axisKey];

          const isInside = (axisVal >= axisMin && axisVal <= axisMax);

          if (isInside) {
            let potentialTarget = axisVal + allowedDist * localDirection;

            if (localDirection > 0 && potentialTarget > axisMax) {
              allowedDist = Math.max(0, axisMax - axisVal);
            } else if (localDirection < 0 && potentialTarget < axisMin) {
              allowedDist = Math.max(0, axisVal - axisMin);
            }
          } else {
            const isMovingToHomeZone = (localDirection > 0 && axisVal < axisMin) || (localDirection < 0 && axisVal > axisMax);
            if (!isMovingToHomeZone) {
              isBlockedTotal = true; 
              break;
            }
          }
        }

        if (isBlockedTotal || allowedDist <= 0.001) {
          return; // Просто прерываем выполнение. Анимация нажатия сработает, но команда не уйдет.
        }

        this._accumulatedDist = allowedDist;
      } else {
        this._accumulatedDist += this.currentStep;
      }

      if (this._accumulatedDist > this._maxAccumulateLimit) {
        this._accumulatedDist = this._maxAccumulateLimit;
      }

      this._isAccumulating = true;
      this._activeAccumulateAxis = elementId;
      this._toggleButtons();

      const displayAxisName = Object.keys(axesConfig).join(''); 
      const softEndstopLabel = this.querySelector('label[for="soft-endstops-chk"]');
      if (softEndstopLabel) {
        softEndstopLabel.innerHTML = `Use software endstops <span style="color: var(--accent-color); font-weight: bold; margin-left: 12px; font-family: monospace;">[${displayAxisName} ${this._accumulatedDist.toFixed(1)} mm]</span>`;
      }

      if (this._accumulateTimeout) clearTimeout(this._accumulateTimeout);

      this._accumulateTimeout = setTimeout(() => {
        const finalDist = this._accumulatedDist;
        this._isAccumulating = false;
        
        if (softEndstopLabel) {
          softEndstopLabel.innerText = 'Use software endstops';
        }

        this.targetCoords = { X: null, Y: null, Z: null };
        let moveParams = '';
        
        for (const [axisName, localDirection] of Object.entries(axesConfig)) {
          const axisKey = axisName.toLowerCase();
          const axisEntityId = 'sensor.' + this.entity_id_prefix + '_' + axisKey;
          const axisObj = this._hass.states[axisEntityId];
          
          if (axisObj) {
            const currentVal = parseFloat(axisObj.state);
            this.targetCoords[axisName] = currentVal + (finalDist * localDirection);
          }
          
          const safeDistance = finalDist * localDirection;
          moveParams += ' ' + axisName + safeDistance.toFixed(2);
        }

        this._toggleButtons();
        this._startMotionLock();

        let feedrate = 1500;
        if (axesConfig.Z !== undefined) {
          if (this.currentStep === 0.1) feedrate = 400;
          if (this.currentStep === 1) feedrate = 600;
          if (this.currentStep === 10) feedrate = 800;
        } else {
          if (this.currentStep === 0.1) feedrate = 1200;
          if (this.currentStep === 10) feedrate = 3000;
        }

        const speedMMPerSec = feedrate / 60;
        const durationMs = (finalDist / speedMMPerSec) * 1000;
        this._motionEndTime = Date.now() + durationMs + 200; 

        const gcode = 'G91\nG1' + moveParams + ' F' + feedrate + '\nG90\n;mid=' + this.card_id + ':HAM2\n';
        this._publishMQTT(this.mqtt_prefix + '/' + this.device_id + '_G/cmd', gcode);

        this._accumulatedDist = 0;
        this._activeAccumulateAxis = null;

      }, this._accumulateDelay);
    });
  }

  _publishMQTT(fullTopic, payloadCommand) {
    this._hass.callWS({
      type: 'call_service', 
      domain: 'mqtt', 
      service: 'publish',
      service_data: { 
        topic: fullTopic, 
        payload: payloadCommand 
      }
    });
  }

  getCardSize() { return 4; }

  // ЭТОТ МЕТОД Автоматически убивает таймер при удалении/перерисовке карточки
  disconnectedCallback() {
    if (this._dimensionsInterval) {
      clearInterval(this._dimensionsInterval);
      this._dimensionsInterval = null;
    }
  }
}
customElements.define('marlin-move-control-card', PrinterControlCard);
