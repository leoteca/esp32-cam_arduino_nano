#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"

// CONFIGURACIÓN WIFI
const char* ssid = "Clinica Odontologica";
const char* password = "mauricio";

// Punto de acceso de respaldo
const char* ap_ssid = "ESP32-CAM-PROJECT";
const char* ap_password = "12345678";

// CONFIGURACIÓN DE PINES DE LA CÁMARA (AI Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Pines adicionales para sensores/LEDs
#define FLASH_LED_PIN      4  // LED flash incorporado
#define STATUS_LED_PIN    33  // LED de estado (si está disponible)

WebServer server(80);

bool wifi_connected = false;
bool ap_mode = false;
bool camera_ok = false;

// Variables para datos del sistema
unsigned long lastDataUpdate = 0;
int photoCount = 0;
float systemTemperature = 0.0;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  
  // Configurar pines
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  
  Serial.println("ESP32-CAM PROYECTO COMPLETO");
  Serial.println("================================");
  
  // DIAGNÓSTICO INICIAL
  Serial.println("Iniciando diagnósticos...");
  Serial.printf("Memoria libre inicial: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("PSRAM disponible: %s\n", psramFound() ? "SI" : "NO");
  Serial.printf("Frecuencia CPU: %d MHz\n", ESP.getCpuFreqMHz());
  
  // Configurar cámara CON MÁS DIAGNÓSTICOS
  Serial.println("Configurando camara...");
  if (setupCamera()) {
    Serial.println("✓ Camara configurada correctamente");
    camera_ok = true;
    
    // Probar captura inicial
    Serial.println("Probando captura inicial...");
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      Serial.printf("✓ Captura inicial exitosa: %dx%d, %d bytes\n", 
                    fb->width, fb->height, fb->len);
      esp_camera_fb_return(fb);
    } else {
      Serial.println("✗ Error en captura inicial");
      camera_ok = false;
    }
  } else {
    Serial.println("✗ Error configurando camara");
    camera_ok = false;
  }
  
  // Configurar WiFi
  setupWiFi();
  
  // Configurar servidor web
  setupWebServer();
  
  Serial.println("Sistema inicializado completamente");
  Serial.println("================================");
  
  if (wifi_connected) {
    Serial.printf("Accede a: http://%s\n", WiFi.localIP().toString().c_str());
  } else if (ap_mode) {
    Serial.printf("Accede a: http://%s\n", WiFi.softAPIP().toString().c_str());
  }
  
  // Estado final
  Serial.printf("Estado final - Camara: %s, WiFi: %s\n", 
                camera_ok ? "OK" : "ERROR", 
                wifi_connected ? "CONECTADO" : "AP MODE");
}

