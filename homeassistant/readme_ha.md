# Home Assistant Ecosystem Integration Guide

This directory contains the comprehensive frontend and backend ecosystem tailored for the WiFi-UART Bridge. By deploying the pre-configured assets found here, you will transform raw MQTT printer data into a rich, reactive, and secure smart-home control hub.

### 📂 Directory Naming & Mapping Conventions
* **The Root Path:** The root directory of your Home Assistant installation is the exact folder where your main `configuration.yaml` file resides. Depending on your specific installation method (HA OS, Docker, or Core), this folder may have any real name on your host system, but it is **always internally mapped to the `/config` path**.
* **Repository Mirroring:** To make deployment effortless, all production-ready asset files within this directory are structured as a **direct mirror** of how they must be allocated inside your server's **`/config`** folder. 
* **Path Creation:** If any target subdirectory or path specified in this guide does not exist in your environment (for example, the `www` or `packages` folder), you must create it manually before copying the assets.

### 📦 Included Ecosystem Components
* **[📂 packages/iot_7.yaml](homeassistant/packages/)** — An isolated Home Assistant Package containing optimized virtual helper sensors, automated filters, and Jinja2-templated entities (such as streamlined target/current heating layouts and responsive Wi-Fi RSSI signal bar tracking).
* **[📂 www/marlin-printer-card/marlin-move-control-card.js](homeassistant/www/marlin-printer-card/)** — A native JavaScript Lovelace card implementing a smart incremental joystick, real-time print bed sizing, customizable heating/macro profiles, and built-in anti-race condition locking.
* **[📂 homeassistant/dashboard/](homeassistant/dashboard/)** — Contains frontend deployment layouts matching the reference [screenshots](addendum/screenshots/):
  * `card_config.yaml` — The explicit configuration block extracted from the custom card visual editor, detailing all available variables, limits, and parameters.
  * `full_view_ui.yaml` — The raw Lovelace view YAML source code for rebuilding the entire dedicated printer control tab.

---

## 📦 Backend Integration via YAML Packages

To avoid configuration fragmentation, all virtual helper entities, template sensors, and automations are consolidated into a single configuration package. 

* 💡 **Note:** It is highly recommended to use your specific **Device Identifier** as the filename and identifier for your package asset, as `iot_7` is provided purely as a placeholder example.

