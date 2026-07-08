#include <Arduino.h>
#include <esp_task_wdt.h>
#include <ESPAsyncWebServer.h>
#include "ConfigManager.h"
#include "CaptivePortal.h"
#include "SensorManager.h"
#include "AlarmManager.h"
#include "TimeManager.h"
#include "MqttManager.h"

ConfigManager configManager;
SensorManager sensorManager;
AlarmManager alarmManager;
TimeManager timeManager;
MqttManager mqttManager;

Config config;
PhaseData phases[MAX_PHASES];
AlarmState alarmState;
AlarmState newAlarms;
AlarmState resolvedAlarms;
AlarmState activeAlarms;

unsigned long lastRead = 0;
unsigned long lastSend = 0;
unsigned long lastAlarmSend = 0;
unsigned long lastMqttTry = 0;
constexpr unsigned long MQTT_RETRY_MS = 5000;
constexpr unsigned long GPIO_HOLD_MS = 5000;
constexpr uint8_t LED_PIN = 2;

bool shouldRebootAp = false;
float currentTemp = NAN;

AsyncWebServer stationServer(80);

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PZEM Monitor</title>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
<style>
*{margin:0;padding:0;box-sizing:border-box;font-family:Arial,sans-serif}
body{background:#f0f2f5;padding:16px}
h1{text-align:center;color:#2c3e50;margin:16px 0;font-size:20px}
h1 i{color:#3498db;margin-right:8px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:12px;max-width:960px;margin:0 auto}
.card{background:#fff;border-radius:12px;padding:16px;box-shadow:0 2px 6px rgba(0,0,0,.08)}
.card h2{font-size:14px;color:#7f8c8d;margin-bottom:12px;border-bottom:2px solid #3498db;padding-bottom:6px}
.card h2 i{margin-right:6px;color:#3498db}
.row{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid #f0f0f0}
.row:last-child{border:0}
.label{color:#555;font-size:13px}
.value{font-weight:700;color:#2c3e50;font-size:14px}
.unit{color:#95a5a6;font-size:11px;margin-left:2px}
.status-dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px}
.dot-green{background:#27ae60}
.dot-red{background:#e74c3c}
.dot-gray{background:#bdc3c7}
.alarm-badge{display:inline-block;background:#e74c3c;color:#fff;font-size:11px;padding:2px 6px;border-radius:4px;margin:2px}
.footer{text-align:center;font-size:11px;color:#95a5a6;margin-top:16px}
.btn-toggle{display:block;margin:0 auto 12px;padding:10px 20px;background:#3498db;color:#fff;border:0;border-radius:8px;font-size:14px;cursor:pointer;font-weight:700}
.btn-toggle:hover{background:#2980b9}
.frm{display:none}
.frm.show{display:block}
.frm .field{margin-bottom:10px}
.frm .field label{display:block;font-size:12px;color:#555;margin-bottom:3px;font-weight:600}
.frm .field input,.frm .field select{width:100%;padding:8px;border:1.5px solid #ddd;border-radius:6px;font-size:13px}
.frm .field input:focus{border-color:#3498db;outline:0}
.frm .field-row{display:flex;gap:10px}
.frm .field-row .field{flex:1}
.frm .frm-title{font-size:13px;color:#7f8c8d;margin:10px 0 6px;border-bottom:1px solid #eee;padding-bottom:4px;font-weight:700}
.frm .btn-group{display:flex;gap:10px;margin-top:12px}
.frm .btn{flex:1;padding:12px;border:0;border-radius:8px;font-size:14px;font-weight:700;cursor:pointer}
.frm .btn-save{background:#27ae60;color:#fff}
.frm .btn-mqtt{background:#3498db;color:#fff}
.frm .btn-cancel{background:#95a5a6;color:#fff}
.frm .mqtt-status{font-size:11px;color:#95a5a6;text-align:center;margin:6px 0 -6px;padding:4px;border-radius:4px}
.frm .mqtt-status.ok{background:#e8f8f0;color:#27ae60}
.frm .mqtt-status.fail{background:#fde8e8;color:#e74c3c}
.frm .btn:hover{opacity:.85}
.msg-ok{background:#27ae60;color:#fff;text-align:center;padding:10px;border-radius:8px;margin-top:10px;font-size:13px;display:none}
.msg-err{background:#e74c3c;color:#fff;text-align:center;padding:10px;border-radius:8px;margin-top:10px;font-size:13px;display:none}
</style>
</head>
<body>
<h1><i class="fas fa-microchip"></i>PZEM Multi-Fase</h1>
<div class="grid" id="status-card"></div>
<div class="grid" id="temp-card"></div>
<div class="grid" id="cards"></div>
<div class="grid" id="limits-card"></div>
<div class="grid" id="factors-card"></div>
<div class="grid" id="alarms-card"></div>
<button class="btn-toggle" onclick="toggleConfig()"><i class="fas fa-cog"></i> Configurar</button>
<div class="grid" id="config-card"></div>
<div class="footer" id="footer"></div>
<script>
var gData=null;
function loadData(){var x=new XMLHttpRequest;x.onreadystatechange=function(){if(this.readyState==4&&this.status==200){gData=JSON.parse(this.responseText);render(gData)}};x.open("GET","/api/data",true);x.send()}
function render(d){
var s='<div class="card"><h2><i class="fas fa-info-circle"></i>Sistema</h2><div class="row"><span class="label">IP</span><span class="value">'+d.ip+'</span></div><div class="row"><span class="label">WiFi</span><span class="value"><span class="status-dot dot-green"></span>OK</span></div><div class="row"><span class="label">MQTT</span><span class="value"><span class="status-dot '+(d.mqtt?"dot-green":"dot-red")+'"></span>'+(d.mqtt?"OK":"OFF")+'</span></div><div class="row"><span class="label">Fases</span><span class="value">'+d.phases+'</span></div><div class="row"><span class="label">V Nominal</span><span class="value">'+d.v_nominal+'<span class="unit">V</span></span></div><div class="row"><span class="label">Uptime</span><span class="value">'+d.uptime+'</span></div></div>';document.getElementById("status-card").innerHTML=s

var t='';if(d.temperature!==undefined&&d.temperature!==null){t='<div class="card"><h2><i class="fas fa-thermometer-half"></i>Temperatura</h2><div class="row"><span class="label">Temperatura</span><span class="value">'+d.temperature.toFixed(1)+'<span class="unit">°C</span></span></div></div>'}document.getElementById("temp-card").innerHTML=t

var c='';var names={"F1":"Fase 1","F2":"Fase 2","F3":"Fase 3"};for(var k in names){if(d[k]){var p=d[k];c+='<div class="card"><h2><i class="fas fa-bolt"></i>'+names[k]+'</h2>';if(p.valid){c+='<div class="row"><span class="label">Tensao</span><span class="value">'+p.v+'<span class="unit">V</span></span></div><div class="row"><span class="label">Corrente</span><span class="value">'+p.a+'<span class="unit">A</span></span></div><div class="row"><span class="label">Potencia</span><span class="value">'+p.w+'<span class="unit">W</span></span></div><div class="row"><span class="label">Energia</span><span class="value">'+p.kwh+'<span class="unit">kWh</span></span></div><div class="row"><span class="label">Frequencia</span><span class="value">'+p.hz+'<span class="unit">Hz</span></span></div><div class="row"><span class="label">F.P.</span><span class="value">'+p.pf+'</span></div>'}else{c+='<div class="row"><span class="label">Estado</span><span class="value"><span class="status-dot dot-red"></span>Falha</span></div>'}c+='</div>'}}document.getElementById("cards").innerHTML=c

var l='';if(d.limits){l='<div class="card"><h2><i class="fas fa-cog"></i>Limites Configurados</h2><div class="row"><span class="label">Tensao Max</span><span class="value">'+d.limits.v_max+'<span class="unit">V</span></span></div><div class="row"><span class="label">Tensao Min</span><span class="value">'+d.limits.v_min+'<span class="unit">V</span></span></div><div class="row"><span class="label">Corrente Max</span><span class="value">'+d.limits.a_max+'<span class="unit">A</span></span></div><div class="row"><span class="label">Corrente Min</span><span class="value">'+d.limits.a_min+'<span class="unit">A</span></span></div><div class="row"><span class="label">Temperatura Max</span><span class="value">'+d.limits.temp_max+'<span class="unit">°C</span></span></div></div>'}document.getElementById("limits-card").innerHTML=l

var f='';if(d.factors){var v=d.factors.v;var a=d.factors.a;var w=d.factors.w;f='<div class="card"><h2><i class="fas fa-sliders-h"></i>Fatores de Correcao</h2><div class="row"><span class="label">V F1/F2/F3</span><span class="value">'+v[0]+' / '+v[1]+' / '+v[2]+'</span></div><div class="row"><span class="label">A F1/F2/F3</span><span class="value">'+a[0]+' / '+a[1]+' / '+a[2]+'</span></div><div class="row"><span class="label">W F1/F2/F3</span><span class="value">'+w[0]+' / '+w[1]+' / '+w[2]+'</span></div></div>'}document.getElementById("factors-card").innerHTML=f

var a='';if(d.alarms&&d.alarms.length>0){a='<div class="card"><h2><i class="fas fa-exclamation-triangle"></i>Alarmes Ativos</h2>';for(var j=0;j<d.alarms.length;j++){a+='<span class="alarm-badge">'+d.alarms[j]+'</span>'}a+='</div>'}document.getElementById("alarms-card").innerHTML=a
document.getElementById("footer").innerHTML='Ultima atualizacao: '+d.ts}

function buildConfigForm(){
var h='<div class="card frm" id="frm-config">';
h+='<h2><i class="fas fa-cog"></i> Editar Configuracoes</h2>';
h+='<div class="frm-title">Limites</div>';
h+='<div class="field-row"><div class="field"><label>Tensao Max (V)</label><input type="number" id="cfg_v_max" step="0.1"></div><div class="field"><label>Tensao Min (V)</label><input type="number" id="cfg_v_min" step="0.1"></div></div>';
h+='<div class="field-row"><div class="field"><label>Corrente Max (A)</label><input type="number" id="cfg_a_max" step="0.1"></div><div class="field"><label>Corrente Min (A)</label><input type="number" id="cfg_a_min" step="0.01"></div></div>';
h+='<div class="field"><label>Temperatura Max (C)</label><input type="number" id="cfg_temp_max" step="0.1"></div>';
h+='<div class="frm-title">Sistema</div>';
h+='<div class="field-row"><div class="field"><label>Tensao Nominal</label><select id="cfg_v_nom"><option value="127">127V</option><option value="220">220V</option><option value="380">380V</option></select></div><div class="field"><label>Fases</label><select id="cfg_phases"><option value="1">1</option><option value="2">2</option><option value="3">3</option></select></div></div>';
h+='<div class="field-row"><div class="field"><label>Envio (s)</label><input type="number" id="cfg_send" min="1" max="3600"></div><div class="field"><label>Alarme (s)</label><input type="number" id="cfg_alarm" min="5" max="3600"></div></div>';
h+='<div class="frm-title">Fatores de Correcao</div>';
h+='<div class="field-row"><div class="field"><label>V F1</label><input type="number" id="cfg_vf0" step="0.0001"></div><div class="field"><label>V F2</label><input type="number" id="cfg_vf1" step="0.0001"></div><div class="field"><label>V F3</label><input type="number" id="cfg_vf2" step="0.0001"></div></div>';
h+='<div class="field-row"><div class="field"><label>A F1</label><input type="number" id="cfg_af0" step="0.0001"></div><div class="field"><label>A F2</label><input type="number" id="cfg_af1" step="0.0001"></div><div class="field"><label>A F3</label><input type="number" id="cfg_af2" step="0.0001"></div></div>';
h+='<div class="field-row"><div class="field"><label>W F1</label><input type="number" id="cfg_wf0" step="0.0001"></div><div class="field"><label>W F2</label><input type="number" id="cfg_wf1" step="0.0001"></div><div class="field"><label>W F3</label><input type="number" id="cfg_wf2" step="0.0001"></div></div>';
h+='<div class="msg-ok" id="msg-ok"></div><div class="msg-err" id="msg-err"></div>';
h+='<div class="btn-group"><button class="btn btn-save" onclick="saveConfig()" title="Salva localmente e reinicia"><i class="fas fa-save"></i> Salvar</button><button class="btn btn-mqtt" id="btn-mqtt" onclick="sendMqttConfig()" title="Publica via MQTT - o dispositivo reinicia ao processar"><i class="fas fa-network-wired"></i> MQTT</button><button class="btn btn-cancel" onclick="toggleConfig()"><i class="fas fa-times"></i> Cancelar</button></div>';
h+='<div class="mqtt-status" id="mqtt-status"></div>';
h+='</div>';
document.getElementById("config-card").innerHTML=h;
}

function toggleConfig(){
var f=document.getElementById("frm-config");
if(!f)buildConfigForm();
f=document.getElementById("frm-config");
f.classList.toggle("show");
if(f.classList.contains("show"))fillConfig();
}

function fillConfig(){
if(!gData)return;
var d=gData;
if(d.limits){sv("cfg_v_max",d.limits.v_max);sv("cfg_v_min",d.limits.v_min);sv("cfg_a_max",d.limits.a_max);sv("cfg_a_min",d.limits.a_min);sv("cfg_temp_max",d.limits.temp_max)}
if(d.v_nominal)sel("cfg_v_nom",d.v_nominal);
if(d.phases)sel("cfg_phases",d.phases);
if(d.send_interval_s)sv("cfg_send",d.send_interval_s);
if(d.alarm_interval_s)sv("cfg_alarm",d.alarm_interval_s);
if(d.factors){var v=d.factors.v,a=d.factors.a,w=d.factors.w;sv("cfg_vf0",v[0]);sv("cfg_vf1",v[1]);sv("cfg_vf2",v[2]);sv("cfg_af0",a[0]);sv("cfg_af1",a[1]);sv("cfg_af2",a[2]);sv("cfg_wf0",w[0]);sv("cfg_wf1",w[1]);sv("cfg_wf2",w[2])}
}

function sv(id,val){var e=document.getElementById(id);if(e)e.value=val}
function sel(id,val){var e=document.getElementById(id);if(e)e.value=val}
function gv(id){return parseFloat(document.getElementById(id).value)}

function getConfig(){
return {
v_max:gv("cfg_v_max"),v_min:gv("cfg_v_min"),
a_max:gv("cfg_a_max"),a_min:gv("cfg_a_min"),
temp_max:gv("cfg_temp_max"),
nominal_voltage:gv("cfg_v_nom"),num_phases:parseInt(document.getElementById("cfg_phases").value),
send_interval_s:parseInt(document.getElementById("cfg_send").value),
alarm_interval_s:parseInt(document.getElementById("cfg_alarm").value),
v_factor:[gv("cfg_vf0"),gv("cfg_vf1"),gv("cfg_vf2")],
a_factor:[gv("cfg_af0"),gv("cfg_af1"),gv("cfg_af2")],
w_factor:[gv("cfg_wf0"),gv("cfg_wf1"),gv("cfg_wf2")]
};
}

function saveConfig(){
var cfg=getConfig();
var x=new XMLHttpRequest();
x.open("POST","/api/config",true);
x.setRequestHeader("Content-Type","application/json");
x.onreadystatechange=function(){
if(this.readyState==4){
var ok=document.getElementById("msg-ok");
var er=document.getElementById("msg-err");
if(this.status==200){ok.style.display="block";er.style.display="none";ok.innerHTML="<i class='fas fa-check'></i> Salvo! Reiniciando..."}
else{er.style.display="block";ok.style.display="none";er.innerHTML="<i class='fas fa-times'></i> Erro ao salvar"}
}};
x.send(JSON.stringify(cfg));
}

function sendMqttConfig(){
var st=document.getElementById("mqtt-status");
var btn=document.getElementById("btn-mqtt");
btn.disabled=true;btn.style.opacity=.5;
st.className="mqtt-status";st.innerHTML="Publicando via MQTT...";
var cfg=getConfig();
var x=new XMLHttpRequest();
x.open("POST","/api/mqtt-config",true);
x.setRequestHeader("Content-Type","application/json");
x.onreadystatechange=function(){
if(this.readyState==4){
btn.disabled=false;btn.style.opacity=1;
var ok=document.getElementById("msg-ok");
var er=document.getElementById("msg-err");
if(this.status==200){
ok.style.display="block";er.style.display="none";
ok.innerHTML="<i class='fas fa-check'></i> Publicado via MQTT!";
st.className="mqtt-status ok";
st.innerHTML="<i class='fas fa-check-circle'></i> Configuracao enviada ao broker. Dispositivo reiniciara ao processa-la.";
}else{
er.style.display="block";ok.style.display="none";
var msg="Erro ao publicar via MQTT";
try{var r=JSON.parse(this.responseText);msg=r.message||msg}catch(e){}
er.innerHTML="<i class='fas fa-times'></i> "+msg;
st.className="mqtt-status fail";
st.innerHTML="<i class='fas fa-exclamation-circle'></i> MQTT nao disponivel";
}}};
x.send(JSON.stringify(cfg));
}

buildConfigForm();
loadData();setInterval(loadData,3000)
</script>
</body></html>
)rawliteral";

void checkGpioBoot() {
  pinMode(0, INPUT_PULLUP);
  unsigned long start = millis();
  bool held = true;
  while (millis() - start < 3000) {
    if (digitalRead(0) != LOW) {
      held = false;
    }
    delay(50);
  }
  if (held) {
    Serial.println("GPIO0 held >3s at boot -> AP mode");
    shouldRebootAp = true;
  }
}

void checkGpioRuntime() {
  if (digitalRead(0) == LOW) {
    unsigned long start = millis();
    while (digitalRead(0) == LOW && millis() - start < GPIO_HOLD_MS + 1000) {
      esp_task_wdt_reset();
      delay(50);
    }
    if (millis() - start >= GPIO_HOLD_MS) {
      Serial.println("GPIO0 held >5s -> reset config + AP mode");
      configManager.reset();
      ESP.restart();
    }
  }
}

void onConfigUpdated(const Config &cfg) {
  config = cfg;
}

void onTimeUpdated(unsigned long epoch) {
  timeManager.setFromEpoch(epoch);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Vigiatech Monitor Trifasico v2.0 ===");

  configManager.load(config);
  configManager.incrementBootCount();

  Serial.println("--- CONFIG ---");
  Serial.printf("wifi_ssid: %s\n", config.wifi_ssid);
  Serial.printf("mqtt_host: %s\n", config.mqtt_host);
  Serial.printf("mqtt_port: %d\n", config.mqtt_port);
  Serial.printf("topic_prefix: %s\n", config.topic_prefix);
  Serial.printf("num_phases: %d\n", config.num_phases);
  Serial.printf("nominal_voltage: %.1f\n", config.nominal_voltage);
  Serial.printf("send_interval_s: %d\n", config.send_interval_s);
  Serial.printf("alarm_interval_s: %d\n", config.alarm_interval_s);
  Serial.printf("v_max: %.1f v_min: %.1f\n", config.v_max, config.v_min);
  Serial.printf("a_max: %.1f a_min: %.1f\n", config.a_max, config.a_min);
  Serial.printf("boot_count: %d\n", configManager.getBootCount());
  Serial.println("----------------------");

  checkGpioBoot();

  bool hasConfig = strlen(config.wifi_ssid) > 0;

  if (shouldRebootAp || !hasConfig) {
    Serial.println("Iniciando Captive Portal...");
    CaptivePortal portal;
    portal.start(configManager);
    return;
  }

  Serial.printf("WiFi %s ... ", config.wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid, config.wifi_pass);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    esp_task_wdt_reset();
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.println(" FAIL - reiniciando em AP mode");
    delay(1000);
    configManager.reset();
    CaptivePortal portal;
    portal.start(configManager);
    return;
  }

  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);

  stationServer.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html", DASHBOARD_HTML);
  });

  stationServer.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *r) {
    String ts = timeManager.getTimestamp();
    alarmManager.check(phases, config.num_phases, currentTemp, config, alarmState);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<1280> doc;
#pragma GCC diagnostic pop
    doc["ts"] = ts;
    doc["ip"] = WiFi.localIP().toString();
    doc["mqtt"] = mqttManager.isConnected();
    doc["uptime"] = timeManager.getUptimeS();
    doc["phases"] = config.num_phases;
    doc["v_nominal"] = config.nominal_voltage;
    if (isnan(currentTemp)) {
      doc["temperature"] = nullptr;
    } else {
      doc["temperature"] = currentTemp;
    }
    doc["send_interval_s"] = config.send_interval_s;
    doc["alarm_interval_s"] = config.alarm_interval_s;

    JsonObject limits = doc["limits"].to<JsonObject>();
    limits["v_max"] = config.v_max;
    limits["v_min"] = config.v_min;
    limits["a_max"] = config.a_max;
    limits["a_min"] = config.a_min;
    limits["temp_max"] = config.temp_max;

    JsonObject factors = doc["factors"].to<JsonObject>();
    JsonArray vArr = factors["v"].to<JsonArray>();
    JsonArray aArr = factors["a"].to<JsonArray>();
    JsonArray wArr = factors["w"].to<JsonArray>();
    for (uint8_t i = 0; i < MAX_PHASES; i++) {
      vArr.add(config.v_factor[i]);
      aArr.add(config.a_factor[i]);
      wArr.add(config.w_factor[i]);
    }

    const char *phaseNames[] = {"F1", "F2", "F3"};
    for (uint8_t i = 0; i < config.num_phases; i++) {
      JsonObject p = doc[phaseNames[i]].to<JsonObject>();
      if (phases[i].valid) {
        p["v"] = phases[i].v;
        p["a"] = phases[i].a;
        p["w"] = phases[i].w;
        p["kwh"] = phases[i].kwh;
        p["hz"] = phases[i].hz;
        p["pf"] = phases[i].pf;
        p["valid"] = true;
      } else {
        p["valid"] = false;
      }
    }

    JsonArray alarmsArr = doc["alarms"].to<JsonArray>();
    for (uint8_t i = 0; i < alarmState.count; i++) {
      if (alarmState.active[i]) {
        alarmsArr.add(String(alarmManager.alarmName(alarmState.events[i].type)) + "_F" + String(alarmState.events[i].phase));
      }
    }

    String buf;
    serializeJson(doc, buf);
    r->send(200, "application/json", buf);
  });

  static String configBody = "";
  stationServer.on("/api/config", HTTP_POST,
    [](AsyncWebServerRequest *r) {
      if (configBody.length() == 0) {
        r->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
      }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      StaticJsonDocument<512> doc;
#pragma GCC diagnostic pop
      DeserializationError err = deserializeJson(doc, configBody);
      if (err) {
        r->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        configBody = "";
        return;
      }

      if (doc.containsKey("nominal_voltage")) config.nominal_voltage = doc["nominal_voltage"];
      if (doc.containsKey("num_phases")) { config.num_phases = doc["num_phases"]; if (config.num_phases < 1) config.num_phases = 1; if (config.num_phases > 3) config.num_phases = 3; }
      if (doc.containsKey("send_interval_s")) { config.send_interval_s = doc["send_interval_s"]; if (config.send_interval_s < 1) config.send_interval_s = 1; if (config.send_interval_s > 3600) config.send_interval_s = 3600; }
      if (doc.containsKey("alarm_interval_s")) { config.alarm_interval_s = doc["alarm_interval_s"]; if (config.alarm_interval_s < 5) config.alarm_interval_s = 5; if (config.alarm_interval_s > 3600) config.alarm_interval_s = 3600; }
      if (doc.containsKey("v_max")) config.v_max = doc["v_max"];
      if (doc.containsKey("v_min")) config.v_min = doc["v_min"];
      if (doc.containsKey("a_max")) config.a_max = doc["a_max"];
      if (doc.containsKey("a_min")) config.a_min = doc["a_min"];
      if (doc.containsKey("temp_max")) config.temp_max = doc["temp_max"];
      if (doc.containsKey("v_factor")) { JsonArray a = doc["v_factor"]; for (uint8_t i = 0; i < MAX_PHASES && i < a.size(); i++) config.v_factor[i] = a[i]; }
      if (doc.containsKey("a_factor")) { JsonArray a = doc["a_factor"]; for (uint8_t i = 0; i < MAX_PHASES && i < a.size(); i++) config.a_factor[i] = a[i]; }
      if (doc.containsKey("w_factor")) { JsonArray a = doc["w_factor"]; for (uint8_t i = 0; i < MAX_PHASES && i < a.size(); i++) config.w_factor[i] = a[i]; }

      configBody = "";

      configManager.save(config);

      r->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Config saved. Rebooting.\"}");
      delay(500);
      ESP.restart();
    },
    NULL,
    [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
      configBody += String((char *)data, len);
    }
  );

  static String mqttConfigBody = "";
  stationServer.on("/api/mqtt-config", HTTP_POST,
    [](AsyncWebServerRequest *r) {
      if (mqttConfigBody.length() == 0) {
        r->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
      }
      if (!mqttManager.isConnected()) {
        r->send(503, "application/json", "{\"status\":\"error\",\"message\":\"MQTT not connected\"}");
        mqttConfigBody = "";
        return;
      }
      mqttManager.publishConfig(config, mqttConfigBody.c_str());
      mqttConfigBody = "";
      r->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Config publicada via MQTT\"}");
    },
    NULL,
    [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
      mqttConfigBody += String((char *)data, len);
    }
  );

  stationServer.begin();
  Serial.printf("HTTP: http://%s\n", WiFi.localIP().toString().c_str());

  timeManager.init();

  mqttManager.setConfigCallback(onConfigUpdated);
  mqttManager.setTimeCallback(onTimeUpdated);
  mqttManager.connect(config);
  sensorManager.init();
}

void loop() {
  esp_task_wdt_reset();

  unsigned long now = millis();

  mqttManager.loop();

  if (!mqttManager.isConnected()) {
    if (now - lastMqttTry >= MQTT_RETRY_MS) {
      lastMqttTry = now;
      mqttManager.connect(config);
    }
  }

  checkGpioRuntime();

  if (now - lastRead >= config.send_interval_s * 1000) {
    lastRead = now;
    sensorManager.read(phases, config.num_phases, config);
    currentTemp = sensorManager.getTemperature();
    for (uint8_t i = 0; i < config.num_phases; i++) {
      phases[i].temperature = currentTemp;
    }
  }

  if (now - lastSend >= config.send_interval_s * 1000) {
    lastSend = now;
    if (mqttManager.isConnected()) {
      String ts = timeManager.getTimestamp();
      mqttManager.publishData(config, phases, currentTemp, ts);
    }
  }

  if (mqttManager.isConnected()) {
    alarmManager.check(phases, config.num_phases, currentTemp, config, alarmState);

    alarmManager.getNewAlarms(alarmState, newAlarms);
    for (uint8_t i = 0; i < newAlarms.count; i++) {
      if (newAlarms.active[i]) {
        mqttManager.publishAlarm(config, newAlarms.events[i], "ALARM",
                                 timeManager.getTimestamp());
      }
    }

    alarmManager.getResolvedAlarms(alarmState, resolvedAlarms);
    for (uint8_t i = 0; i < resolvedAlarms.count; i++) {
      if (resolvedAlarms.active[i]) {
        mqttManager.publishAlarm(config, resolvedAlarms.events[i], "RESOLVED",
                                 timeManager.getTimestamp());
      }
    }

    if (now - lastAlarmSend >= config.alarm_interval_s * 1000) {
      lastAlarmSend = now;
      alarmManager.getActiveAlarms(alarmState, activeAlarms);
      for (uint8_t i = 0; i < activeAlarms.count; i++) {
        if (activeAlarms.active[i]) {
          mqttManager.publishAlarm(config, activeAlarms.events[i], "ALARM_REFRESH",
                                   timeManager.getTimestamp());
        }
      }
    }
  }
}
