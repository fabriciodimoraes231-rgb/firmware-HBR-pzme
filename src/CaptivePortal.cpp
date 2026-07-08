#include "CaptivePortal.h"

ConfigManager *CaptivePortal::m_cfgMgr = nullptr;
AsyncWebServer CaptivePortal::m_server(80);

void CaptivePortal::start(ConfigManager &cfgMgr) {
  m_cfgMgr = &cfgMgr;

  constexpr uint8_t ledPin = 2;
  pinMode(ledPin, OUTPUT);

  WiFi.mode(WIFI_AP);
  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.softAP("ESP32_Config");

  m_server.on("/", HTTP_GET, handleRoot);
  m_server.on("/save", HTTP_POST, handleSave);
  m_server.on("/reset", HTTP_POST, handleReset);

  m_server.begin();

  Serial.println("\n=== CAPTIVE PORTAL ===");
  Serial.println("SSID: ESP32_Config");
  Serial.println("IP: 192.168.4.1");
  Serial.println("Acesse http://192.168.4.1");

  while (true) {
    digitalWrite(ledPin, HIGH);
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500);
  }
}

String CaptivePortal::buildHtml() {
  return F("<!DOCTYPE html>"
  "<html lang='pt'>"
  "<head>"
  "<meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>ESP32 - Configuracao</title>"
  "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css'>"
  "<style>"
  "*{margin:0;padding:0;box-sizing:border-box;font-family:Arial,sans-serif}"
  "body{background:#f0f2f5;padding:20px}"
  ".container{max-width:720px;margin:0 auto}"
  "h1{text-align:center;color:#2c3e50;margin-bottom:20px}"
  "h1 i{color:#3498db;margin-right:10px}"
  ".card{background:#fff;border-radius:12px;box-shadow:0 2px 8px rgba(0,0,0,.1);padding:20px;margin-bottom:16px}"
  ".card h2{font-size:16px;color:#2c3e50;margin-bottom:16px;border-bottom:2px solid #3498db;padding-bottom:8px}"
  ".card h2 i{margin-right:8px;color:#3498db}"
  ".field{margin-bottom:12px}"
  ".field label{display:block;font-size:13px;color:#555;margin-bottom:4px;font-weight:600}"
  ".field input,.field select{width:100%;padding:10px 12px;border:1.5px solid #ddd;border-radius:8px;font-size:14px;transition:border .2s}"
  ".field input:focus,.field select:focus{border-color:#3498db;outline:0}"
  ".field-row{display:flex;gap:12px}"
  ".field-row .field{flex:1}"
  ".grid-2{display:grid;grid-template-columns:1fr 1fr;gap:12px}"
  ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px}"
  ".alarm-item{display:flex;align-items:center;gap:12px;padding:10px 0;border-bottom:1px solid #eee}"
  ".alarm-item:last-child{border-bottom:0}"
  ".alarm-item .switch{position:relative;width:44px;height:24px;flex-shrink:0}"
  ".alarm-item .switch input{opacity:0;width:0;height:0}"
  ".alarm-item .switch .slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;border-radius:24px;transition:.3s}"
  ".alarm-item .switch .slider:before{content:'';position:absolute;height:18px;width:18px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.3s}"
  ".alarm-item .switch input:checked+.slider{background:#e74c3c}"
  ".alarm-item .switch input:checked+.slider:before{transform:translateX(20px)}"
  ".alarm-item .alarm-label{flex:1;font-size:14px;color:#333}"
  ".alarm-item .alarm-label small{display:block;color:#888;font-size:11px}"
  ".alarm-item .alarm-val{width:100px}"
  ".alarm-item .alarm-val input{width:100%;padding:6px 8px;border:1.5px solid #ddd;border-radius:6px;font-size:13px;text-align:right}"
  ".btn-group{display:flex;gap:12px;margin-top:8px}"
  ".btn{flex:1;padding:14px;border:0;border-radius:10px;font-size:16px;font-weight:700;cursor:pointer;transition:opacity .2s}"
  ".btn:hover{opacity:.85}"
  ".btn-save{background:#27ae60;color:#fff}"
  ".btn-reset{background:#e74c3c;color:#fff}"
  ".info{text-align:center;font-size:12px;color:#888;margin-top:12px}"
  "</style>"
  "</head>"
  "<body>"
  "<div class='container'>"
  "<h1><i class='fas fa-microchip'></i> PZEM Multi-Fase - Config</h1>"
  "<form action='/save' method='POST'>"

  "<div class='card'>"
  "<h2><i class='fas fa-wifi'></i> WiFi</h2>"
  "<div class='field'><label>SSID</label><input type='text' name='wifi_ssid' placeholder='Nome da rede WiFi' required></div>"
  "<div class='field'><label>Senha</label><input type='password' name='wifi_pass' placeholder='Senha da rede WiFi'></div>"
  "</div>"

  "<div class='card'>"
  "<h2><i class='fas fa-server'></i> MQTT Broker</h2>"
  "<div class='field'><label>Host</label><input type='text' name='mqtt_host' placeholder='ex: 10.0.0.1 ou broker.exemplo.com'></div>"
  "<div class='field-row'>"
  "<div class='field'><label>Porta</label><input type='number' name='mqtt_port' value='1883'></div>"
  "<div class='field'><label>Prefixo Topico</label><input type='text' name='topic_prefix' value='vigiatech/setor/painel/CT'></div>"
  "</div>"
  "<div class='field-row'>"
  "<div class='field'><label>Usuario</label><input type='text' name='mqtt_user' placeholder='opcional'></div>"
  "<div class='field'><label>Senha</label><input type='password' name='mqtt_pass' placeholder='opcional'></div>"
  "</div>"
  "</div>"

  "<div class='card'>"
  "<h2><i class='fas fa-cog'></i> Sistema</h2>"
  "<div class='grid-3'>"
  "<div class='field'><label>Tensao Nominal</label><select name='nominal_voltage'><option value='127'>127V</option><option value='220'>220V</option><option value='380'>380V</option></select></div>"
  "<div class='field'><label>Fases</label><select name='num_phases'><option value='1'>1</option><option value='2'>2</option><option value='3'>3</option></select></div>"
  "<div class='field'><label>Temperatura</label><input type='text' value='AHT10 (I2C)' disabled></div>"
  "</div>"
  "<div class='field-row'>"
  "<div class='field'><label>Intervalo Envio (s)</label><input type='number' name='send_interval_s' value='5' min='1' max='3600'></div>"
  "<div class='field'><label>Intervalo Alarme (s)</label><input type='number' name='alarm_interval_s' value='60' min='5' max='3600'></div>"
  "</div>"
  "</div>"

  "<div class='card'>"
  "<h2><i class='fas fa-sliders-h'></i> Fator de Correcao (por fase)</h2>"
  "<div class='grid-3'>"
  "<div class='field'><label>V Fase 1</label><input type='number' name='v_factor_0' value='1.0' step='0.0001'></div>"
  "<div class='field'><label>V Fase 2</label><input type='number' name='v_factor_1' value='1.0' step='0.0001'></div>"
  "<div class='field'><label>V Fase 3</label><input type='number' name='v_factor_2' value='1.0' step='0.0001'></div>"
  "<div class='field'><label>A Fase 1</label><input type='number' name='a_factor_0' value='1.0' step='0.0001'></div>"
  "<div class='field'><label>A Fase 2</label><input type='number' name='a_factor_1' value='1.0' step='0.0001'></div>"
  "<div class='field'><label>A Fase 3</label><input type='number' name='a_factor_2' value='1.0' step='0.0001'></div>"
  "<div class='field'><label>W Fase 1</label><input type='number' name='w_factor_0' value='1.0' step='0.0001'></div>"
  "<div class='field'><label>W Fase 2</label><input type='number' name='w_factor_1' value='1.0' step='0.0001'></div>"
  "<div class='field'><label>W Fase 3</label><input type='number' name='w_factor_2' value='1.0' step='0.0001'></div>"
  "</div>"
  "<div class='info'>Fator = leitura_referencia / leitura_pzem. Default 1.0 (sem correcao).</div>"
  "</div>"

  "<div class='card'>"
  "<h2><i class='fas fa-exclamation-triangle'></i> Alarmes</h2>"

  "<div class='alarm-item'>"
  "<div class='alarm-label'>Tensao Maxima<small>Limite maximo tensao</small></div>"
  "<div class='alarm-val'><input type='number' name='v_max' value='242' step='0.1'></div>"
  "</div>"

  "<div class='alarm-item'>"
  "<div class='alarm-label'>Tensao Minima<small>Limite minimo tensao</small></div>"
  "<div class='alarm-val'><input type='number' name='v_min' value='198' step='0.1'></div>"
  "</div>"

  "<div class='alarm-item'>"
  "<div class='alarm-label'>Corrente Maxima<small>Limite maximo corrente</small></div>"
  "<div class='alarm-val'><input type='number' name='a_max' value='100' step='0.1'></div>"
  "</div>"

  "<div class='alarm-item'>"
  "<div class='alarm-label'>Corrente Minima<small>Limite minimo corrente</small></div>"
  "<div class='alarm-val'><input type='number' name='a_min' value='0.5' step='0.01'></div>"
  "</div>"

  "<div class='alarm-item'>"
  "<div class='alarm-label'>Temperatura Maxima<small>Limite maximo temperatura</small></div>"
  "<div class='alarm-val'><input type='number' name='temp_max' value='80' step='0.1'></div>"
  "</div>"

  "</div>"

  "<div class='btn-group'>"
  "<button type='submit' class='btn btn-save'><i class='fas fa-save'></i> Salvar e Reiniciar</button>"
  "</div>"
  "</form>"

  "<form action='/reset' method='POST' style='margin-top:8px'>"
  "<button type='submit' class='btn btn-reset' onclick=\"return confirm('Resetar todas as configuracoes?')\"><i class='fas fa-undo'></i> Resetar Configuracoes</button>"
  "</form>"

  "<div class='info'>IP: 192.168.4.1 | AP: ESP32_Config</div>"
  "</div>"
  "</body></html>");
}

