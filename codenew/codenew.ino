#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#define DHTTYPE DHT11
#include <RTClib.h>
#include <SPI.h>
#include <ArduinoJson.h>

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// WiFi credentials - change to your network
const char* ssid = "Nem";  
const char* password = "11111111";

// Hardcoded login credentials
const char* loginUsername = "admin";
const char* loginPassword = "1";

WebServer server(80);

// Simple session variable
bool loggedIn = false;

// Simulated sensor data
float soilMoisture = 45.3;

#define cbas 25    // Cảm biến ánh sáng
#define doam 35    // Cảm biến độ ẩm đất
#define dhtssn 33  // Cảm biến DHT11
#define relay1 26  // Đèn
#define relay2 27  // Bơm
#define relay3 14  // Quạt
#define relay4 13  // Phun sương
#define but 18     // Nút Up
#define dow 15     // Nút Down
#define en 19      // Nút Enter
#define lu 5       // Nút Back
#define DHTTYPE DHT11
DHT dht(dhtssn, DHTTYPE);
int i = 0;
int j = -1;
int a = 0;
int b = 0;
int c = 0;
int save = 0;

// Biến cài đặt hẹn giờ
int setHourfan_on = 23;
int setMinutefan_on = 59;
int setHourfan_off = 23;
int setMinutefan_off = 59;
int setHourbom_on = 23;
int setMinutebom_on = 59;
int setHourbom_off = 23;
int setMinutebom_off = 59;
int setHourphun_on = 23;
int setMinutephun_on = 59;
int setHourphun_off = 23;
int setMinutephun_off = 59;
int setTimerfan_on = 1;
int setTimerfan_off = 1;
int setTimerbom_on = 1;
int setTimerbom_off = 1;
int setTimerphun_on = 1;
int setTimerphun_off = 1;
int setTimerden_on = 1;
int setTimerden_off = 1;
int setHourden_on = 23;
int setMinuteden_on = 59;
int setHourden_off = 23;
int setMinuteden_off = 59;

// Biến cài đặt tự động
int set_tem_on = 35;
int set_hum_on = 50;
int set_dat_on = 90;
int set_tem_off = 28;
int set_hum_off = 65;
int set_dat_off = 95;

// Device states
bool pumpState = false;    // Bơm
bool mistState = false;    // Phun sương
bool fanState = false;     // Quạt
bool lightState = false;   // Đèn
bool lastUpState = HIGH;   // Theo dõi trạng thái nút Up
bool lastDowwState = HIGH; // Theo dõi trạng thái nút Down
bool manualOverride = false; // Ưu tiên điều khiển thủ công
unsigned long manualOverrideTime = 0; // Thời gian ưu tiên thủ công

// Uptime tracking
unsigned long lastMillis = 0;
unsigned long uptimeSeconds = 0;

