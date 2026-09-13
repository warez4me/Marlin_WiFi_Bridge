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
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Marlin WiFi bridge configuration</title>
<style>
body{font-family:monospace;background:#112;color:#eee;padding:20px;text-align:center}
.box{background:#223;padding:20px;border-radius:6px;display:inline-block;text-align:left;max-width:400px;width:100%;box-shadow:0 4px 15px #000}
h2{text-align:center;margin-top:0;color:#4af}
label{display:block;margin:10px 0 2px 0;font-size:12px;color:#88a}
input{width:100%;padding:8px;background:#000;border:1px solid #445;color:#0f0;box-sizing:border-box;font-family:monospace}
button{width:100%;padding:10px;margin-top:20px;background:#4af;border:none;color:#fff;font-weight:bold;cursor:pointer}
button:hover{background:#5bf}#t{margin-top:15px;text-align:center;font-weight:bold}
</style></head><body><div class="box"><h2>Marlin WiFi bridge configuration</h2>
<form id="f"><input type="hidden" id="o" name="o" value="0">
<label>1.Wi-Fi SSID</label><input type="text" id="s" name="s" required>
<label>2.Wi-Fi password</label><input type="password" id="p" name="p">
<label style="display:block; margin-top:15px; font-size:14px; cursor:pointer;">
  <input type="checkbox" id="v" style="width:auto; transform:scale(1.3); margin-right:8px; vertical-align:middle;">View password</label>
<label>3.MQTT broker IP address[:port]</label><input type="text" id="i" name="i">
<label>4.MQTT broker login</label><input type="text" id="u" name="u">
<label>5.MQTT broker password</label><input type="password" id="w" name="w">
<label>6.HA MQTT discovery prefix</label><input type="text" id="h" name="h" placeholder="homeassistant">
<label>7.HA device ID</label><input type="text" id="d" name="d">
<button type="submit">SAVE and RESTART</button></form><div id="t"></div></div>
<script>

document.addEventListener('DOMContentLoaded',function(){
  const r=new XMLHttpRequest();
  r.open('GET',document.location,false);
  r.send(null);
  // Читаем заголовки и заполняем по name через коллекцию формы f
  // 1. скрытый Anti-CSRF токен безопасности 'o'
  o.value=r.getResponseHeader('x')||'0';
  // 2. остальные
  s.value = r.getResponseHeader('s') || '';
  i.value = r.getResponseHeader('i') || '';
  u.value = r.getResponseHeader('u') || '';
  h.value = r.getResponseHeader('h') || '';
  d.value = r.getResponseHeader('d') || '';
});

f.addEventListener('submit', function(e) {
    e.preventDefault(); t.style.color = '#fff'; t.innerText = 'Sending..';
    fetch('/config', { method: 'POST', body: new URLSearchParams(new FormData(f))
    }).then(r => {
      t.style.color = r.ok ? '#0f0' : '#f32';
      return r.text();
    }).then(k => {
      t.innerText = k;
    }).catch(() => {
      t.style.color='#f32';
      t.innerText='Network error';
    });
});

v.addEventListener('change', function() {
  // Если чекбокс нажат, меняем тип полей на text, иначе - возвращаем password
  p.type = w.type = this.checked ? 'text' : 'password';
});
</script></body></html>
)=====";