1. **Enable Package Inclusions:** Ensure your root `configuration.yaml` allows named directory package inclusions under the primary `homeassistant:` domain:
   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```
   Alternatively, if you prefer explicit individual package registration:
   ```yaml
   homeassistant:
     packages:
       iot_7: !include packages/iot_7.yaml
   ```
2. **Deploy the Asset:** Copy the pre-configured package file [packages/iot_7.yaml](homeassistant/packages/) into your server's packages directory, using your desired filename instead of the default `iot_7`:
   `/config/packages/iot_7.yaml`
3. **Audit the Variables:** Open the deployed `.yaml` file and ensure all hardcoded entities properly reflect the specific **Device Identifier** token configured during the **Initial Configuration** procedure.
4. **Reload HA Configuration:** Check your new configuration via **Developer Tools ➔ YAML** tab, and then simply reload the **Template entities** to spin up the calculated temperature streams and dynamic Wi-Fi RSSI device classes without interrupting your system. Alternatively, you can reload the entire Home Assistant backend.

---

## 🎛️ Custom Lovelace Card Registration

The card operates as a standalone JavaScript module. To register it :

1. **File Allocation:** Copy [www/marlin-printer-card/marlin-move-control-card.js](homeassistant/www/marlin-printer-card/) file into your server's root web storage under the recommended path, maintaining the mirrored directory structure.
   * Note that `/config/www/` maps internally to the `/local/` URL prefix.
   * Restart Home Assistant if the `www/` directory was newly created.
2. **Check the Version:** Open the [marlin-move-control-card.js](homeassistant/www/marlin-printer-card/marlin-move-control-card.js) file and look at the version number declared in the comments at the very beginning of the script.
3. **Resource Definition:** Navigate to **Settings ➔ Dashboards ➔ Three dots ➔ Resources ➔ Add resource** and edit the registration dialog:
   * **URL:** `/local/marlin-printer-card/marlin-move-control-card.js?v=21.4.0` *(It is highly recommended to replace `21.4.0` with the actual version number you found inside the script file)*.
   * **Type:** `JavaScript Module`
4. **Card Configuration (For Existing Tabs):** To inject the card into **already existing tab** on your dashboard, initialize a manual card type on your dashboard and utilize the raw reference settings code provided in [homeassistant/dashboard/card_config.yaml](homeassistant/dashboard/card_config.yaml).
   * ⚠️ **Device ID Alignment:** Ensure that the `device_id:` parameter mapped inside your dashboard card YAML configuration exactly matches the **Device Identifier** token configured during the WiFi-UART Bridge **Initial Configuration** procedure. Misalignment will prevent the card from acquiring the correct state streams and will make it fully misfunctional.
5. **UI Customization & Scaling:** The frontend architecture is modular and highly configurable directly within the card YAML code:
   * **Temperature Quick-Actions:** You can scale the number of temperature preset buttons and modify their individual targets (`h:` for hotend, `b:` for bed) to match your materials workflow.
   * **G-Code Macros:** The layout allows you to freely expand or compress the macro panel matrix. You can modify button names, inject complex multi-line custom G-code routines via the literal block pipe (`|`), and switch the operational execution type using explicit `mode: pulse` (single run) or `mode: loop` (toggle state indicator) triggers.

### 🔄 Future Card Script Updates & Cache Clearing
Follow these steps:

1. Download an updated version of the [marlin-move-control-card.js](homeassistant/www/marlin-printer-card/) file and replace existing one in the `/config/www/marlin-printer-card/` folder.
2. **Check the Version number:** declared in the comments at the very [beginning of the new file](homeassistant/www/marlin-printer-card/marlin-move-control-card.js).
3. **Update HA Resource:** Navigate to **Settings ➔ Dashboards ➔ Three dots ➔ Resources**, select the existing resource line for this card, and open the update dialog.
4. **Modify the Link:** Change the version suffix at the end of the URL string to match the fresh version (e.g., update `?v=21.4.0` to `?v=22.0.0`) and click **Update**.
5. **Clear Browser Cache & Reload:** 
   * **Google Chrome Lifehack:** To quickly force a clean reload without losing other session data, open the developer tools by pressing **F12**, right-click (or long press) the standard browser **Reload** button next to the address bar, and select **"Empty Cache and Hard Reload"**. Once the page refreshes, press **F12** again to close the devtools panel.
   * For other environments or mobile companion apps, perform a standard manual cache wipe and force-close the application before restarting Home Assistant.

---

## 📊 Dashboard Layout & Customization via Lovelace Editor

* **Whole Tab Dashboard Deployment:** To fully deploy the user interface matching the reference screenshot, open [homeassistant/dashboard/full_view_ui.yaml](homeassistant/dashboard/full_view_ui.yaml), copy its entire content, enter your dashboard's raw configuration editor (**Edit Dashboard ➔ + ➔ Three Dots ➔ Edit in YAML**), and paste the snippet to instantly rebuild the structured layout tab.

* ⚠️ **Crucial Double-Configuration Warning:** If you deploy the whole tab, you do not need to use the separate [card_config.yaml](homeassistant/dashboard/) file. However, just like described in the **Custom Lovelace Card Registration** section above, you **must completely perform the ⚠️ Device ID Alignment sub-routine throughout the entire full-view YAML text**. The configured identifier token is tightly integrated not only into the custom card code, but also nested inside the standard HA entities cards, MQTT service call actions, and automation buttons. You must find and replace all default ID substrings within the file to match your actual server configuration, otherwise the dashboard components will remain fully non-functional. Note that Home Assistant configuration requires entity names to strictly use **lowercase** letters, while MQTT topic paths must maintain the **original case** form of your device ID.

  - In addition to the mandatory device ID audit, you can freely modify the embedded card configuration (such as custom button matrices, targets, and macros) directly inside the full-view view file as detailed in the **UI Customization & Scaling** guide above. Furthermore, you can always manage and adjust the card's specific configuration variables independently later on through the standard Home Assistant card editor interface, just like with any other native HA dashboard component.

### 🌐 Embedding the Web Interface via iFrame (Lovelace View)
The provided layout file [homeassistant/dashboard/full_view_ui.yaml](homeassistant/dashboard/) already includes a dedicated embedded panel section. Within that [configuration text](homeassistant/dashboard/full_view_ui.yaml), you will find an explicit URL path property mapping directly to the bridge server resource:

* **URL Endpoint:** For seamless integration inside the Home Assistant view container, always configure that source link using the dedicated sub-path: **`http://<YOUR_ESP_IP>/ha`**
* **IP Address Management:** To avoid the hassle of editing your dashboard YAML every time your router assigns a new dynamic IP to the bridge, remember to lock the ESP8266 to a **Static IP** via your router's DHCP reservation page. This keeps your embedded layout working seamlessly without manual intervention.

## License & Acknowledgments

Copyright (c) 2026 warez4me. All rights reserved.

This project is open-source software licensed under the **GNU General Public License v3.0 (GPLv3)**. You are free to modify and distribute it, provided that any derivative works also remain open-source under the same license.

### AI Assistance
* The codebase was fully generated by Generative AI tools based on human feature specifications, architectural requirements, and rigorous output validation.