// HTML login page
const char loginPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Smart Garden Login</title>
  <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1.0">
  <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap" rel="stylesheet">
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body { 
      font-family: 'Roboto', Arial, sans-serif;
      background: linear-gradient(135deg, #2E7D32 0%, #4CAF50 50%, #66BB6A 100%);
      display: flex; 
      height: 100vh; 
      justify-content: center; 
      align-items: center;
      position: relative;
      overflow: hidden;
    }
    body::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-image: 
        radial-gradient(circle at 20% 50%, rgba(255,255,255,0.1) 0%, transparent 50%),
        radial-gradient(circle at 75% 75% 20%, rgba(255,255,255,0.1) 0%, transparent 50%),
        radial-gradient(circle-circle at 40% 80%, rgba(255,255,255,0.1) 0%, transparent 50%);
      pointer-events: none;
    }
    .login-container {
      position: relative;
      z-index: 1;
    }
    .login-box { 
      background: rgba(255, 255, 255, 0.95);
      backdrop-filter: blur(10px);
      padding: 40px 30px;
      border-radius: 20px;
      box-shadow: 
        0 20px 40px rgba(0,0,0,0,0.1),
        0 0 0 1px rgba(255,255,255,0.2);
      width: 400px;
      text-align: center;
      animation: slideIn 0.8s ease-out;
    }
    @keyframes slideIn {
      from {
        opacity: 0;
        transform: translateY(30px) scale(0.95);
      }
      to {
        opacity: 1;
        transform: translateY(0) scale(1);
      }
    }
    .logo {
      width: 80px;
      height: 80px;
      background: linear-gradient(45deg, #2E7D32, #4CAF50);
      border-radius: 50%;
      margin: 0 auto 20px;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 30px;
      color: white;
      font-weight: bold;
    }
    h2 {
      color: #2E7D32;
      font-size: 28px;
      margin-bottom: 10px;
      font-weight: 600;
    }
    .subtitle {+
      color: #666;
      margin-bottom: 30px;
      font-size: 14px;
    }
    .input-group {
      position: relative;
      margin-bottom: 25px;
    }
    input[type=text], input[type=password] { 
      width: 100%; 
      padding: 15px 20px;
      border: 2px solid #E8F5E8;
      border-radius: 12px;
      font-size: 16px;
      transition: all 0.3s ease;
      background: #FAFAFA;
    }
    input[type=text]:focus, input[type=password]:focus {
      outline: none;
      border-color: #4CAF50;
      background: white;
      box-shadow: 0 0 0 3px rgba(76, 175, 80, 0.1);
      transform: translateY(-2px);
    }
    .login-btn { 
      background: linear-gradient(45deg, #2E7D32, #4CAF50);
      color: white; 
      border: none; 
      padding: 15px 30px;
      width: 100%; 
      cursor: pointer;
      border-radius: 12px;
      font-size: 16px;
      font-weight: 600;
      transition: all 0.3s ease;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    .login-btn:hover { 
      background: linear-gradient(45deg, #1B5E20, #2E7D32);
      transform: translateY(-2px);
      box-shadow: 0 10px 25px rgba(46, 125, 50, 0.3);
    }
    .login-btn:active {
      transform: translateY(0);
    }
    .footer-text {
      margin-top: 20px;
      color: #888;
      font-size: 12px;
    }
    .error-message {
      background: #ffebee;
      color: #c62828;
      padding: 10px;
      border-radius: 8px;
      margin-bottom: 20px;
      border-left: 4px solid #f44336;
    }
  </style>
</head>
<body>
  <div class="login-container">
    <div class="login-box">
      <div class="logo">🌱</div>
      <h2>Smart Garden</h2>
      <p class="subtitle">Hệ thống quản lý vườn thông minh</p>
      <form action="/login" method="POST">
        <div class="input-group">
          <input type="text" name="username" placeholder="Tên đăng nhập" required>
        </div>
        <div class="input-group">
          <input type="password" name="password" placeholder="Mật khẩu" required>
        </div>
        <button type="submit" class="login-btn">Đăng nhập</button>
      </form>
      <p class="footer-text">Phiên bản 1.0 - Đồ án IoT</p>
    </div>
  </div>
</body>
</html>
)rawliteral";

// HTML dashboard page
const char dashboardPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Smart Garden Dashboard</title>
  <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1.0">
  <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap" rel="stylesheet">
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body { 
      font-family: 'Roboto', Arial, sans-serif;
      background: linear-gradient(135deg, #E8F5E8 0%, #F1F8E9 100%);
      min-height: 100vh;
    }
    .header {
      background: linear-gradient(135deg, #2E7D32 0%, #4CAF50 100%);
      color: white;
      padding: 20px 0;
      text-align: center;
      box-shadow: 0 4px 20px rgba(46, 125, 50, 0.3);
      position: relative;
      overflow: hidden;
    }
    .header::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><defs><pattern id="grain" width="100" height="100" patternUnits="userSpaceOnUse"><circle cx="25" cy="25" r="1" fill="rgba(255,255,255,0.1)"/><circle cx="75" cy="75" r="1" fill="rgba(255,255,255,0.1)"/><circle cx="50" cy="10" r="1" fill="rgba(255,255,255,0.05)"/></pattern></defs><rect width="100" height="100" fill="url(%23grain)"/></svg>');
      pointer-events: none;
    }
    .header h1 {
      font-size: 32px;
      font-weight: 700;
      margin-bottom: 5px;
      position: relative;
      z-index: 1;
    }
    .header .subtitle {
      font-size: 16px;
      opacity: 0.9;
      position: relative;
      z-index: 1;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      padding: 30px 20px;
    }
    .dashboard-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 25px;
      margin-bottom: 30px;
    }
    .card {
      background: white;
      border-radius: 16px;
      padding: 25px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
      border: 1px solid rgba(76, 175, 80, 0.1);
      transition: all 0.3s ease;
      position: relative;
      overflow: hidden;
    }
    .card::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 4px;
      background: linear-gradient(90deg, #4CAF50, #66BB6A);
    }
    .card:hover {
      transform: translateY(-5px);
      box-shadow: 0 15px 40px rgba(0,0,0,0.15);
    }
    .card-title {
      font-size: 20px;
      font-weight: 600;
      color: #2E7D32;
      margin-bottom: 20px;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .sensor-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 15px 0;
      border-bottom: 1px solid #F5F5F5;
    }
    .sensor-item:last-child {
      border-bottom: none;
    }
    .sensor-label {
      font-weight: 500;
      color: #555;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .sensor-value {
      font-size: 24px;
      font-weight: 700;
      color: #2E7D32;
    }
    .sensor-unit {
      font-size: 16px;
      color: #666;
      margin-left: 5px;
    }
    .device-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 15px;
    }
    .device-item {
      background: #F8F9FA;
      border-radius: 12px;
      padding: 20px;
      text-align: center;
      transition: all 0.3s ease;
    }
    .device-item:hover {
      background: #F1F8E9;
    }
    .device-label {
      font-weight: 500;
      color: #555;
      margin-bottom: 15px;
      font-size: 16px;
    }
    .device-btn {
      background: linear-gradient(45deg, #757575, #9E9E9E);
      color: white;
      border: none;
      padding: 12px 24px;
      border-radius: 25px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s ease;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      width: 100%;
    }
    .device-btn.on {
      background: linear-gradient(45deg, #4CAF50, #66BB6A);
      box-shadow: 0 4px 15px rgba(76, 175, 80, 0.3);
    }
    .device-btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 8px 20px rgba(0,0,0,0.2);
    }
    .device-btn:active {
      transform: translateY(0);
    }
    .settings-section {
      margin-top: 30px;
    }
    .settings-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 25px;
    }
    .settings-item {
      background: #F8F9FA;
      border-radius: 12px;
      padding: 20px;
    }
    .settings-label {
      font-weight: 500;
      color: #555;
      margin-bottom: 10px;
      display: block;
    }
    .settings-input {
      width: 100%;
      padding: 10px;
      margin-bottom: 10px;
      border: 1px solid #E0E0E0;
      border-radius: 8px;
      font-size: 14px;
    }
    .settings-btn {
      background: linear-gradient(45deg, #2E7D32, #4CAF50);
      color: white;
      border: none;
      padding: 10px 20px;
      border-radius: 8px;
      cursor: pointer;
      font-weight: 600;
    }
    .settings-btn:hover {
      background: linear-gradient(45deg, #1B5E20, #2E7D32);
    }
    .logout-section {
      text-align: center;
      margin-top: 30px;
    }
    .logout-btn {
      background: linear-gradient(45deg, #F44336, #E57373);
      color: white;
      border: none;
      padding: 12px 30px;
      border-radius: 25px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s ease;
    }
    .logout-btn:hover {
      background: linear-gradient(45deg, #D32F2F, #F44336);
      transform: translateY(-2px);
      box-shadow: 0 8px 20px rgba(244, 67, 54, 0.3);
    }
    .footer {
      text-align: center;
      padding: 30px 0;
      background: linear-gradient(135deg, #2E7D32 0%, #4CAF50 100%);
      color: white;
      margin-top: 50px;
    }
    .status-indicator {
      width: 12px;
      height: 12px;
      border-radius: 50%;
      display: inline-block;
      margin-right: 8px;
    }
    .status-online {
      background: #4CAF50;
      box-shadow: 0 0 8px rgba(76, 175, 80, 0.5);
    }
    .status-offline {
      background: #F44336;
      box-shadow: 0 0 8px rgba(244, 67, 54, 0.5);
    }
    .loading {
      opacity: 0.6;
      pointer-events: none;
    }
    @keyframes pulse {
      0% { transform: scale(1); }
      50% { transform: scale(1.05); }
      100% { transform: scale(1); }
    }
    .sensor-value.updating {
      animation: pulse 0.5s ease-in-out;
    }
    @media (max-width: 768px) {
      .header h1 {
        font-size: 24px;
      }
      .container {
        padding: 20px 15px;
      }
      .card {
        padding: 20px;
      }
      .dashboard-grid {
        grid-template-columns: 1fr;
      }
      .device-grid {
        grid-template-columns: 1fr;
      }
      .settings-grid {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <header class="header">
    <h1>🌱 Smart Garden System</h1>
    <p class="subtitle">Hệ thống giám sát khu vườn thông minh</p>
  </header>
  <div class="container">
    <div class="dashboard-grid">
      <div class="card">
        <h2 class="card-title">
          📊 Trạng thái cảm biến
          <span class="status-indicator status-online"></span>
        </h2>
        <div class="sensor-item">
          <div class="sensor-label">
            🌡️ Nhiệt độ
          </div>
          <div>
            <span id="temperature" class="sensor-value">--</span>
            <span class="sensor-unit">°C</span>
          </div>
        </div>
        <div class="sensor-item">
          <div class="sensor-label">
            💧 Độ ẩm không khí
          </div>
          <div>
            <span id="humidity" class="sensor-value">--</span>
            <span class="sensor-unit">%</span>
          </div>
        </div>
        <div class="sensor-item">
          <div class="sensor-label">
            🌱 Độ ẩm đất
          </div>
          <div>
            <span id="soilMoisture" class="sensor-value">--</span>
            <span class="sensor-unit">%</span>
          </div>
        </div>
        <div class="sensor-item">
          <div class="sensor-label">
            ⏳ Thời gian thực
          </div>
          <div>
            <span id="uptime" class="sensor-value">--</span>
          </div>
        </div>
      </div>
      <div class="card">
        <h2 class="card-title">
          🎛️ Điều khiển thiết bị
        </h2>
        <div class="device-grid">
          <div class="device-item">
            <div class="device-label">💧 Bơm</div>
            <button id="pumpBtn" class="device-btn" onclick="toggleDevice('pump')">Bơm OFF</button>
          </div>
          <div class="device-item">
            <div class="device-label">🌫️ Phun sương</div>
            <button id="mistBtn" class="device-btn" onclick="toggleDevice('mist')">Phun sương OFF</button>
          </div>
          <div class="device-item">
            <div class="device-label">🌀 Quạt</div>
            <button id="fanBtn" class="device-btn" onclick="toggleDevice('fan')">Quạt OFF</button>
          </div>
          <div class="device-item">
            <div class="device-label">💡 Đèn</div>
            <button id="lightBtn" class="device-btn" onclick="toggleDevice('light')">Đèn OFF</button>
          </div>
        </div>
      </div>
    </div>
    <div class="settings-section">
      <div class="card">
        <h2 class="card-title">⏰ Cài đặt hẹn giờ</h2>
        <div class="settings-grid">
          <div class="settings-item">
            <label class="settings-label">Quạt Bật</label>
            <input type="time" id="fanOn" class="settings-input">
            <label class="settings-label">Quạt Tắt</label>
            <input type="time" id="fanOff" class="settings-input">
            <button class="settings-btn" onclick="setTimer('fan')">Cập nhật</button>
          </div>
          <div class="settings-item">
            <label class="settings-label">Bơm Bật</label>
            <input type="time" id="pumpOn" class="settings-input">
            <label class="settings-label">Bơm Tắt</label>
            <input type="time" id="pumpOff" class="settings-input">
            <button class="settings-btn" onclick="setTimer('pump')">Cập nhật</button>
          </div>
          <div class="settings-item">
            <label class="settings-label">Phun Sương Bật</label>
            <input type="time" id="mistOn" class="settings-input">
            <label class="settings-label">Phun Sương Tắt</label>
            <input type="time" id="mistOff" class="settings-input">
            <button class="settings-btn" onclick="setTimer('mist')">Cập nhật</button>
          </div>
          <div class="settings-item">
            <label class="settings-label">Đèn Bật</label>
            <input type="time" id="lightOn" class="settings-input">
            <label class="settings-label">Đèn Tắt</label>
            <input type="time" id="lightOff" class="settings-input">
            <button class="settings-btn" onclick="setTimer('light')">Cập nhật</button>
          </div>
        </div>
      </div>
      <div class="card">
        <h2 class="card-title">⚙️ Cài đặt tự động</h2>
        <div class="settings-grid">
          <div class="settings-item">
            <label class="settings-label">Nhiệt độ Bật (Quạt)</label>
            <input type="number" id="tempOn" min="0" max="99" class="settings-input">
            <label class="settings-label">Nhiệt độ Tắt (Quạt)</label>
            <input type="number" id="tempOff" min="0" max="99" class="settings-input">
          </div>
          <div class="settings-item">
            <label class="settings-label">Độ ẩm Bật (Phun sương)</label>
            <input type="number" id="humOn" min="0" max="99" class="settings-input">
            <label class="settings-label">Độ ẩm Tắt (Phun sương)</label>
            <input type="number" id="humOff" min="0" max="99" class="settings-input">
          </div>
          <div class="settings-item">
            <label class="settings-label">Độ ẩm đất Bật (Bơm)</label>
            <input type="number" id="soilOn" min="0" max="99" class="settings-input">
            <label class="settings-label">Độ ẩm đất Tắt (Bơm)</label>
            <input type="number" id="soilOff" min="0" max="99" class="settings-input">
            <button class="settings-btn" onclick="setAuto()">Cập nhật</button>
          </div>
        </div>
      </div>
    </div>
    <div class="logout-section">
      <button class="logout-btn" onclick="logout()">🚪 Đăng xuất</button>
    </div>
  </div>
  <footer class="footer">
    <p>🌿 Đồ án Khu vườn thông minh - IoT Project 2025</p>
  </footer>
  <script>
    function fetchData() {
      document.querySelectorAll('.sensor-value').forEach(el => {
        el.classList.add('updating');
      });
      fetch('/data')
      .then(response => {
        if (response.status === 401) {
          window.location.href = '/';
          return;
        }
        return response.json();
      })
      .then(data => {
        if (!data) return;
        document.querySelectorAll('.sensor-value').forEach(el => {
          el.classList.remove('updating');
        });
        document.getElementById('soilMoisture').textContent = data.soilMoisture.toFixed(1);
        document.getElementById('temperature').textContent = data.temperature.toFixed(1);
        document.getElementById('humidity').textContent = data.humidity.toFixed(1);
        document.getElementById('uptime').textContent = data.uptime;
      })
      .catch(error => {
        console.log('Fetch error:', error);
        document.querySelectorAll('.sensor-value').forEach(el => {
          el.classList.remove('updating');
        });
      });
    }
    function logout() {
      fetch('/logout')
      .then(() => window.location.href = '/');
    }
    function updateButtonState(id, state) {
      const btn = document.getElementById(id);
      if (state) {
        btn.textContent = btn.textContent.replace("OFF", "ON");
        btn.classList.add("on");
      } else {
        btn.textContent = btn.textContent.replace("ON", "OFF");
        btn.classList.remove("on");
      }
    }
    function fetchStates() {
      fetch('/status')
        .then(res => {
          if (res.status === 401) { window.location = '/'; return; }
          return res.json();
        })
        .then(data => {
          if (!data) return;
          updateButtonState('pumpBtn', data.pumpState);
          updateButtonState('mistBtn', data.mistState);
          updateButtonState('fanBtn', data.fanState);
          updateButtonState('lightBtn', data.lightState);
        });
    }
    function toggleDevice(device) {
      const buttons = document.querySelectorAll('.device-btn');
      buttons.forEach(btn => btn.classList.add('loading'));
      fetch('/toggle?device=' + device, { method: 'GET' })
        .then(res => {
          if (res.status === 401) {
            window.location = '/';
            return null;
          }
          if (!res.ok) {
            throw new Error('Network response was not ok');
          }
          return res.json();
        })
        .then(data => {
          if (!data) return;
          updateButtonState('pumpBtn', data.pumpState);
          updateButtonState('mistBtn', data.mistState);
          updateButtonState('fanBtn', data.fanState);
          updateButtonState('lightBtn', data.lightState);
        })
        .catch(error => {
          console.error('Toggle error:', error);
          alert('Lỗi khi điều khiển thiết bị!');
        })
        .finally(() => {
          buttons.forEach(btn => btn.classList.remove('loading'));
        });
    }
    function fetchSettings() {
      fetch('/settings')
        .then(res => {
          if (res.status === 401) { window.location = '/'; return; }
          return res.json();
        })
        .then(data => {
          if (!data) return;
          document.getElementById('fanOn').value = `${data.fanOnHour.toString().padStart(2, '0')}:${data.fanOnMinute.toString().padStart(2, '0')}`;
          document.getElementById('fanOff').value = `${data.fanOffHour.toString().padStart(2, '0')}:${data.fanOffMinute.toString().padStart(2, '0')}`;
          document.getElementById('pumpOn').value = `${data.pumpOnHour.toString().padStart(2, '0')}:${data.pumpOnMinute.toString().padStart(2, '0')}`;
          document.getElementById('pumpOff').value = `${data.pumpOffHour.toString().padStart(2, '0')}:${data.pumpOffMinute.toString().padStart(2, '0')}`;
          document.getElementById('mistOn').value = `${data.mistOnHour.toString().padStart(2, '0')}:${data.mistOnMinute.toString().padStart(2, '0')}`;
          document.getElementById('mistOff').value = `${data.mistOffHour.toString().padStart(2, '0')}:${data.mistOffMinute.toString().padStart(2, '0')}`;
          document.getElementById('lightOn').value = `${data.lightOnHour.toString().padStart(2, '0')}:${data.lightOnMinute.toString().padStart(2, '0')}`;
          document.getElementById('lightOff').value = `${data.lightOffHour.toString().padStart(2, '0')}:${data.lightOffMinute.toString().padStart(2, '0')}`;
          document.getElementById('tempOn').value = data.tempOn;
          document.getElementById('tempOff').value = data.tempOff;
          document.getElementById('humOn').value = data.humOn;
          document.getElementById('humOff').value = data.humOff;
          document.getElementById('soilOn').value = data.soilOn;
          document.getElementById('soilOff').value = data.soilOff;
        });
    }
    function setTimer(device) {
      let onTime, offTime;
      if (device === 'fan') {
        onTime = document.getElementById('fanOn').value;
        offTime = document.getElementById('fanOff').value;
      } else if (device === 'pump') {
        onTime = document.getElementById('pumpOn').value;
        offTime = document.getElementById('pumpOff').value;
      } else if (device === 'mist') {
        onTime = document.getElementById('mistOn').value;
        offTime = document.getElementById('mistOff').value;
      } else if (device === 'light') {
        onTime = document.getElementById('lightOn').value;
        offTime = document.getElementById('lightOff').value;
      }
      const [onHour, onMinute] = onTime.split(':');
      const [offHour, offMinute] = offTime.split(':');
      fetch('/setTimer', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          device: device,
          onHour: parseInt(onHour),
          onMinute: parseInt(onMinute),
          offHour: parseInt(offHour),
          offMinute: parseInt(offMinute)
        })
      })
      .then(res => {
        if (res.status === 401) { window.location = '/'; return; }
        return res.json();
      })
      .then(data => {
        if (data.success) alert('Cài đặt hẹn giờ thành công!');
        else alert('Lỗi khi cài đặt hẹn giờ.');
      });
    }
    function setAuto() {
      const tempOn = document.getElementById('tempOn').value;
      const tempOff = document.getElementById('tempOff').value;
      const humOn = document.getElementById('humOn').value;
      const humOff = document.getElementById('humOff').value;
      const soilOn = document.getElementById('soilOn').value;
      const soilOff = document.getElementById('soilOff').value;
      fetch('/setAuto', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          tempOn: parseInt(tempOn),
          tempOff: parseInt(tempOff),
          humOn: parseInt(humOn),
          humOff: parseInt(humOff),
          soilOn: parseInt(soilOn),
          soilOff: parseInt(soilOff)
        })
      })
      .then(res => {
        if (res.status === 401) { window.location = '/'; return; }
        return res.json();
      })
      .then(data => {
        if (data.success) alert('Cài đặt tự động thành công!');
        else alert('Lỗi khi cài đặt tự động.');
      });
    }
    setInterval(fetchData, 3000);
    fetchData();
    setInterval(fetchStates, 3000);
    fetchStates();
    fetchSettings();
  </script>
</body>
</html>
)rawliteral";

// Helper function to send login page
void handleRoot() {
  if (loggedIn) {
    server.send_P(200, "text/html", dashboardPage);
  } else {
    server.send_P(200, "text/html", loginPage);
  }
}

// Handle login POST
void handleLogin() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String username = server.arg("username");
  String pwd = server.arg("password");
  if (username == loginUsername && pwd == loginPassword) {
    loggedIn = true;
    server.sendHeader("Location", "/", true);
    server.send(302);
  } else {
    String message = "<div class='error-message'>Tên đăng nhập hoặc mật khẩu không đúng!</div>";
    String page = FPSTR(loginPage);
    page.replace("<form action", message + "<form action");
    server.send(200, "text/html", page);
  }
}

// Handle status request
void handleStatus() {
  if (!loggedIn) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  String json = "{";
  json += "\"pumpState\": " + String(pumpState ? "true" : "false") + ",";
  json += "\"mistState\": " + String(mistState ? "true" : "false") + ",";
  json += "\"fanState\": " + String(fanState ? "true" : "false") + ",";
  json += "\"lightState\": " + String(lightState ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// Handle toggle device
void handleToggle() {
  if (!loggedIn) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  if (!server.hasArg("device")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }
  String device = server.arg("device");
  manualOverride = true;
  manualOverrideTime = millis() + 300000; // 5 phút
  if (device == "pump") {
    pumpState = !pumpState;
    digitalWrite(relay2, pumpState ? HIGH : LOW);
  } else if (device == "mist") {
    mistState = !mistState;
    digitalWrite(relay4, mistState ? HIGH : LOW);
  } else if (device == "fan") {
    fanState = !fanState;
    digitalWrite(relay3, fanState ? HIGH : LOW);
  } else if (device == "light") {
    lightState = !lightState;
    digitalWrite(relay1, lightState ? HIGH : LOW);
  } else {
    server.send(400, "text/plain", "Unknown device");
    return;
  }
  handleStatus();
}

// Handle settings request
void handleSettings() {
  if (!loggedIn) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  String json = "{";
  json += "\"fanOnHour\": " + String(setHourfan_on) + ",";
  json += "\"fanOnMinute\": " + String(setMinutefan_on) + ",";
  json += "\"fanOffHour\": " + String(setHourfan_off) + ",";
  json += "\"fanOffMinute\": " + String(setMinutefan_off) + ",";
  json += "\"pumpOnHour\": " + String(setHourbom_on) + ",";
  json += "\"pumpOnMinute\": " + String(setMinutebom_on) + ",";
  json += "\"pumpOffHour\": " + String(setHourbom_off) + ",";
  json += "\"pumpOffMinute\": " + String(setMinutebom_off) + ",";
  json += "\"mistOnHour\": " + String(setHourphun_on) + ",";
  json += "\"mistOnMinute\": " + String(setMinutephun_on) + ",";
  json += "\"mistOffHour\": " + String(setHourphun_off) + ",";
  json += "\"mistOffMinute\": " + String(setMinutephun_off) + ",";
  json += "\"lightOnHour\": " + String(setHourden_on) + ",";
  json += "\"lightOnMinute\": " + String(setMinuteden_on) + ",";
  json += "\"lightOffHour\": " + String(setHourden_off) + ",";
  json += "\"lightOffMinute\": " + String(setMinuteden_off) + ",";
  json += "\"tempOn\": " + String(set_tem_on) + ",";
  json += "\"tempOff\": " + String(set_tem_off) + ",";
  json += "\"humOn\": " + String(set_hum_on) + ",";
  json += "\"humOff\": " + String(set_hum_off) + ",";
  json += "\"soilOn\": " + String(set_dat_on) + ",";
  json += "\"soilOff\": " + String(set_dat_off);
  json += "}";
  server.send(200, "application/json", json);
}

// Handle timer settings
void handleSetTimer() {
  if (!loggedIn) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String body = server.arg("plain");
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }
  String device = doc["device"];
  int onHour = doc["onHour"];
  int onMinute = doc["onMinute"];
  int offHour = doc["offHour"];
  int offMinute = doc["offMinute"];
  if (onHour < 0 || onHour > 23 || onMinute < 0 || onMinute > 59 ||
      offHour < 0 || offHour > 23 || offMinute < 0 || offMinute > 59) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid time values\"}");
    return;
  }
  if (device == "fan") {
    setHourfan_on = onHour;
    setMinutefan_on = onMinute;
    setHourfan_off = offHour;
    setMinutefan_off = offMinute;
    setTimerfan_on = 1;
    setTimerfan_off = 1;
  } else if (device == "pump") {
    setHourbom_on = onHour;
    setMinutebom_on = onMinute;
    setHourbom_off = offHour;
    setMinutebom_off = offMinute;
    setTimerbom_on = 1;
    setTimerbom_off = 1;
  } else if (device == "mist") {
    setHourphun_on = onHour;
    setMinutephun_on = onMinute;
    setHourphun_off = offHour;
    setMinutephun_off = offMinute;
    setTimerphun_on = 1;
    setTimerphun_off = 1;
  } else if (device == "light") {
    setHourden_on = onHour;
    setMinuteden_on = onMinute;
    setHourden_off = offHour;
    setMinuteden_off = offMinute;
    setTimerden_on = 1;
    setTimerden_off = 1;
  } else {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Unknown device\"}");
    return;
  }
  server.send(200, "application/json", "{\"success\": true}");
}

