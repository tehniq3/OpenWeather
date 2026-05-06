/*
 * Received a lt of info from Open-weather.com based on AI work 
 * requests and test by Nicu FLORICA (niq_ro)
 * v.0 - initial version with data shown in Serial Monitor 
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>

const char* ssid = "bbk2";
const char* password = "internet2";

// Craiova 
float latitude = 44.3167; 
float longitude = 23.8;
float altitudine = 100.0; // Craiova este la aproximativ 100m deasupra nivelul marii

String traducereVreme(int cod) {
  switch(cod) {
    case 0: return "Cer senin";
    case 1: return "Predominant senin";
    case 2: return "Parțial noros";
    case 3: return "Înnorat";
    case 45: case 48: return "Ceață";
    case 51: case 53: case 55: return "Burniță";
    case 61: case 63: case 65: return "Ploaie";
    case 71: case 73: case 75: return "Ninsoare";
    case 80: case 81: case 82: return "Averse de ploaie";
    case 95: return "Furtună";
    default: return "Necunoscut";
  }
}

String directieVant(int grade) {
  const char* directii[] = {"N", "NE", "E", "SE", "S", "SV", "V", "NV"};
  int index = int((float)(grade / 45.)) % 8;
  return directii[index];
}

String descriereUV(float uv) {
  if (uv <= 2) return "Scăzut";
  if (uv <= 5) return "Moderat";
  if (uv <= 7) return "Ridicat";
  if (uv <= 10) return "Foarte ridicat";
  return "Extreme";
}

String descriereAQI(float aqi) {
  if (aqi <= 20) return "Bun";
  if (aqi <= 40) return "Acceptabil";
  if (aqi <= 60) return "Moderat";
  if (aqi <= 80) return "Slab";
  if (aqi <= 100) return "Foarte slab";
  return "Extrem de slab";
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n--- Pornire Sistem Meteo Open-Meteo ---");

  WiFi.begin(ssid, password);
  Serial.print("Conectare la WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConectat la WiFi!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    
    // ==========================================
    // PARTEA 1: DATE METEO (Vreme, Presiune, UV, Zi/Noapte)
    // ==========================================
    {
      WiFiClientSecure client;
      HTTPClient http;
      client.setInsecure();

      // Am adaugat "is_day" la finalul listei
      String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) + 
                   "&longitude=" + String(longitude, 4) + 
                   "&current=temperature_2m,relative_humidity_2m,surface_pressure,pressure_msl,weather_code,wind_speed_10m,wind_direction_10m,uv_index,is_day&timezone=auto";
      
      http.begin(client, url);
      int httpCode = http.GET();
      
      if (httpCode > 0) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
          float temp = doc["current"]["temperature_2m"].as<float>();
          float umiditate = doc["current"]["relative_humidity_2m"].as<float>();
          int codVreme = doc["current"]["weather_code"].as<int>();
          String timp = doc["current"]["time"].as<String>();
          
          // Extragere Zi/Noapte (1 = Zi, 0 = Noapte)
          int isDay = doc["current"]["is_day"].as<int>();
          String stareZiNoapte = isDay ? "Zi" : "Noapte";
          
          float presiuneSol_hPa = doc["current"]["surface_pressure"].as<float>();
          float presiuneMSL_hPa = doc["current"]["pressure_msl"].as<float>();
          float presiuneSol_mmHg = presiuneSol_hPa * 0.75006;
          float presiuneMSL_mmHg = presiuneMSL_hPa * 0.75006;

          float vitezaVant = doc["current"]["wind_speed_10m"].as<float>();
          int directieGrade = doc["current"]["wind_direction_10m"].as<int>();
          float uvIndex = doc["current"]["uv_index"].as<float>();
          
          Serial.println("\n=========================================");
          Serial.printf("Locatie: Craiova (Alt: %.0f m)\n", altitudine);
          // Afisam Zi/Noapte langa ora
          Serial.printf("Ora: %s (%s)\n", timp.c_str(), stareZiNoapte.c_str());
          Serial.printf("Stare: %s\n", traducereVreme(codVreme).c_str());
          Serial.printf("Temp: %.1f °C | Umid: %.0f %%\n", temp, umiditate);
          Serial.printf("Presiune (Nivelul Marii): %.1f mmHg (%.1f hPa)\n", presiuneMSL_mmHg, presiuneMSL_hPa);
          Serial.printf("Presiune (Reala la sol):  %.1f mmHg (%.1f hPa)\n", presiuneSol_mmHg, presiuneSol_hPa);
          Serial.printf("Vant: %.1f km/h din %s (%d grade)\n", vitezaVant, directieVant(directieGrade).c_str(), directieGrade);
          Serial.printf("Indice UV: %.1f (%s)\n", uvIndex, descriereUV(uvIndex).c_str());
        }
      }
      http.end();
    }

    // ==========================================
    // PARTEA 2: CALITATEA AERULUI (AQI, PM2.5, PM10)
    // ==========================================
    {
      WiFiClientSecure client2;
      HTTPClient http2;
      client2.setInsecure();

      String aqiUrl = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(latitude, 4) + 
                      "&longitude=" + String(longitude, 4) + 
                      "&current=european_aqi,pm2_5,pm10&timezone=auto";
      
      http2.begin(client2, aqiUrl);
      int httpCode2 = http2.GET();
      
      if (httpCode2 > 0) {
        String payload2 = http2.getString();
        DynamicJsonDocument doc2(512);
        DeserializationError error2 = deserializeJson(doc2, payload2);
        
        if (!error2) {
          float euAqi = doc2["current"]["european_aqi"].as<float>();
          float pm25 = doc2["current"]["pm2_5"].as<float>();
          float pm10 = doc2["current"]["pm10"].as<float>();
          
          Serial.println("-----------------------------------------");
          Serial.printf("Indice Aer (European): %.0f (%s)\n", euAqi, descriereAQI(euAqi).c_str());
          Serial.printf("PM 2.5: %.1f µg/m³\n", pm25);
          Serial.printf("PM 10:  %.1f µg/m³\n", pm10);
          Serial.println("=========================================\n");
        } else {
          Serial.println("Eroare la parsarea JSON pentru Aer");
        }
      } else {
        Serial.printf("Eroare HTTPS Aer: %s\n", http2.errorToString(httpCode2).c_str());
      }
      http2.end();
    }

  } else {
    Serial.println("Conexiune WiFi pierduta. Reconectare...");
    WiFi.begin(ssid, password);
  }
  
  delay(60000); // Asteapta 60 de secunde intre citiri
}  // end main loop