bool setupCamera() {
  Serial.println("Paso 1: Inicializando configuración de cámara...");
  
  // DIAGNÓSTICO PREVIO
  Serial.println("=== DIAGNÓSTICO DE HARDWARE ===");
  Serial.printf("Pin PWDN (32): %s\n", digitalRead(32) ? "HIGH" : "LOW");
  Serial.printf("Pin XCLK (0): %s\n", digitalRead(0) ? "HIGH" : "LOW");
  Serial.printf("PSRAM: %s\n", psramFound() ? "SÍ" : "NO");
  
  // Resetear cámara si es posible
  if (PWDN_GPIO_NUM != -1) {
    pinMode(PWDN_GPIO_NUM, OUTPUT);
    digitalWrite(PWDN_GPIO_NUM, HIGH);  // Desactivar primero
    delay(100);
    digitalWrite(PWDN_GPIO_NUM, LOW);   // Activar cámara
    delay(100);
    Serial.println("Reset de cámara realizado");
  }
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Configuración MUY conservadora para debugging
  Serial.println("Paso 2: Configurando parámetros BÁSICOS...");
  config.frame_size = FRAMESIZE_QVGA;    // 320x240 - MUY pequeño para test
  config.jpeg_quality = 25;             // Calidad muy baja para test
  config.fb_count = 1;                  // Solo 1 buffer
  config.fb_location = CAMERA_FB_IN_DRAM; // Forzar uso de DRAM
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  
  Serial.println("Paso 3: Intentando inicializar cámara...");
  Serial.println("Configuración de pines:");
  Serial.printf("  PWDN: %d, RESET: %d, XCLK: %d\n", PWDN_GPIO_NUM, RESET_GPIO_NUM, XCLK_GPIO_NUM);
  Serial.printf("  SDA: %d, SCL: %d\n", SIOD_GPIO_NUM, SIOC_GPIO_NUM);
  
  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  Serial.printf("Resultado de esp_camera_init: 0x%x\n", err);
  
  if (err != ESP_OK) {
    Serial.printf("✗ Error inicializando camara: 0x%x\n", err);
    Serial.println("Códigos de error ESP32-CAM:");
    Serial.println("  0x101 (257) - ESP_ERR_INVALID_ARG");
    Serial.println("  0x102 (258) - ESP_ERR_INVALID_STATE");
    Serial.println("  0x103 (259) - ESP_ERR_INVALID_SIZE");
    Serial.println("  0x105 (261) - ESP_ERR_NOT_FOUND");
    Serial.println("  0x20001 - Cámara no detectada");
    Serial.println("  0x20002 - Error configurando resolución");
    
    // INTENTAR CONFIGURACIONES ALTERNATIVAS
    Serial.println("\n=== PROBANDO CONFIGURACIONES ALTERNATIVAS ===");
    
    // Configuración 1: Sin PWDN
    Serial.println("Intento 1: Sin pin PWDN...");
    config.pin_pwdn = -1;
    err = esp_camera_init(&config);
    if (err == ESP_OK) {
      Serial.println("✓ Éxito con pin PWDN deshabilitado");
      goto camera_success;
    }
    Serial.printf("Fallo 1: 0x%x\n", err);
    esp_camera_deinit();
    
    // Configuración 2: Frecuencia diferente
    Serial.println("Intento 2: Frecuencia XCLK 10MHz...");
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.xclk_freq_hz = 10000000;  // Mitad de frecuencia
    err = esp_camera_init(&config);
    if (err == ESP_OK) {
      Serial.println("✓ Éxito con XCLK 10MHz");
      goto camera_success;
    }
    Serial.printf("Fallo 2: 0x%x\n", err);
    esp_camera_deinit();
    
    // Configuración 3: FRAMESIZE aún más pequeño
    Serial.println("Intento 3: FRAMESIZE_QQVGA...");
    config.frame_size = FRAMESIZE_QQVGA;  // 160x120 - mínimo
    config.xclk_freq_hz = 20000000;      // Volver a frecuencia original
    err = esp_camera_init(&config);
    if (err == ESP_OK) {
      Serial.println("✓ Éxito con FRAMESIZE_QQVGA");
      goto camera_success;
    }
    Serial.printf("Fallo 3: 0x%x\n", err);
    
    Serial.println("✗ Todos los intentos fallaron");
    return false;
  } else {
    Serial.println("✓ Cámara inicializada exitosamente en primer intento");
  }
  
camera_success:
  // Configuraciones adicionales del sensor
  Serial.println("Paso 4: Configurando sensor...");
  sensor_t * s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("✗ Error: No se pudo obtener el sensor");
    return false;
  }
  
  Serial.printf("✓ Sensor detectado - PID: 0x%02X\n", s->id.PID);
  
  if (s->id.PID == OV2640_PID) {
    Serial.println("✓ Sensor OV2640 configurando...");
    s->set_vflip(s, 1);        // Voltear verticalmente
    s->set_hmirror(s, 1);      // Espejo horizontal
    s->set_brightness(s, 0);   // Brillo normal
    s->set_contrast(s, 0);     // Contraste normal
    Serial.println("✓ OV2640 configurado");
  } else if (s->id.PID == OV3660_PID) {
    Serial.println("✓ Sensor OV3660 configurando...");
    s->set_vflip(s, 1);
    s->set_hmirror(s, 0);
    Serial.println("✓ OV3660 configurado");
  } else {
    Serial.printf("⚠️ Sensor desconocido PID: 0x%02X (continuando...)\n", s->id.PID);
  }
  
  Serial.println("✓ Cámara configurada completamente");
  return true;
}