// Handle auto settings
void handleSetAuto() {
  if (!loggedIn) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String body = server.arg("plain");
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }
  int tempOn = doc["tempOn"];
  int tempOff = doc["tempOff"];
  int humOn = doc["humOn"];
  int humOff = doc["humOff"];
  int soilOn = doc["soilOn"];
  int soilOff = doc["soilOff"];
  if (tempOn < 0 || tempOn > 99 || tempOff < 0 || tempOff > 99 ||
      humOn < 0 || humOn > 99 || humOff < 0 || humOff > 99 ||
      soilOn < 0 || soilOn > 99 || soilOff < 0 || soilOff > 99) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid values\"}");
    return;
  }
  set_tem_on = tempOn;
  set_tem_off = tempOff;
  set_hum_on = humOn;
  set_hum_off = humOff;
  set_dat_on = soilOn;
  set_dat_off = soilOff;
  server.send(200, "application/json", "{\"success\": true}");
}

String getUptimeString() {
  unsigned long currentMillis = millis();
    DateTime now = rtc.now();
  if (currentMillis < lastMillis) { // Handle millis() overflow
    lastMillis = currentMillis;
  }
  uptimeSeconds = currentMillis / 1000;
  lastMillis = currentMillis;

  unsigned long days = now.day();
  unsigned long hours = now.hour();
  unsigned long minutes = now.minute();
  unsigned long seconds = now.second();

  String uptimeStr = "";
  if (days > 0) uptimeStr += "";
  uptimeStr += String(hours) + ":";
  if (minutes < 10) uptimeStr += "0";
  uptimeStr += String(minutes) + ":";
  if (seconds < 10) uptimeStr += "0";
  uptimeStr += String(seconds);
  return uptimeStr;
}

