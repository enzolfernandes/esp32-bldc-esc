#include <Arduino.h>
#include <WiFi.h>

void setup() {
  // Inicializa a comunicação serial. 
  // Lembre-se de configurar o mesmo baud rate no platformio.ini (monitor_speed = 115200)
  Serial.begin(115200);

  // Configura o ESP32 para operar no modo Estação (Client)
  WiFi.mode(WIFI_STA);
  
  // Desconecta de qualquer rede para focar apenas na varredura
  WiFi.disconnect();
  delay(100);

  Serial.println("\n--- Setup concluído. Iniciando varredura WiFi ---");
}

void loop() {
  Serial.println("Escaneando redes nas proximidades...");
  
  // A função scanNetworks retorna o número de redes encontradas
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("Nenhuma rede WiFi encontrada.");
  } else {
    Serial.print(n);
    Serial.println(" redes encontradas:");
    
    for (int i = 0; i < n; ++i) {
      // Imprime o índice, o SSID (nome da rede) e o RSSI (intensidade do sinal em dBm)
      Serial.print("  ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm)");
      
      // Indica com um asterisco se a rede possui senha (não é aberta)
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " [Aberta]" : " *");
      delay(10);
    }
  }
  
  Serial.println("\n------------------------------------------------");
  // Aguarda 5 segundos antes de realizar uma nova varredura
  delay(5000);
}