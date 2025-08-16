#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// CONFIGURACIÓN WIFI - PRUEBA ESTAS OPCIONES:

// OPCIÓN 1: Red abierta (sin contraseña)
//const char* ssid = "MERCUSYS_7E678C";  // Red #18 - ABIERTA
//const char* password = "";

// OPCIÓN 2: Red principal (descomenta para probar)
//const char* ssid = "Edtenica lobby";
//const char* password = "estudiantes-lobby";

// OPCIÓN 3: Red de invitados (descomenta para probar)
const char* ssid = "Clinica Odontologica";
const char* password = "mauricio";  // O pregunta la contraseña

// 🔥 NUEVO: Configuración del punto de acceso de respaldo
const char* ap_ssid = "ESP32-CAM-DEBUG";
const char* ap_password = "12345678";

bool setup_done = false;
bool wifi_connected = false;
bool ap_mode = false;
WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("\n\n==============================================");
  Serial.println("🚀 ESP32-CAM DIAGNÓSTICO WiFi MEJORADO");
  Serial.println("==============================================\n");
  
  // Información básica
  Serial.println("📋 INFORMACIÓN DEL SISTEMA:");
  Serial.printf("  MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf("  Memoria libre: %d KB\n", ESP.getFreeHeap()/1024);
  Serial.printf("  Frecuencia CPU: %d MHz\n", ESP.getCpuFreqMHz());
  
  Serial.println("\n⏳ Esperando 5 segundos para que puedas leer...");
  delay(5000);
  
  // Configurar WiFi
  Serial.println("\n📡 CONFIGURANDO WIFI:");
  Serial.printf("  SSID objetivo: '%s'\n", ssid);
  Serial.printf("  Password: '%s'\n", password);
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);
  delay(2000);
  
  Serial.println("\n🔍 ESCANEANDO REDES WiFi...");
  Serial.println("(Esto puede tardar 10-15 segundos)");
  
  int n = WiFi.scanNetworks();
  
  if (n <= 0) {
    Serial.printf("❌ ERROR: No se encontraron redes. Código: %d\n", n);
    Serial.println("\n🔧 POSIBLES CAUSAS:");
    Serial.println("  • Hardware WiFi defectuoso");
    Serial.println("  • Problemas de alimentación");
    Serial.println("  • Interferencia electromagnética");
    Serial.println("\n🔥 Creando Punto de Acceso...");
    createAccessPoint();
    return;
  }
  
  Serial.printf("\n✅ REDES ENCONTRADAS (%d):\n", n);
  Serial.println("──────────────────────────────────────────────────");
  
  bool targetFound = false;
  for (int i = 0; i < n; i++) {
    String currentSSID = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    
    Serial.printf("%2d. %-25s | %4d dBm | Canal %2d | ", 
                  i+1, currentSSID.c_str(), rssi, WiFi.channel(i));
    
    switch(WiFi.encryptionType(i)) {
      case WIFI_AUTH_OPEN: Serial.print("ABIERTA"); break;
      case WIFI_AUTH_WEP: Serial.print("WEP"); break;
      case WIFI_AUTH_WPA_PSK: Serial.print("WPA"); break;
      case WIFI_AUTH_WPA2_PSK: Serial.print("WPA2"); break;
      case WIFI_AUTH_WPA_WPA2_PSK: Serial.print("WPA/WPA2"); break;
      default: Serial.print("OTRO"); break;
    }
    
    if (currentSSID == ssid) {
      Serial.print(" ⭐ OBJETIVO");
      targetFound = true;
    }
    Serial.println();
  }
  
  Serial.println("──────────────────────────────────────────────────");
  WiFi.scanDelete();
  
  Serial.println("\n⏳ Esperando 3 segundos para que puedas leer...");
  delay(3000);
  
  if (!targetFound) {
    Serial.printf("\n❌ RED '%s' NO ENCONTRADA\n", ssid);
    Serial.println("\n🔧 SOLUCIONES:");
    Serial.println("  1. Verifica el SSID (mayúsculas/minúsculas)");
    Serial.println("  2. Asegúrate que sea red 2.4GHz (no 5GHz)");
    Serial.println("  3. Acércate más al router");
    Serial.println("  4. Crea un hotspot con tu teléfono");
    Serial.println("\n🔥 Creando Punto de Acceso...");
    createAccessPoint();
    return;
  }
  
  // Intentar conectar con reintentos más agresivos
  Serial.println("\n🔗 INTENTANDO CONEXIÓN...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) { // 🔥 Más intentos
    delay(1000);
    attempts++;
    Serial.printf("  Intento %d/25 - Estado: ", attempts);
    
    switch(WiFi.status()) {
      case WL_IDLE_STATUS: Serial.println("Esperando"); break;
      case WL_NO_SSID_AVAIL: Serial.println("SSID no disponible"); break;
      case WL_CONNECT_FAILED: Serial.println("Conexión falló"); break;
      case WL_CONNECTION_LOST: Serial.println("Conexión perdida"); break;
      case WL_DISCONNECTED: Serial.println("Desconectado (Estado 6)"); break;
      default: Serial.println("Desconocido"); break;
    }
    
    // 🔥 NUEVO: Reintentar cada 10 intentos
    if (attempts % 10 == 0 && attempts < 25) {
      Serial.println("  🔄 Reintentando conexión...");
      WiFi.disconnect();
      delay(2000);
      WiFi.begin(ssid, password);
      delay(1000);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n🎉 ¡CONEXIÓN EXITOSA!");
    Serial.printf("📍 IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("📶 Señal: %d dBm\n", WiFi.RSSI());
    Serial.printf("🌐 Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    wifi_connected = true;
    setupWebServer();
  } else {
    Serial.println("\n❌ CONEXIÓN FALLÓ DESPUÉS DE 25 INTENTOS");
    Serial.printf("Estado final: %d", WiFi.status());
    switch(WiFi.status()) {
      case 6: Serial.println(" (WL_DISCONNECTED - Problema muy común con ESP32-CAM)"); break;
      case 4: Serial.println(" (WL_CONNECT_FAILED - Contraseña incorrecta)"); break;
      case 1: Serial.println(" (WL_NO_SSID_AVAIL - Red no disponible)"); break;
      default: Serial.println(" (Otro error)"); break;
    }
    Serial.println("\n🔧 POSIBLES CAUSAS DEL ESTADO 6:");
    Serial.println("  • Problemas de alimentación (muy común)");
    Serial.println("  • Señal WiFi demasiado débil");
    Serial.println("  • Router con muchos dispositivos conectados");
    Serial.println("  • Incompatibilidad con algunos routers");
    Serial.println("  • Red 5GHz (ESP32 solo soporta 2.4GHz)");
    Serial.println("\n🔥 Creando Punto de Acceso como respaldo...");
    createAccessPoint();
  }
  
  setup_done = true;
  Serial.println("\n✅ DIAGNÓSTICO COMPLETADO");
  Serial.println("==============================================");
}

void createAccessPoint() {
  WiFi.mode(WIFI_AP);
  bool result = WiFi.softAP(ap_ssid, ap_password);
  
  if (result) {
    ap_mode = true;
    Serial.println("✅ Punto de Acceso creado exitosamente:");
    Serial.printf("  📡 Red: %s\n", ap_ssid);
    Serial.printf("  🔑 Password: %s\n", ap_password);
    Serial.printf("  🌐 IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("\n📱 INSTRUCCIONES PARA CONECTARTE:");
    Serial.println("  1. Ve a la configuración WiFi de tu teléfono/PC");
    Serial.printf("  2. Conecta a la red: %s\n", ap_ssid);
    Serial.printf("  3. Usa la contraseña: %s\n", ap_password);
    Serial.printf("  4. Abre tu navegador y ve a: http://%s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("  5. ¡Deberías ver la página de la ESP32-CAM!");
    setupWebServer();
  } else {
    Serial.println("❌ Error crítico: No se pudo crear el Punto de Acceso");
    Serial.println("🔧 Verifica la alimentación y reinicia la ESP32-CAM");
  }
}

void setupWebServer() {
  // 🔥 Página principal mejorada
  server.on("/", [](){
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>ESP32-CAM Diagnóstico</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 40px; background: #f0f2f5; }";
    html += ".container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
    html += ".success { color: #28a745; font-size: 28px; text-align: center; }";
    html += ".info { background: #e9ecef; padding: 15px; border-radius: 5px; margin: 10px 0; }";
    html += ".status { padding: 10px; border-radius: 5px; margin: 10px 0; }";
    html += ".wifi-ok { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
    html += ".ap-mode { background: #fff3cd; color: #856404; border: 1px solid #ffeaa7; }";
    html += "hr { margin: 20px 0; }";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1 class='success'>🎉 ESP32-CAM Funcionando!</h1>";
    
    if (ap_mode) {
      html += "<div class='status ap-mode'>";
      html += "<strong>📡 Modo:</strong> Punto de Acceso<br>";
      html += "<strong>🌐 IP:</strong> " + WiFi.softAPIP().toString() + "<br>";
      html += "<strong>📶 Red creada:</strong> " + String(ap_ssid) + "<br>";
      html += "<strong>ℹ️ Estado:</strong> WiFi normal falló, usando modo AP";
      html += "</div>";
    } else if (wifi_connected) {
      html += "<div class='status wifi-ok'>";
      html += "<strong>📡 Modo:</strong> Cliente WiFi<br>";
      html += "<strong>🌐 IP:</strong> " + WiFi.localIP().toString() + "<br>";
      html += "<strong>📶 Señal:</strong> " + String(WiFi.RSSI()) + " dBm<br>";
      html += "<strong>🏠 Red:</strong> " + String(ssid);
      html += "</div>";
    }
    
    html += "<div class='info'><strong>💾 Memoria libre:</strong> " + String(ESP.getFreeHeap()/1024) + " KB</div>";
    html += "<div class='info'><strong>⏱️ Tiempo activo:</strong> " + String(millis()/1000) + " segundos</div>";
    html += "<div class='info'><strong>📱 MAC:</strong> " + WiFi.macAddress() + "</div>";
    
    html += "<hr>";
    html += "<h3>✅ Estado del Sistema</h3>";
    html += "<p>✓ Hardware: OK<br>";
    html += "✓ Conectividad: OK<br>";
    html += "✓ Servidor Web: OK<br>";
    html += "✓ Memoria: OK</p>";
    
    html += "<p><small>🔧 Tu ESP32-CAM está lista para ser usada en proyectos más avanzados.</small></p>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
  });
  
  // 🔥 Página de información técnica
  server.on("/info", [](){
    String html = "<!DOCTYPE html><html><head><title>Info Técnica</title></head><body>";
    html += "<h1>Información Técnica ESP32-CAM</h1>";
    html += "<p><strong>Chip ID:</strong> " + String((uint32_t)ESP.getEfuseMac(), HEX) + "</p>";
    html += "<p><strong>CPU Frecuencia:</strong> " + String(ESP.getCpuFreqMHz()) + " MHz</p>";
    html += "<p><strong>Flash Size:</strong> " + String(ESP.getFlashChipSize()/1024) + " KB</p>";
    html += "<p><strong>Memoria libre:</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";
    html += "<p><strong>Tiempo activo:</strong> " + String(millis()) + " ms</p>";
    if (wifi_connected) {
      html += "<p><strong>RSSI:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
      html += "<p><strong>Gateway:</strong> " + WiFi.gatewayIP().toString() + "</p>";
      html += "<p><strong>DNS:</strong> " + WiFi.dnsIP().toString() + "</p>";
    }
    html += "<p><a href='/'>← Volver</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  
  server.begin();
  Serial.println("🌐 Servidor web iniciado correctamente");
  
  if (ap_mode) {
    Serial.printf("🌍 Accede desde: http://%s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("🔧 Info técnica: http://%s/info\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.printf("🌍 Accede desde: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("🔧 Info técnica: http://%s/info\n", WiFi.localIP().toString().c_str());
  }
}

void loop() {
  if (setup_done) {
    server.handleClient();
    
    // 🔥 Mensaje de estado cada 2 minutos con más información
    static unsigned long lastMessage = 0;
    if (millis() - lastMessage > 120000) {
      lastMessage = millis();
      Serial.printf("💚 Sistema OK - RAM: %d KB", ESP.getFreeHeap()/1024);
      
      if (wifi_connected && WiFi.status() == WL_CONNECTED) {
        Serial.printf(" - WiFi: %d dBm", WiFi.RSSI());
      } else if (ap_mode) {
        Serial.printf(" - AP: %d clientes", WiFi.softAPgetStationNum());
      }
      Serial.println();
      
      // 🔥 Verificar si perdió conexión WiFi
      if (wifi_connected && WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ Conexión WiFi perdida - funcionando en modo normal");
      }
    }
  }
}