void handleData() {
  if (!loggedIn) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  float da = analogRead(doam);
  float soilMoisture = (100 - ((da / 4095.0) * 100));
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  String json = "{";
  json += "\"soilMoisture\": " + String(soilMoisture, 1) + ",";
  json += "\"temperature\": " + String(temperature, 1) + ",";
  json += "\"humidity\": " + String(humidity, 1) + ",";
  json += "\"uptime\": \"" + getUptimeString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleLogout() {
  loggedIn = false;
  server.sendHeader("Location", "/", true);
  server.send(302);
}

void setup() {
  Serial.begin(9600);
  pinMode(cbas, INPUT);
  dht.begin();
  delay(1000);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/toggle", HTTP_GET, handleToggle);
  server.on("/data", HTTP_GET, handleData);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/setTimer", HTTP_POST, handleSetTimer);
  server.on("/setAuto", HTTP_POST, handleSetAuto);
  server.begin();
  Serial.println("HTTP server started");
  rtc.begin();
  if (!rtc.begin()) {
    Serial.println("RTC not found");
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time...");
    rtc.adjust(DateTime(2026, 1, 3, 12, 37, 0)); // Cập nhật thời gian mặc định
  }
  pinMode(doam, INPUT);
  pinMode(but, INPUT_PULLUP);
  pinMode(dow, INPUT_PULLUP);
  pinMode(en, INPUT_PULLUP);
  pinMode(lu, INPUT_PULLUP);
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);
  digitalWrite(relay1, LOW); // Đèn
  digitalWrite(relay2, LOW); // Bơm
  digitalWrite(relay3, LOW); // Quạt
  digitalWrite(relay4, LOW); // Phun sương
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Hello every body");
  lcd.setCursor(0, 2);
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Garden");
  lcd.setCursor(0, 2);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
  delay(3000);
  lcd.clear();
}