void CaptivePortal::handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", buildHtml());
}

void CaptivePortal::handleSave(AsyncWebServerRequest *request) {
  Config cfg;

  auto g = [&](const char *name, const char *def) -> String {
    return request->hasParam(name, true) ? request->getParam(name, true)->value() : String(def);
  };

  strlcpy(cfg.wifi_ssid, g("wifi_ssid", "").c_str(), sizeof(cfg.wifi_ssid));
  strlcpy(cfg.wifi_pass, g("wifi_pass", "").c_str(), sizeof(cfg.wifi_pass));
  strlcpy(cfg.mqtt_host, g("mqtt_host", "").c_str(), sizeof(cfg.mqtt_host));
  strlcpy(cfg.mqtt_user, g("mqtt_user", "").c_str(), sizeof(cfg.mqtt_user));
  strlcpy(cfg.mqtt_pass, g("mqtt_pass", "").c_str(), sizeof(cfg.mqtt_pass));
  strlcpy(cfg.topic_prefix, g("topic_prefix", "vigiatech/setor/painel/CT").c_str(), sizeof(cfg.topic_prefix));
  cfg.mqtt_port = g("mqtt_port", "1883").toInt();
  cfg.num_phases = g("num_phases", "1").toInt();
  cfg.nominal_voltage = g("nominal_voltage", "127").toFloat();
  cfg.send_interval_s = g("send_interval_s", "5").toInt();
  cfg.alarm_interval_s = g("alarm_interval_s", "60").toInt();

  cfg.v_max = g("v_max", "242").toFloat();
  cfg.v_min = g("v_min", "198").toFloat();
  cfg.a_max = g("a_max", "100").toFloat();
  cfg.a_min = g("a_min", "0.5").toFloat();
  cfg.temp_max = g("temp_max", "80").toFloat();

  cfg.v_factor[0] = g("v_factor_0", "1.0").toFloat();
  cfg.v_factor[1] = g("v_factor_1", "1.0").toFloat();
  cfg.v_factor[2] = g("v_factor_2", "1.0").toFloat();
  cfg.a_factor[0] = g("a_factor_0", "1.0").toFloat();
  cfg.a_factor[1] = g("a_factor_1", "1.0").toFloat();
  cfg.a_factor[2] = g("a_factor_2", "1.0").toFloat();
  cfg.w_factor[0] = g("w_factor_0", "1.0").toFloat();
  cfg.w_factor[1] = g("w_factor_1", "1.0").toFloat();
  cfg.w_factor[2] = g("w_factor_2", "1.0").toFloat();

  m_cfgMgr->save(cfg);

  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
  "<meta http-equiv='refresh' content='3;url=/'>"
  "<title>Salvo</title><style>"
  "body{font-family:Arial;text-align:center;padding:40px;background:#f0f2f5}"
  ".msg{background:#27ae60;color:#fff;padding:20px;border-radius:12px;display:inline-block}"
  "</style></head><body>"
  "<div class='msg'><h2><i class='fas fa-check-circle'></i> Configuracao salva!</h2>"
  "<p>Reiniciando em 3 segundos...</p></div></body></html>");

  request->send(200, "text/html", html);
  delay(500);
  ESP.restart();
}

void CaptivePortal::handleReset(AsyncWebServerRequest *request) {
  m_cfgMgr->reset();

  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
  "<meta http-equiv='refresh' content='3;url=/'>"
  "<title>Resetado</title><style>"
  "body{font-family:Arial;text-align:center;padding:40px;background:#f0f2f5}"
  ".msg{background:#e74c3c;color:#fff;padding:20px;border-radius:12px;display:inline-block}"
  "</style></head><body>"
  "<div class='msg'><h2><i class='fas fa-undo'></i> Configuracoes resetadas!</h2>"
  "<p>Reiniciando em 3 segundos...</p></div></body></html>");

  request->send(200, "text/html", html);
  delay(500);
  ESP.restart();
}