void setupWiFi() {
  Serial.println("Configurando WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
    Serial.println("\nWiFi conectado!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Senal: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\nWiFi fallo, creando punto de acceso...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    ap_mode = true;
    Serial.printf("AP creado: %s\n", ap_ssid);
    Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
  }
}

void setupWebServer() {
  // Página principal con interfaz completa
  server.on("/", HTTP_GET, handleRoot);
  
  // Stream de video en vivo
  server.on("/stream", HTTP_GET, handleStream);
  
  // Capturar foto
  server.on("/capture", HTTP_GET, handleCapture);
  
  // Obtener datos del sistema
  server.on("/data", HTTP_GET, handleData);
  
  // Control del flash
  server.on("/flash", HTTP_GET, handleFlash);
  
  // Información del sistema
  server.on("/info", HTTP_GET, handleInfo);
  
  // Ruta de diagnóstico de cámara
  server.on("/test-camera", HTTP_GET, handleCameraTest);
  
  server.begin();
  Serial.println("Servidor web iniciado");
}

void handleCameraTest() {
  String result = "<html><body><h1>Diagnóstico de Cámara</h1>";
  
  if (!camera_ok) {
    result += "<p style='color:red'>❌ Cámara no está inicializada</p>";
  } else {
    result += "<p style='color:green'>✅ Cámara inicializada</p>";
    
    // Probar captura
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      result += "<p style='color:green'>✅ Captura de prueba exitosa</p>";
      result += "<p>Resolución: " + String(fb->width) + "x" + String(fb->height) + "</p>";
      result += "<p>Tamaño: " + String(fb->len) + " bytes</p>";
      result += "<p>Formato: " + String(fb->format == PIXFORMAT_JPEG ? "JPEG" : "Otro") + "</p>";
      esp_camera_fb_return(fb);
    } else {
      result += "<p style='color:red'>❌ Error en captura de prueba</p>";
    }
  }
  
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    result += "<p>Sensor ID: 0x" + String(s->id.PID, HEX) + "</p>";
  } else {
    result += "<p style='color:red'>❌ No se pudo obtener información del sensor</p>";
  }
  
  result += "<p><a href='/'>← Volver</a></p></body></html>";
  server.send(200, "text/html", result);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32-CAM Proyecto</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; }";
  html += ".container { max-width: 1200px; margin: 0 auto; background: rgba(255,255,255,0.1); padding: 30px; border-radius: 15px; }";
  html += ".header { text-align: center; margin-bottom: 30px; }";
  html += ".header h1 { font-size: 2.5em; margin: 0; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }";
  html += ".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 20px; }";
  html += ".card { background: rgba(255,255,255,0.1); padding: 20px; border-radius: 10px; border: 1px solid rgba(255,255,255,0.2); }";
  html += ".video-container { text-align: center; background: black; border-radius: 10px; padding: 10px; margin-bottom: 20px; }";
  html += "#videoStream { max-width: 100%; height: auto; border-radius: 5px; }";
  html += ".controls { display: flex; gap: 10px; justify-content: center; flex-wrap: wrap; margin: 20px 0; }";
  html += "button { padding: 12px 24px; font-size: 16px; border: none; border-radius: 25px; cursor: pointer; background: linear-gradient(45deg, #ff6b6b, #ee5a24); color: white; transition: all 0.3s ease; box-shadow: 0 4px 15px rgba(0,0,0,0.2); }";
  html += "button:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(0,0,0,0.3); }";
  html += ".data-display { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-top: 20px; }";
  html += ".data-item { background: rgba(255,255,255,0.1); padding: 15px; border-radius: 10px; text-align: center; }";
  html += ".data-value { font-size: 1.5em; font-weight: bold; margin-top: 5px; }";
  html += ".error-msg { background: rgba(255,0,0,0.2); border: 1px solid red; padding: 15px; border-radius: 10px; margin: 10px 0; }";
  html += "@media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<h1>ESP32-CAM Proyecto</h1>";
  html += "<p>Sistema de monitoreo con camara y datos en tiempo real</p>";
  html += "</div>";
  
  // Mostrar error si la cámara no funciona
  if (!camera_ok) {
    html += "<div class='error-msg'>";
    html += "<h3>⚠️ Error de Cámara</h3>";
    html += "<p>La cámara no está funcionando correctamente. ";
    html += "<a href='/test-camera' style='color: yellow;'>Ejecutar diagnóstico</a></p>";
    html += "</div>";
  }
  
  html += "<div class='video-container'>";
  html += "<h3>Video en Vivo</h3>";
  if (camera_ok) {
    html += "<img id='videoStream' src='/stream' alt='Cargando video...'>";
  } else {
    html += "<div style='color: red; padding: 50px;'>Cámara no disponible</div>";
  }
  html += "</div>";
  
  html += "<div class='controls'>";
  if (camera_ok) {
    html += "<button onclick='capturePhoto()'>Capturar Foto</button>";
  }
  html += "<button onclick='toggleFlash()'>Flash</button>";
  html += "<button onclick='refreshData()'>Actualizar Datos</button>";
  html += "<button onclick=\"window.open('/info', '_blank')\">Info Sistema</button>";
  html += "<button onclick=\"window.open('/test-camera', '_blank')\">Test Cámara</button>";
  html += "</div>";
  
  html += "<div class='grid'>";
  html += "<div class='card'>";
  html += "<h3>Datos del Sistema</h3>";
  html += "<div class='data-display'>";
  html += "<div class='data-item'><div>Memoria Libre</div><div class='data-value' id='freeHeap'>-- KB</div></div>";
  html += "<div class='data-item'><div>Tiempo Activo</div><div class='data-value' id='uptime'>-- s</div></div>";
  html += "<div class='data-item'><div>Fotos Capturadas</div><div class='data-value' id='photoCount'>--</div></div>";
  html += "<div class='data-item'><div>Senal WiFi</div><div class='data-value' id='wifiSignal'>-- dBm</div></div>";
  html += "</div></div>";
  
  html += "<div class='card'>";
  html += "<h3>Estado del Sistema</h3>";
  html += "<div id='systemStatus'>";
  html += "<p>Camara: <span id='cameraStatus'>--</span></p>";
  html += "<p>WiFi: <span id='wifiStatus'>--</span></p>";
  html += "<p>Servidor: <span id='serverStatus'>OK</span></p>";
  html += "<p>Memoria: <span id='memoryStatus'>--</span></p>";
  html += "</div></div>";
  html += "</div></div>";

  // JavaScript mejorado
  html += "<script>";
  html += "let flashOn = false;";
  html += "function capturePhoto() {";
  html += "  if (!" + String(camera_ok ? "true" : "false") + ") { alert('Cámara no disponible'); return; }";
  html += "  fetch('/capture').then(response => {";
  html += "    if (response.ok) { alert('Foto capturada exitosamente!'); refreshData(); }";
  html += "    else { alert('Error capturando foto'); }";
  html += "  }).catch(error => { alert('Error: ' + error); });";
  html += "}";
  html += "function toggleFlash() {";
  html += "  fetch('/flash').then(response => response.text()).then(data => {";
  html += "    flashOn = !flashOn;";
  html += "    document.querySelector('button[onclick=\"toggleFlash()\"]').textContent = flashOn ? 'Flash ON' : 'Flash OFF';";
  html += "  }).catch(error => console.error('Error:', error));";
  html += "}";
  html += "function refreshData() {";
  html += "  fetch('/data').then(response => response.json()).then(data => {";
  html += "    document.getElementById('freeHeap').textContent = data.freeHeap + ' KB';";
  html += "    document.getElementById('uptime').textContent = data.uptime + ' s';";
  html += "    document.getElementById('photoCount').textContent = data.photoCount;";
  html += "    document.getElementById('wifiSignal').textContent = data.wifiSignal + ' dBm';";
  html += "    document.getElementById('cameraStatus').textContent = data.cameraOK ? 'OK' : 'ERROR';";
  html += "    document.getElementById('wifiStatus').textContent = data.wifiConnected ? 'Conectado' : 'AP Mode';";
  html += "    document.getElementById('memoryStatus').textContent = data.freeHeap > 100 ? 'OK' : 'BAJO';";
  html += "  }).catch(error => console.error('Error obteniendo datos:', error));";
  html += "}";
  html += "setInterval(refreshData, 5000);";
  html += "refreshData();";
  html += "if (" + String(camera_ok ? "true" : "false") + ") {";
  html += "  document.getElementById('videoStream').onerror = function() {";
  html += "    setTimeout(() => { this.src = '/stream?' + new Date().getTime(); }, 1000);";
  html += "  };";
  html += "}";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleStream() {
  if (!camera_ok) {
    server.send(500, "text/plain", "Camara no disponible");
    return;
  }
  
  WiFiClient client = server.client();
  
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);
  
  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Error capturando frame");
      break;
    }
    
    String header = "--frame\r\n";
    header += "Content-Type: image/jpeg\r\n";
    header += "Content-Length: " + String(fb->len) + "\r\n\r\n";
    
    server.sendContent(header);
    client.write(fb->buf, fb->len);
    server.sendContent("\r\n");
    
    esp_camera_fb_return(fb);
    
    if (!client.connected()) break;
    delay(100); // Pequeña pausa para estabilidad
  }
}