void dislcd() {
  int da = analogRead(doam);
  float doa = (100-((da / 4095.0) * 100));
  DateTime now = rtc.now();
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int up = digitalRead(but);
  int doww = digitalRead(dow);
  int enter = digitalRead(en);
  int back = digitalRead(lu);
  int currentHour = now.hour();
  int currentMinute = now.minute();
  int currentSecond = now.second();
  //------------------------------------------------------------------------------------------------------------------------------------------
  //------------------------------------------------------------------------------------------------------------------------------------------
  if (!fanState) {
    if(((now.hour()==setHourfan_on)&&(now.minute()==setMinutefan_on)&&(now.second()==0)))
    {
      // digitalWrite(relay3, HIGH);
      fanState = true;
    }
    else if((now.hour()==setHourfan_off)&&(now.minute()==setMinutefan_off)&&(now.second()==0))
    {
      // digitalWrite(relay3, LOW);
      fanState = false;
    }
  }
  if (!pumpState) {
    if((now.hour()==setHourbom_on)&&(now.minute()==setMinutebom_on)&&(now.second()==0))
    {
      // digitalWrite(relay2, HIGH);
      pumpState = true;
    }
    else if((now.hour()==setHourbom_off)&&(now.minute()==setMinutebom_off)&&(now.second()==0))
    {
      // digitalWrite(relay2, LOW);
      pumpState = false;
    }
  }
  if (!mistState) {
    if((now.hour()==setHourphun_on)&&(now.minute()==setMinutephun_on)&&(now.second()==0))
    {
      digitalWrite(relay4, HIGH);
      mistState = true;
    }
    else if((now.hour()==setHourphun_off)&&(now.minute()==setMinutephun_off)&&(now.second()==0))
    {
      digitalWrite(relay4, LOW);
      mistState = false;
    }
  }
  if (!lightState) {
    if((now.hour()==setHourden_on)&&(now.minute()==setMinuteden_on)&&(now.second()==0))
    {
      digitalWrite(relay1, HIGH);
      lightState = true;
    }
    else if((now.hour()==setHourden_off)&&(now.minute()==setMinuteden_off)&&(now.second()==0))
    {
      digitalWrite(relay1, LOW);
      lightState = false;
    }
  }
  if((now.hour()==setHourphun_on)&&(now.minute()==setMinutephun_on)&&(now.second()==0))
    {
      digitalWrite(relay4, HIGH);
      
    } 
     if((now.hour()==setHourphun_off)&&(now.minute()==setMinutephun_off)&&(now.second()==0))
    {
      digitalWrite(relay4, LOW);
      
    }
    if((now.hour()==setHourbom_on)&&(now.minute()==setMinutebom_on)&&(now.second()==0))
    {
       digitalWrite(relay2, HIGH);
      
    }
    if((now.hour()==setHourbom_off)&&(now.minute()==setMinutebom_off)&&(now.second()==0))
    {
       digitalWrite(relay2, LOW);
     
    }
     if(((now.hour()==setHourfan_on)&&(now.minute()==setMinutefan_on)&&(now.second()==0)))
    {
       digitalWrite(relay3, HIGH);
     
    }
    else if((now.hour()==setHourfan_off)&&(now.minute()==setMinutefan_off)&&(now.second()==0))
    {
       digitalWrite(relay3, LOW);
      
    }
     if (setTimerden_on == 1 && !lightState) {
    if (currentHour == setHourden_on && currentMinute == setMinuteden_on && currentSecond == 0) {
      lightState = true;
      digitalWrite(relay1, HIGH);
    }
  }
  if (setTimerden_off == 1 && lightState) {
    if (currentHour == setHourden_off && currentMinute == setMinuteden_off && currentSecond == 0) {
      lightState = false;
      digitalWrite(relay1, LOW);
    }
  }
  //-----------------------------
    if (setTimerfan_on == 1 && !fanState) {
    if (currentHour == setHourfan_on && currentMinute == setMinutefan_on && currentSecond == 0) {
      fanState = true;
      digitalWrite(relay3, HIGH);
    }
  }
  if (setTimerfan_off == 1 && fanState) {
    if (currentHour == setHourfan_off && currentMinute == setMinutefan_off && currentSecond == 0) {
      fanState = false;
      digitalWrite(relay3, LOW);
    }
  }
  if (setTimerbom_on == 1 && !pumpState) {
    if (currentHour == setHourbom_on && currentMinute == setMinutebom_on && currentSecond == 0) {
      pumpState = true;
      digitalWrite(relay2, HIGH);
    }
  }
  if (setTimerbom_off == 1 && pumpState) {
    if (currentHour == setHourbom_off && currentMinute == setMinutebom_off && currentSecond == 0) {
      pumpState = false;
      digitalWrite(relay2, LOW);
    }
  }
  if (setTimerphun_on == 1 && !mistState) {
    if (currentHour == setHourphun_on && currentMinute == setMinutephun_on && currentSecond == 0) {
      mistState = true;
      digitalWrite(relay4, HIGH);
    }
  }
  if (setTimerphun_off == 1 && mistState) {
    if (currentHour == setHourphun_off && currentMinute == setMinutephun_off && currentSecond == 0) {
      mistState = false;
      digitalWrite(relay4, LOW);
    }
  }
  //------------------------------------------------------------------------------------------------------------------------------------------
  //------------------------------------------------------------------------------------------------------------------------------------------
  if(int(t) == set_tem_on) {
    digitalWrite(relay3, HIGH);
  }
  if(int(t) == set_tem_off) {
    digitalWrite(relay3, LOW);
  }
  //----------------------------------------------------------------------------------------------------------------
  //----------------------------------------------------------------------------------------------------------------
  if(int(h) == set_hum_on) {
    digitalWrite(relay4, HIGH);

  }
  if(int(h) == set_hum_off) {
    digitalWrite(relay4, LOW);
  }
  //----------------------------------------------------------------------------------------------------------------
  //----------------------------------------------------------------------------------------------------------------
  if(int(doa) == set_dat_on) {
    digitalWrite(relay2, HIGH);
  }
  if(int(doa) == set_dat_off) {
    digitalWrite(relay2, LOW);
  }
  //----------------------------------------------------------------------------------------------------------------
  //----------------------------------------------------------------------------------------------------------------------------------------
  // if(digitalRead(cbas) == HIGH){
  //   digitalWrite(relay1, HIGH);
  // }
  // if(digitalRead(cbas)==LOW)
  // {
  //   digitalWrite(relay1, LOW);
  // }

  // Auto control
  if (!manualOverride || millis() > manualOverrideTime) {
    manualOverride = false;
    if (int(t) >= set_tem_on && !fanState) {
      digitalWrite(relay3, HIGH);
      fanState = true;
    }
    if (int(t) <= set_tem_off && fanState) {
      digitalWrite(relay3, LOW);
      fanState = false;
    }
    if (int(h) >= set_hum_on && !mistState) {
      digitalWrite(relay4, HIGH);
      mistState = true;
    }
    if (int(h) <= set_hum_off && mistState) {
      digitalWrite(relay4, LOW);
      mistState = false;
    }
    if (int(doa) <= set_dat_on && !pumpState) {
      digitalWrite(relay2, HIGH);
      pumpState = true;
    }
    if (int(doa) >= set_dat_off && pumpState) {
      digitalWrite(relay2, LOW);
      pumpState = false;
    }
  }
  // Light sensor
  if (!manualOverride && digitalRead(cbas) == HIGH && !lightState) {
    digitalWrite(relay1, HIGH);
    lightState = true;
  }
  if (!manualOverride && digitalRead(cbas) == LOW && lightState) {
    digitalWrite(relay1, LOW);
    lightState = false;
  }
  //---------------------------------------------------------------------------------------------------------------------------------------
  //--------------------------------------------------------------------------------------------------------------------------------------
if (up == LOW && dow == LOW) {
  };
  if(save == 0) {
    if (up == HIGH) {
      if(up == HIGH) {
        i+=1;
      };
      if(i>=6) {
        i=0;
      }
    };
    if (doww == HIGH) {
      if(i <=0 ) {
        i = 5;
      }
      else{
        i = i - 1;
      }
    };
  }
  if (enter == LOW) {
    j = i;
    save = 1;
  };
  if (back == LOW) {
    j = -1;
    save= 0;
    a=0;
  };
  if(j==-1) { 
    if(i==0) {
      lcd.setCursor(0, 0);
      lcd.print(">Temp & Hum         ");
      lcd.setCursor(0, 1);
      lcd.print(" Date & Time        ");
      lcd.setCursor(0, 2);
      lcd.print(" Device             ");
      lcd.setCursor(0, 3);
      lcd.print(" Do Am Dat          ");
    }
    else if(i==1) {
      lcd.setCursor(0, 0);
      lcd.print(" Temp & Hum         ");
      lcd.setCursor(0, 1);
      lcd.print(">Date & Time        ");
      lcd.setCursor(0, 2);
      lcd.print(" Device             ");
      lcd.setCursor(0, 3);
      lcd.print(" Do Am Dat          ");
    }
    else if(i==2) {
      lcd.setCursor(0, 0);
      lcd.print(" Temp & Hum         ");
      lcd.setCursor(0, 1);
      lcd.print(" Date & Time        ");
      lcd.setCursor(0, 2);
      lcd.print(">Device             ");
      lcd.setCursor(0, 3);
      lcd.print(" Do Am Dat          ");
    }
    else if(i==3) {
      lcd.setCursor(0, 0);
      lcd.print(" Temp & Hum         ");
      lcd.setCursor(0, 1);
      lcd.print(" Date & Time        ");
      lcd.setCursor(0, 2);
      lcd.print(" Device             ");
      lcd.setCursor(0, 3);
      lcd.print(">Do Am Dat          ");
    }
    else if(i==4) {
      lcd.setCursor(0, 0);
      lcd.print(" Date & Time        ");
      lcd.setCursor(0, 1);
      lcd.print(" Device             ");
      lcd.setCursor(0, 2);
      lcd.print(" Do Am Dat          ");
      lcd.setCursor(0, 3);
      lcd.print(">Hen gio            ");
    }
    else if(i==5) {
      lcd.setCursor(0, 0);
      lcd.print(" Device             ");
      lcd.setCursor(0, 1);
      lcd.print(" Do Am Dat          ");
      lcd.setCursor(0, 2);
      lcd.print(" Hen gio            ");
      lcd.setCursor(0, 3);
      lcd.print(">Set Auto           ");
    }
  }
  else if(j==0) {
    lcd.setCursor(0, 0);
    lcd.print("   Temp:     ");
    lcd.setCursor(0, 1);
    lcd.print("        ");
    lcd.print(t);
    lcd.print(" *C     ");
    lcd.setCursor(0, 2);
    lcd.print("   Humi:        ");
    lcd.setCursor(0, 3);
    lcd.print("        ");
    lcd.print(h);
    lcd.print(" %     ");
  }
  else if(j==4) {
    if(save==1) {
      if (enter == LOW) {
        if(a <=0 ) {
          a = 5;
          delay(100);
        }
        else{
          a = a - 1;
          delay(100);
        }
      };
    }
    if(a==5) {
      if(save==1) {
        if (up == LOW) {
          if(setHourfan_on <= 0 ) {
            setHourfan_on = 23;
            delay(100);
          }
          else{
            setHourfan_on = setHourfan_on - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(setMinutefan_on <= 0 ) {
            setMinutefan_on = 59;
            delay(100);
          }
          else{
            setMinutefan_on = setMinutefan_on - 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print(">Set Quat On : ");
      lcd.print(setHourfan_on);
      lcd.print(":");
      lcd.print(setMinutefan_on); 
       lcd.print(" ");
      lcd.setCursor(0, 1);
      lcd.print(" Set Quat Off: ");
      lcd.print(setHourfan_off);
      lcd.print(":");
      lcd.print(setMinutefan_off);
       lcd.print(" ");
      lcd.setCursor(0, 2);
      lcd.print(" Set Bom On  : ");
      lcd.print(setHourbom_on);
      lcd.print(":");
      lcd.print(setMinutebom_on); 
       lcd.print(" ");
      lcd.setCursor(0, 3);
      lcd.print(" Set Bom Off : ");
      lcd.print(setHourbom_off);
      lcd.print(":");
      lcd.print(setMinutebom_off);
       lcd.print(" ");
    }
    else if(a==4) {
      if(save==1) {
        if (up == LOW) {
          if(setHourfan_off <= 0 ) {
            setHourfan_off = 23;
            delay(100);
          }
          else{
            setHourfan_off = setHourfan_off - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(setMinutefan_off <= 0 ) {
            setMinutefan_off = 59;
            delay(100);
          }
          else{
            setMinutefan_off = setMinutefan_off - 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print(" Set Quat On : ");
      lcd.print(setHourfan_on);
      lcd.print(":");
      lcd.print(setMinutefan_on); 
       lcd.print(" ");
      lcd.setCursor(0, 1);
      lcd.print(">Set Quat Off: ");
      lcd.print(setHourfan_off);
      lcd.print(":");
      lcd.print(setMinutefan_off);
       lcd.print(" ");
      lcd.setCursor(0, 2);
      lcd.print(" Set Bom On  : ");
      lcd.print(setHourbom_on);
      lcd.print(":");
      lcd.print(setMinutebom_on); 
      lcd.print(" ");
      lcd.setCursor(0, 3);
      lcd.print(" Set Bom Off : ");
      lcd.print(setHourbom_off);
      lcd.print(":");
      lcd.print(setMinutebom_off);
      lcd.print(" ");
    }
    else if(a==3) {
      if(save==1) {
        if (up == LOW) {
          if(setHourbom_on <= 0 ) {
            setHourbom_on = 23;
            delay(100);
          }
          else{
            setHourbom_on = setHourbom_on - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(setMinutebom_on <= 0 ) {
            setMinutebom_on = 59;
            delay(100);
          }
          else{
            setMinutebom_on = setMinutebom_on - 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print(" Set Quat On : ");
      lcd.print(setHourfan_on);
      lcd.print(":");
      lcd.print(setMinutefan_on); 
      lcd.print(" ");
      lcd.setCursor(0, 1);
      lcd.print(" Set Quat Off: ");
      lcd.print(setHourfan_off);
      lcd.print(":");
      lcd.print(setMinutefan_off);
      lcd.print(" ");
      lcd.setCursor(0, 2);
      lcd.print(">Set Bom On  : ");
      lcd.print(setHourbom_on);
      lcd.print(":");
      lcd.print(setMinutebom_on); 
      lcd.print(" ");
      lcd.setCursor(0, 3);
      lcd.print(" Set Bom Off : ");
      lcd.print(setHourbom_off);
      lcd.print(":");
      lcd.print(setMinutebom_off);
      lcd.print(" ");
    }
    else if(a==2) {
      if(save==1) {
        if (up == LOW) {
          if(setHourbom_off <= 0 ) {
            setHourbom_off = 23;
            delay(100);
          }
          else{
            setHourbom_off = setHourbom_off - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(setMinutebom_off <= 0 ) {
            setMinutebom_off = 59;
            delay(100);
          }
          else{
            setMinutebom_off = setMinutebom_off - 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print(" Set Quat On : ");
      lcd.print(setHourfan_on);
      lcd.print(":");
      lcd.print(setMinutefan_on); 
      lcd.print(" ");
      lcd.setCursor(0, 1);
      lcd.print(" Set Quat Off: ");
      lcd.print(setHourfan_off);
      lcd.print(":");
      lcd.print(setMinutefan_off);
      lcd.print(" ");
      lcd.setCursor(0, 2);
      lcd.print(" Set Bom On  : ");
      lcd.print(setHourbom_on);
      lcd.print(":");
      lcd.print(setMinutebom_on); 
      lcd.print(" ");
      lcd.setCursor(0, 3);
      lcd.print(">Set Bom Off : ");
      lcd.print(setHourbom_off);
      lcd.print(":");
      lcd.print(setMinutebom_off);
      lcd.print(" ");
    }
    else if(a==1) {
      if(save==1) {
        if (up == LOW) {
          if(setHourphun_on <= 0 ) {
            setHourphun_on = 23;
            delay(100);
          }
          else{
            setHourphun_on = setHourphun_on - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(setMinutephun_on <= 0 ) {
            setMinutephun_on = 59;
            delay(100);
          }
          else{
            setMinutephun_on = setMinutephun_on - 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print(" Set Quat Off: ");
      lcd.print(setHourfan_off);
      lcd.print(":");
      lcd.print(setMinutefan_off);
      lcd.print(" ");
      lcd.setCursor(0, 1);
      lcd.print(" Set Bom On  : ");
      lcd.print(setHourbom_on);
      lcd.print(":");
      lcd.print(setMinutebom_on); 
      lcd.print(" ");
      lcd.setCursor(0, 2);
      lcd.print(" Set Bom Off : ");
      lcd.print(setHourbom_off);
      lcd.print(":");
      lcd.print(setMinutebom_off);
      lcd.print(" ");
      lcd.setCursor(0, 3);
      lcd.print(">Set Phun On : ");
      lcd.print(setHourphun_on);
      lcd.print(":");
      lcd.print(setMinutephun_on);
      lcd.print(" ");
    }
    else if(a==0) {
      if(save==1) {
        if (up == LOW) {
          if(setHourphun_off <= 0 ) {
            setHourphun_off = 23;
            delay(100);
          }
          else{
            setHourphun_off = setHourphun_off - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(setMinutephun_off <= 0 ) {
            setMinutephun_off = 59;
            delay(100);
          }
          else{
            setMinutephun_off = setMinutephun_off - 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print(" Set Bom On  : ");
      lcd.print(setHourbom_on);
      lcd.print(":");
      lcd.print(setMinutebom_on); 
      lcd.print(" ");
      lcd.setCursor(0, 1);
      lcd.print(" Set Bom Off : ");
      lcd.print(setHourbom_off);
      lcd.print(":");
      lcd.print(setMinutebom_off);
      lcd.print(" ");
      lcd.setCursor(0, 2);
      lcd.print(" Set Phun On : ");
      lcd.print(setHourphun_on);
      lcd.print(":");
      lcd.print(setMinutephun_on);
      lcd.print(" ");
      lcd.setCursor(0, 3);
      lcd.print(">Set Phun Off: ");
      lcd.print(setHourphun_off);
      lcd.print(":");
      lcd.print(setMinutephun_off);
      lcd.print(" ");
    }
  }
  else if(j==1) {
    lcd.setCursor(0, 0);
    lcd.print("Date: ");
    lcd.print(now.day());
    lcd.print("/");
    lcd.print(now.month());
    lcd.print("/");
    lcd.print(now.year());
    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    lcd.print(now.hour(), DEC);
    lcd.print(':');
    if (now.minute() < 10) lcd.print("0");
    lcd.print(now.minute(), DEC);
    lcd.print(':');
    if (now.second() < 10) lcd.print("0");
    lcd.print(now.second(), DEC);
    lcd.setCursor(0, 2);
    lcd.print("                  ");
    lcd.setCursor(0, 3);
    lcd.print("                  ");
  }
  else if(j==2) {
    static bool stateR1 = LOW;
    static bool stateR2 = LOW;
    static bool stateR3 = LOW;
    static bool stateR4 = LOW; 
    if(save==1) {
      if (enter == LOW) {
        if(b <=0 ) {
          b = 3;
          delay(100);
        }
        else{
          b = b - 1;
          delay(100);
        }
      };
    }
    if(b==3) {
      if(up == LOW) {
        digitalWrite(relay1, HIGH);
        lightState = true;
      }
      else if(doww == LOW) {
        digitalWrite(relay1, LOW);
        lightState = false;
      }
      lcd.setCursor(0, 0);
      lcd.print(">Den : ");
      if(digitalRead(relay1) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 1);
      lcd.print(" Bom : ");
      if(digitalRead(relay2) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 2);
      lcd.print(" Quat: ");
      if(digitalRead(relay3) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 3);
      lcd.print(" Phun: ");
      if(digitalRead(relay4) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
    }
    if(b==2) {
      if(up == LOW) {
        digitalWrite(relay2, HIGH);
        pumpState = true;
      }
      else if(doww == LOW) {
        digitalWrite(relay2, LOW);
        pumpState = false;
      }
      lcd.setCursor(0, 0);
      lcd.print(" Den : ");
      if(digitalRead(relay1) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 1);
      lcd.print(">Bom : ");
      if(digitalRead(relay2) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 2);
      lcd.print(" Quat: ");
      if(digitalRead(relay3) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 3);
      lcd.print(" Phun: ");
      if(digitalRead(relay4) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
    }
    if(b==1) {
      if(up == LOW) {
        digitalWrite(relay3, HIGH);
        fanState = true;
      }
      else if(doww == LOW) {
        digitalWrite(relay3, LOW);
        fanState = false;
      }
      lcd.setCursor(0, 0);
      lcd.print(" Den : ");
      if(digitalRead(relay1) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 1);
      lcd.print(" Bom : ");
      if(digitalRead(relay2) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 2);
      lcd.print(">Quat: ");
      if(digitalRead(relay3) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 3);
      lcd.print(" Phun: ");
      if(digitalRead(relay4) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
    }
    if(b==0) {
      if(up == LOW) {
        digitalWrite(relay4, HIGH);
        mistState = true;
      }
      else if(doww == LOW) {
        digitalWrite(relay4, LOW);
        mistState = false;
      }
      lcd.setCursor(0, 0);
      lcd.print(" Den : ");
      if(digitalRead(relay1) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 1);
      lcd.print(" Bom : ");
      if(digitalRead(relay2) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 2);
      lcd.print(" Quat: ");
      if(digitalRead(relay3) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
      lcd.setCursor(0, 3);
      lcd.print(">Phun: ");
      if(digitalRead(relay4) == HIGH) {
        lcd.print("ON     ");
      }
      else {
        lcd.print("OFF     ");
      }
    }
  }
  else if(j==3) {
    lcd.setCursor(0, 0);
    lcd.print("  Do Am Dat: ");
    lcd.print(doa);
    lcd.print(" % ");
    lcd.setCursor(0, 1);
    lcd.print("              ");
    lcd.setCursor(0, 2);
    lcd.print("   State:        ");
    if(digitalRead(cbas)==HIGH) {
      lcd.setCursor(0, 3);
      lcd.print("        ");
      lcd.print(" Ban dem  ");
    }
    else {
      lcd.setCursor(0, 3);
      lcd.print("        ");
      lcd.print(" Ban ngay  ");
    }
    delay(1500);
  }
  else if(j==5) {
    if(save==1) {
      if (enter == LOW) {
        if(c <=0 ) {
          c = 5;
          delay(100);
        }
        else{
          c = c - 1;
          delay(100);
        }
      };
    }
    if(c==5) {
      if(save==1) {
        if (up == LOW) {
          if(set_tem_on <= 0 ) {
            set_tem_on = 99;
            delay(100);
          }
          else{
            set_tem_on= set_tem_on - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(set_tem_on >= 99 ) {
            set_tem_on = 0;
            delay(100);
          }
          else{
            set_tem_on= set_tem_on + 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print("     SET AUTO");

      lcd.setCursor(0, 1);
      lcd.print(">Set Temp on : ");
      lcd.print(set_tem_on);
      lcd.print("*C");
      lcd.print("  ");

      lcd.setCursor(0, 2);
      lcd.print(" Set Temp off: ");
      lcd.print(set_tem_off);
      lcd.print("*C");
      lcd.print("  ");

      lcd.setCursor(0, 3);
      lcd.print(" Set Humi on : ");
      lcd.print(set_hum_on);
      lcd.print("%");
      lcd.print("  ");

      // lcd.setCursor(0, 3);
      // lcd.print(" Set Humi off: ");
      // lcd.print(set_hum);
      // lcd.print(" % ");
      
      // lcd.setCursor(0, 3);
      // lcd.print(" Set Soil on : ");
      // lcd.print(set_dat);
      // lcd.print(" % ");

      // lcd.setCursor(0, 3);
      // lcd.print(" Set Soil off: ");
      // lcd.print(set_dat);
      // lcd.print(" % ");
    }
    if(c==4) {
      if(save==1) {
        if (up == LOW) {
          if(set_tem_off <= 0 ) {
            set_tem_off = 99;
            delay(100);
          }
          else{
            set_tem_off = set_tem_off - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(set_tem_off >= 99 ) {
            set_tem_off = 0;
            delay(100);
          }
          else{
            set_tem_off = set_tem_off + 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print("     SET AUTO ");

      lcd.setCursor(0, 1);
      lcd.print(" Set Temp on : ");
      lcd.print(set_tem_on);
      lcd.print("*C");
      lcd.print("  ");

      lcd.setCursor(0, 2);
      lcd.print(">Set Temp off: ");
      lcd.print(set_tem_off);
      lcd.print("*C");
      lcd.print("  ");

      lcd.setCursor(0, 3);
      lcd.print(" Set Humi on : ");
      lcd.print(set_hum_on);
      lcd.print("%");
      lcd.print("  ");
    }
    if(c==3) {
      if(save==1) {
        if (up == LOW) {
          if(set_hum_on <= 0 ) {
            set_hum_on = 99;
            delay(100);
          }
          else{
            set_hum_on = set_hum_on - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(set_hum_on >= 99) {
            set_hum_on = 0;
            delay(100);
          }
          else{
            set_hum_on = set_hum_on + 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print("     SET AUTO ");
      
      lcd.setCursor(0, 1);
      lcd.print(" Set Temp on : ");
      lcd.print(set_tem_on);
      lcd.print("*C");
      lcd.print("  ");

      lcd.setCursor(0, 2);
      lcd.print(" Set Temp off: ");
      lcd.print(set_tem_off);
      lcd.print("*C");
      lcd.print("  ");

      lcd.setCursor(0, 3);
      lcd.print(">Set Humi on : ");
      lcd.print(set_hum_on);
      lcd.print("%");
      lcd.print("  ");
    }
    if(c==2) {
      if(save==1) {
        if (up == LOW) {
          if(set_hum_off <= 0 ) {
            set_hum_off = 99;
            delay(100);
          }
          else{
            set_hum_off = set_hum_off - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(set_hum_off >= 99) {
            set_hum_off = 0;
            delay(100);
          }
          else{
            set_hum_off = set_hum_off + 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print("     SET AUTO");
      
      lcd.setCursor(0, 1);
      lcd.print(" Set Temp off: ");
      lcd.print(set_tem_off);
      lcd.print("*C");
      lcd.print("  ");

      lcd.setCursor(0, 2);
      lcd.print(" Set Humi on : ");
      lcd.print(set_hum_on);
      lcd.print("%");
      lcd.print("  ");

      lcd.setCursor(0, 3);
      lcd.print(">Set Humi off: ");
      lcd.print(set_hum_off);
      lcd.print("%");
      lcd.print("  ");
    }
    if(c==1) {
      if(save==1) {
        if (up == LOW) {
          if(set_dat_on <= 0 ) {
            set_dat_on = 99;
            delay(100);
          }
          else{
            set_dat_on = set_dat_on - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(set_dat_on >= 99) {
            set_dat_on = 0;
            delay(100);
          }
          else{
            set_dat_on = set_dat_on + 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print("     SET AUTO");
      
      lcd.setCursor(0, 1);
      lcd.print(" Set Humi on : ");
      lcd.print(set_hum_on);
      lcd.print("%");
      lcd.print("  ");

      lcd.setCursor(0, 2);
      lcd.print(" Set Humi off: ");
      lcd.print(set_hum_off);
      lcd.print("%");
      lcd.print("  ");
      
      lcd.setCursor(0, 3);
      lcd.print(">Set Soil on : ");
      lcd.print(set_dat_on);
      lcd.print("%");
      lcd.print("  ");
    }
    if(c==0) {
      if(save==1) {
        if (up == LOW) {
          if(set_dat_off <= 0 ) {
            set_dat_off = 99;
            delay(100);
          }
          else{
            set_dat_off = set_dat_off - 1;
            delay(100);
          }
        };
        if (doww == LOW) {
          if(set_dat_off >= 99) {
            set_dat_off = 0;
            delay(100);
          }
          else{
            set_dat_off = set_dat_off + 1;
            delay(100);
          }
        };
      }
      lcd.setCursor(0, 0);
      lcd.print("     SET AUTO");
      
      lcd.setCursor(0, 1);
      lcd.print(" Set Humi off: ");
      lcd.print(set_hum_off);
      lcd.print("%");
      lcd.print("  ");
      
      lcd.setCursor(0, 2);
      lcd.print(" Set Soil on : ");
      lcd.print(set_dat_on);
      lcd.print("%");
      lcd.print("  ");

      lcd.setCursor(0, 3);
      lcd.print(">Set Soil off: ");
      lcd.print(set_dat_off);
      lcd.print("%");
      lcd.print("  ");
    }
  }
  // Serial.println(j);
  // Serial.print("save: ");
  // Serial.println(save);
  // Serial.print("a: ");
  // Serial.println(a);
   Serial.println("b: ");
  // Serial.println("độ sáng: ");
  // Serial.print(asss);
}
void loop() {
  server.handleClient();
  dislcd();
}

