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
  
  // Configurar cámara primero
  if (setupCamera()) {
    Serial.println("Camara configurada correctamente");
    camera_ok = true;
  } else {
    Serial.println("Error configurando camara");
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
}

bool setupCamera() {
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
  
  // Configuración de calidad según memoria disponible
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    Serial.println("PSRAM encontrada - Alta calidad");
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("Sin PSRAM - Calidad media");
  }
  
  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error inicializando camara: 0x%x\n", err);
    return false;
  }
  
  // Configuraciones adicionales del sensor
  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV2640_PID) {
    Serial.println("Sensor OV2640 detectado");
    s->set_vflip(s, 1);        // Voltear verticalmente
    s->set_hmirror(s, 1);      // Espejo horizontal
  }
  
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
  
  server.begin();
  Serial.println("Servidor web iniciado");
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
  html += "@media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<h1>ESP32-CAM Proyecto</h1>";
  html += "<p>Sistema de monitoreo con camara y datos en tiempo real</p>";
  html += "</div>";
  
  html += "<div class='video-container'>";
  html += "<h3>Video en Vivo</h3>";
  html += "<img id='videoStream' src='/stream' alt='Cargando video...'>";
  html += "</div>";
  
  html += "<div class='controls'>";
  html += "<button onclick='capturePhoto()'>Capturar Foto</button>";
  html += "<button onclick='toggleFlash()'>Flash</button>";
  html += "<button onclick='refreshData()'>Actualizar Datos</button>";
  html += "<button onclick=\"window.open('/info', '_blank')\">Info Sistema</button>";
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

  // JavaScript
  html += "<script>";
  html += "let flashOn = false;";
  html += "function capturePhoto() {";
  html += "  fetch('/capture').then(response => response.text()).then(data => {";
  html += "    alert('Foto capturada exitosamente!'); refreshData();";
  html += "  }).catch(error => { alert('Error capturando foto: ' + error); });";
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
  html += "document.getElementById('videoStream').onerror = function() {";
  html += "  setTimeout(() => { this.src = '/stream?' + new Date().getTime(); }, 1000);";
  html += "};";
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
  }
}

void handleCapture() {
  if (!camera_ok) {
    server.send(500, "text/plain", "Camara no disponible");
    return;
  }
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Error capturando imagen");
    return;
  }
  
  photoCount++;
  
  server.sendHeader("Content-disposition", "attachment; filename=capture.jpg");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  
  Serial.printf("Foto #%d capturada\n", photoCount);
}

void handleData() {
  String json = "{";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"photoCount\":" + String(photoCount) + ",";
  json += "\"wifiSignal\":" + String(wifi_connected ? WiFi.RSSI() : 0) + ",";
  json += "\"cameraOK\":" + String(camera_ok ? "true" : "false") + ",";
  json += "\"wifiConnected\":" + String(wifi_connected ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleFlash() {
  static bool flashState = false;
  flashState = !flashState;
  digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
  server.send(200, "text/plain", flashState ? "Flash ON" : "Flash OFF");
}

void handleInfo() {
  String html = "<html><body><h1>Información del Sistema</h1>";
  html += "<p><strong>Chip ID:</strong> " + String((uint32_t)ESP.getEfuseMac(), HEX) + "</p>";
  html += "<p><strong>Memoria libre:</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";
  html += "<p><strong>PSRAM:</strong> " + String(psramFound() ? "Disponible" : "No disponible") + "</p>";
  html += "<p><strong>Flash Size:</strong> " + String(ESP.getFlashChipSize() / 1024) + " KB</p>";
  html += "<p><strong>CPU Freq:</strong> " + String(ESP.getCpuFreqMHz()) + " MHz</p>";
  if (wifi_connected) {
    html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>RSSI:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
  }
  html += "<p><a href='/'>← Volver</a></p></body></html>";
  
  server.send(200, "text/html", html);
}

void loop() {
  server.handleClient();
  
  // Actualizar datos del sistema cada 30 segundos
  if (millis() - lastDataUpdate > 30000) {
    lastDataUpdate = millis();
    
    Serial.printf("Sistema: RAM %dKB | Fotos: %d", 
                  ESP.getFreeHeap()/1024, photoCount);
    
    if (wifi_connected && WiFi.status() == WL_CONNECTED) {
    Serial.printf(" | WiFi: %ddBm", WiFi.RSSI());
    }
    Serial.println();
  }
}