void handleCapture() {
  if (!camera_ok) {
    server.send(500, "text/plain", "Camara no disponible");
    return;
  }
  
  // Encender flash brevemente para la foto
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(100);
  
  camera_fb_t * fb = esp_camera_fb_get();
  
  digitalWrite(FLASH_LED_PIN, LOW); // Apagar flash
  
  if (!fb) {
    server.send(500, "text/plain", "Error capturando imagen");
    Serial.println("Error: No se pudo capturar imagen");
    return;
  }
  
  photoCount++;
  
  server.sendHeader("Content-disposition", "attachment; filename=capture.jpg");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  
  Serial.printf("Foto #%d capturada (%d bytes)\n", photoCount, fb->len);
}

void handleData() {
  String json = "{";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"photoCount\":" + String(photoCount) + ",";
  json += "\"wifiSignal\":" + String(wifi_connected ? WiFi.RSSI() : 0) + ",";
  json += "\"cameraOK\":" + String(camera_ok ? "true" : "false") + ",";
  json += "\"wifiConnected\":" + String(wifi_connected ? "true" : "false") + ",";
  json += "\"psram\":" + String(psramFound() ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleFlash() {
  static bool flashState = false;
  flashState = !flashState;
  digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
  server.send(200, "text/plain", flashState ? "Flash ON" : "Flash OFF");
  Serial.printf("Flash %s\n", flashState ? "encendido" : "apagado");
}

void handleInfo() {
  String html = "<html><body><h1>Información del Sistema</h1>";
  html += "<h2>Hardware</h2>";
  html += "<p><strong>Chip ID:</strong> " + String((uint32_t)ESP.getEfuseMac(), HEX) + "</p>";
  html += "<p><strong>Memoria libre:</strong> " + String(ESP.getFreeHeap()) + " bytes (" + String(ESP.getFreeHeap()/1024) + " KB)</p>";
  html += "<p><strong>PSRAM:</strong> " + String(psramFound() ? "Disponible" : "No disponible") + "</p>";
  html += "<p><strong>Flash Size:</strong> " + String(ESP.getFlashChipSize() / 1024) + " KB</p>";
  html += "<p><strong>CPU Freq:</strong> " + String(ESP.getCpuFreqMHz()) + " MHz</p>";
  
  html += "<h2>Cámara</h2>";
  html += "<p><strong>Estado:</strong> " + String(camera_ok ? "Funcionando" : "Error") + "</p>";
  
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    html += "<p><strong>Sensor:</strong> ";
    if (s->id.PID == OV2640_PID) html += "OV2640";
    else if (s->id.PID == OV3660_PID) html += "OV3660";
    else html += "Desconocido (0x" + String(s->id.PID, HEX) + ")";
    html += "</p>";
  }
  
  html += "<h2>Red</h2>";
  if (wifi_connected) {
    html += "<p><strong>Modo:</strong> Estación WiFi</p>";
    html += "<p><strong>SSID:</strong> " + String(ssid) + "</p>";
    html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>RSSI:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
  } else if (ap_mode) {
    html += "<p><strong>Modo:</strong> Punto de Acceso</p>";
    html += "<p><strong>SSID:</strong> " + String(ap_ssid) + "</p>";
    html += "<p><strong>IP:</strong> " + WiFi.softAPIP().toString() + "</p>";
  }
  
  html += "<p><strong>Tiempo activo:</strong> " + String(millis() / 1000) + " segundos</p>";
  html += "<p><a href='/'>← Volver</a> | <a href='/test-camera'>Test Cámara</a></p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void loop() {
  server.handleClient();
  
  // Actualizar datos del sistema cada 30 segundos
  if (millis() - lastDataUpdate > 30000) {
    lastDataUpdate = millis();
    
    Serial.printf("Sistema: RAM %dKB | Fotos: %d | Camara: %s", 
                  ESP.getFreeHeap()/1024, photoCount, camera_ok ? "OK" : "ERROR");
    
    if (wifi_connected && WiFi.status() == WL_CONNECTED) {
      Serial.printf(" | WiFi: %ddBm", WiFi.RSSI());
    }
    Serial.println();
    
    // Verificar estado de la cámara periódicamente
    if (camera_ok) {
      camera_fb_t * fb = esp_camera_fb_get();
      if (fb) {
        esp_camera_fb_return(fb);
      } else {
        Serial.println("⚠️ Advertencia: Error en verificación de cámara");
      }
    }
  }
}