#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include "DHT.h"
#include <MQ135.h>
#include <Wire.h>
#include <RTClib.h> // Thư viện RTC để lấy thời gian hiện tại
#include <Adafruit_Fingerprint.h>
#include <ArduinoJson.h>
//----------------------------------------

// Defines SS/SDA PIN and Reset PIN for RFID-RC522.
#define SS_PIN  5  
#define RST_PIN 2

// Số lượng học sinh tối đa for RFID-RC522.
#define MAX_STUDENTS 127 

// Defines pin connection for the fingerprint sensor.
#define RX_PIN 16
#define TX_PIN 17

// ID cho vân tay admin
#define ADMIN_ID 1 

//DHT
#define DHTPIN  4
#define DHTTYPE DHT11

//MQ135
#define PIN_MQ135 35 

// Defines the button PIN.
#define BTN_PIN 15

// defines the buzzer pin
#define BUZZER_PIN 33
#define BUZZER1_PIN 26 

//--------------------------------------------------------------------// KẾT NỐI WI-FI //---------------------------------------------------------------------------------
//.......................................................SSID và MẬT KHẨU của mạng WiFi
const char* ssid = "Truong Thi Ha";  //--> Your wifi name
const char* password = "25101973"; //--> Your wifi password
//const char* ssid = "Nguyen Le";  //--> Your wifi name
//const char* password = "12345678"; //--> Your wifi password
//.......................................................token của bot tu BotFather va id cua nguoi nhan
const char* botToken = "7228680901:AAGkLGZ9PRUXmJ98kpbpPw3bgEh0";
const char* chatID = "1793971";

//---------------------------------------------------------------- // Google script Web_App_URL //--------------------------------------------------------------------
String Web_App_URL = "https://script.google.com/macros/s/AKfycbweE4TTXY9Qr1dcXFMQ9Y9S6FFs6NcxLaKp0zZMuUHZQDLm5wQq_YgjGUvQlBcusLE9Hw/exec";

//--------------------------------------------------------------------// LCD 20X4 //---------------------------------------------------------------------------------
// Biến cho số cột và hàng trên màn hình LCD.
int lcdColumns = 20;
int lcdRows = 4;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  // (lcd_address, lcd_Columns, lcd_Rows)

//--------------------------------------------------------------------// XỬ LÝ RFID //-------------------------------------------------------------------------------

//.............................................KHAI BÁO BIẾN CƠ BẢN...................................
String reg_Info = "";
String atc_Info = "";
String atc_Name = "";
String atc_Date = "";
String atc_Time_In = "";
String atc_Time_Out = "";

// Cấu trúc để lưu thông tin học sinh
struct Student {
  int id;
  char uid[20];
  char name[50];
  char timeIn[20];
  char timeOut[20];
};

// Khai báo biến toàn cục
#define MAX_STUDENTS 100 // Số học sinh tối đa
Student students[MAX_STUDENTS]; // Mảng lưu thông tin học sinh
int studentCount = 0; // Đếm số học sinh
MFRC522 mfrc522(SS_PIN, RST_PIN); 

//........................ Biến để đọc dữ liệu từ RFID-RC522.................

int readsuccess;
char str[32] = "";
String UID_Result = "--------";// Biến lưu UID
String modes = "atc";
 

//...........................................GETVALUE......................................
// Hàm String để xử lý(phân tách chuỗi) dữ liệu nhận về từ gg sheets (Split String).
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


//...............................................getUID()....................................
// Hàm lấy mã rfid 
int getUID() {  
  if(!mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if(!mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }
  
  byteArray_to_string(mfrc522.uid.uidByte, mfrc522.uid.size, str);
  UID_Result = str;
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  return 1;
}

//.............................................byteArray_to_string()..................................
void byteArray_to_string(byte array[], unsigned int len, char buffer[]) {
  for (unsigned int i = 0; i < len; i++) {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len*2] = '\0';
}


//.............................................HANDLEUID()............................................... 
// Khai báo biến toàn cục để kiểm tra trạng thái admin
bool isAdminAccess = false; // Biến để kiểm tra quyền truy cập admin

void handleUID() {
  // Xử lý đọc UID và điều kiện theo chế độ hiện tại
  bool readsuccess = getUID();
  
  if ((modes == "atc" || modes == "atcfp") && readsuccess) {
    if (readsuccess){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Getting UID");
      lcd.setCursor(0,1);
      lcd.print("Successfully");
      lcd.setCursor(0,2);
      lcd.print("UID : ");
      lcd.print(UID_Result);
      lcd.setCursor(0,3);
      lcd.print("Please wait...");
      delay(1000);
      lcd.clear();
      http_Req(modes, UID_Result);
    }
  }
  else if ((modes == "reg" || modes == "regfp") && readsuccess) {
    // Kiểm tra nếu là thẻ admin
    if (UID_Result == "9393AD14") { //  UID của admin 
      isAdminAccess = true; // Cấp quyền admin
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Admin Access");
      delay(1000);
      lcd.clear();
    } 
    else if (isAdminAccess) {
      // Nếu đã có quyền admin, cho phép đăng ký UID mới
      if (readsuccess){
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Getting UID");
        lcd.setCursor(0,1);
        lcd.print("Successfully");
        lcd.setCursor(0,2);
        lcd.print("UID : ");
        lcd.print(UID_Result);
        lcd.setCursor(0,3);
        lcd.print("Please wait...");
        delay(1000);
        lcd.clear();
        http_Req(modes, UID_Result);
      }
    } 
    else {
      // Nếu không phải admin và chưa có quyền, từ chối
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print("Access Denied1");
      delay(1000);
      lcd.clear();
    }
  }
}


//--------------------------------------------------------------------// XỬ LÝ VÂN TAY //-------------------------------------------------------------------------------

//.............................................KHAI BÁO BIẾN CƠ BẢN...........................
// Sử dụng UART2 với chân RX và TX đã chọn
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
String modesfp = "atcfp";// Mode điểm danh vântay
uint8_t id;// Fingerprint ID storage.
bool fingerprintActive = false;  // Biến kiểm tra trạng thái đọc vân tay
// Để lưu trạng thái quét vân tay
unsigned long previousMillis = 0; // Biến để lưu thời gian trước đó

const long interval =120000;  // khoảng thời gian giữa các lần là 2 phút
//..................................Hàm quét dấu vân tay..................................
int getFingerprintIDez() {
  static int step = 0; // Biến trạng thái
  static unsigned long lastTime = 0; // Thời gian cho bước chờ
  uint8_t p = 0; // Biến lưu kết quả

  switch (step) {
    case 0: // Bắt đầu quá trình nhận dạng
      p = finger.getImage();
      if (p == FINGERPRINT_OK) {
        Serial.println("Image taken for recognition");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Image taken for recognition");
        delay(500);
        lcd.clear();
        step = 1; // Chuyển sang bước tiếp theo
      } else if (p == FINGERPRINT_NOFINGER) {
        // Không làm gì, đợi đặt ngón tay lại
      } else {
        //Serial.println("Error capturing image1");
        //lcd.clear();
        //lcd.setCursor(0,0);
        //lcd.print("Error capturing image2");
        //delay(200);
        //lcd.clear();
        step = 0; // Đặt lại step nếu có lỗi
      }
      break;

    case 1: // Chuyển đổi hình ảnh thành mẫu
      p = finger.image2Tz();
      if (p == FINGERPRINT_OK) {
        Serial.println("Template created for recognition");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Template created for recognition");
        delay(500);
        lcd.clear();
        step = 2; // Chuyển sang bước tiếp theo
      } else {
        Serial.println("Error creating template");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Error creating template");
        delay(500);
        lcd.clear();
        step = 0; // Đặt lại step nếu có lỗi
      }
      break;

    case 2: // Tìm kiếm vân tay
      p = finger.fingerFastSearch();
      if (p == FINGERPRINT_OK) {
        // Tìm thấy một kết quả phù hợp
        Serial.print("Found ID #"); 
        Serial.print(finger.fingerID);
        Serial.print(" with confidence of "); 
        Serial.println(finger.confidence);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Remove finger");
        delay(500);
        lcd.clear();
        return finger.fingerID; // Trả về ID tìm thấy
        step = 0; // Hoàn tất quá trình

      } else {
        Serial.println("No match found");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("No match found");
        delay(500);
        lcd.clear();
        step = 0; // Hoàn tất quá trình
      }
      break;
  }

  return -1; // Trả về -1 nếu không có kết quả
}


//..................................Hàm đăng ký dấu vân tay mới..................................
uint8_t getFingerprintEnroll() {
  static int step = 0; // Biến trạng thái
  static unsigned long lastTime = 0; // Thời gian cho bước chờ
  static int id = 1; // Bắt đầu từ ID = 1 để đảm bảo ID 1 là admin
  int p = -1;

  // Bước xác thực admin trước khi đăng ký
  if (step == 0) {
    //Serial.println("Place admin finger to authenticate");

    p = finger.getImage();
    if (p == FINGERPRINT_OK) {
      p = finger.image2Tz(1);
      if (p == FINGERPRINT_OK) {
        p = finger.fingerSearch(); // Kiểm tra vân tay với database
        if (p == FINGERPRINT_OK && finger.fingerID == 1) { 
          Serial.println("Admin authenticated");
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Admin authenticated");
          delay(1000);
          lcd.clear();
          step = 1; // Chuyển sang bước tiếp theo để đăng ký vân tay mới
        } else {
          Serial.println("Authentication failed: Not admin");
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Auth failed!");
          lcd.setCursor(0,1);
          lcd.print("Not admin");
          delay(2000);
          lcd.clear();
          return 0; // Kết thúc nếu không xác thực được admin
        }
      } else {
        Serial.println("Error creating template");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Error creating template");
        delay(500);
        lcd.clear();
        return 0; // Kết thúc nếu có lỗi
      }
    } else if (p == FINGERPRINT_NOFINGER) {
      // Đợi ngón tay đặt lại nếu không có vân tay
      return 0;
    } else {
      Serial.println("Error capturing admin image");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Error capturing image3");
      delay(500);
      lcd.clear();
      return 0; // Kết thúc nếu có lỗi
    }
  }

  // Các bước tiếp theo của quy trình đăng ký vân tay
  switch (step) {
    case 1: // Tìm ID chưa sử dụng
      if (id == 1 && finger.loadModel(id) == FINGERPRINT_OK) {
        id++; // Bỏ qua nếu ID 1 đã được lưu (Admin đã tồn tại)
      }
      while (finger.loadModel(id) == FINGERPRINT_OK) {
        id++;
      }
      Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
      step = 2; // Chuyển sang bước tiếp theo
      break;

    case 2: // Chụp ảnh vân tay đầu tiên
      p = finger.getImage();
      if (p == FINGERPRINT_OK) {
        Serial.println("Image taken");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Image taken");
        delay(500);
        lcd.clear();
        step = 3; // Chuyển sang bước tiếp theo
      } else if (p == FINGERPRINT_NOFINGER) {
        // Không làm gì, đợi đặt ngón tay lại
      } else {
        Serial.println("Error capturing image4");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Error capturing image5");
        delay(500);
        lcd.clear();        
        step = 1; // Quay lại bước xác thực admin nếu có lỗi
      }
      break;

    case 3: // Chuyển đổi hình ảnh thành mẫu (Template 1)
      p = finger.image2Tz(1);
      if (p == FINGERPRINT_OK) {
        Serial.println("Remove finger");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Remove finger");
        delay(500);
        lcd.clear();
        step = 4; // Chuyển sang bước tiếp theo
        lastTime = millis(); // Đặt thời gian chờ cho bước tiếp theo
      } else {
        Serial.println("Error creating template");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Error creating template");
        delay(500);
        lcd.clear();
        step = 1; // Quay lại bước xác thực admin nếu có lỗi
      }
      break;

    case 4: // Chờ rút ngón tay
      if (millis() - lastTime >= 2000 && finger.getImage() == FINGERPRINT_NOFINGER) {
        Serial.println("Place same finger again");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Place same finger again");
        delay(500);
        lcd.clear();        
        step = 5; // Chuyển sang bước tiếp theo
      }
      break;

    case 5: // Chụp ảnh vân tay lần thứ hai
      p = finger.getImage();
      if (p == FINGERPRINT_OK) {
        Serial.println("Image taken again");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Image taken again");
        delay(200);
        lcd.clear();
        step = 6; // Chuyển sang bước tiếp theo
      } else if (p == FINGERPRINT_NOFINGER) {
        // Không làm gì, đợi đặt ngón tay lại
      } else {
        Serial.println("Error capturing second image");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Error capturing second image");
        delay(200);
        lcd.clear();
        step = 1; // Quay lại bước xác thực admin nếu có lỗi
      }
      break;

    case 6: // Chuyển đổi hình ảnh thành mẫu (Template 2) và lưu  
      p = finger.image2Tz(2);
      if (p == FINGERPRINT_OK) {
        p = finger.createModel();
        if (p == FINGERPRINT_OK) {
          p = finger.storeModel(id); // Lưu vào ID hiện tại
          if (p == FINGERPRINT_OK) {
            Serial.println("Stored!");
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Stored!");
            lcd.print(id);
            lcd.clear();

            http_Req("regfp", String(id)); // Gửi ID đã đăng ký lên Google Sheets

            delay(500);
            lcd.setCursor(0,0);
            lcd.print("Getting ID");
            lcd.setCursor(0,1);
            lcd.print("Successfully");
            lcd.setCursor(0,2);
            lcd.print("ID: ");
            lcd.print(id);
            lcd.setCursor(0,3);
            lcd.print("Please wait...");
            delay(1000);
            lcd.clear(); 

            id++; // Tăng ID sau khi lưu thành công
            step = 0; // Đặt lại step để bắt đầu lại quy trình từ đầu
            return FINGERPRINT_OK; // Trả về thành công
          } else {
            Serial.println("Error storing model");
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Error storing model");
            delay(500);
            lcd.clear();
            step = 1; // Quay lại bước xác thực admin nếu có lỗi
          }
        }
      } else {
        Serial.println("Error creating second template");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Error creating second template");
        delay(500);
        lcd.clear();
        step = 1; // Quay lại bước xác thực admin nếu có lỗi
      }
      break;
  }
  return 0; // Trả về không thành công
}

//.............................................HANDLEUID_Fingerprint()............................................... 
// Khai báo biến toàn cục để kiểm tra trạng thái admin
//bool isAdminAccessFp = false; // Biến để kiểm tra quyền truy cập admin cho vân tay

void handleUIDFp() {
  int id = -1;
  if (modesfp == "atcfp" || modesfp == "regfp") {
    if (modesfp == "atcfp") {
      id = getFingerprintIDez(); // Gọi hàm lấy ID vân tay trong chế độ điểm danh
      if (id >= 0) { // Nếu quét thành công
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Getting ID");
        lcd.setCursor(0,1);
        lcd.print("Successfully");
        lcd.setCursor(0,2);
        lcd.print("ID : ");
        lcd.print(id);
        lcd.setCursor(0,3);
        lcd.print("Please wait...");
        delay(1000);
        lcd.clear();
        http_Req(modesfp, String(id)); // Gửi ID lên HTTP request
      }
    }
    else if (modesfp == "regfp") {
      id = getFingerprintEnroll(); // Gọi hàm đăng ký vân tay trong chế độ đăng ký
      if (id > 0) { // Nếu đăng ký thành công
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Getting ID");
        lcd.setCursor(0,1);
        lcd.print("Successfully");
        lcd.setCursor(0,2);
        lcd.print("ID : ");
        lcd.print(id);
        lcd.setCursor(0,3);
        lcd.print("Please wait...");
        delay(1000);
        lcd.clear();
        http_Req(modesfp, String(id)); // Gửi ID lên HTTP request
      }
    }
  }
}

//--------------------------------------------------------------------// XỬ LÝ NÚT NHẤN CHUYỂN CHẾ ĐỘ //------------------------------------------------------------------ 
void handleButtons() {
  // Xử lý các nút bấm và thay đổi chế độ
  int BTN_State = digitalRead(BTN_PIN);
  if (BTN_State == LOW) {
    lcd.clear();
    // Chuyển đổi giữa các chế độ
    if (modes == "atc") {
      modes = "reg";
      modesfp = "regfp";
    } else if (modes == "reg") {
      modes = "atc";
      modesfp = "atcfp";
    }
    delay(500);
  }
}

//--------------------------------------------------------------------// KHAI BÁO BIẾN DHT11 VÀ MQ135 //-----------------------------------------------------------------------------------------
String Status_Read_Sensor = "";
float Temp;
int Humd;
float smoke_ppm;
float co2_ppm;
float benzen_ppm;

// Khai báo các biến và mảng lưu dữ liệu
float tempArray[10], humdArray[10], co2Array[10], smokeArray[10], benzenArray[10];
int arrayIndex = 0;
int count = 0;
float avgTemp = 0, avgHumd = 0, avgCo2 = 0, avgSmoke = 0, avgBenzen = 0;
unsigned long lastMeasureTime = 0;
const long measureInterval = 1000;  // Đo mỗi 1 giây
unsigned long lastSendTime = 0;
const long sendInterval = 60000; // Gửi sau mỗi phút (sau 6 lần trung bình)

//...................................Ngưỡng để kích hoạt còi cảnh báo........................................



//...............create DHT object as dht11 and set pin data and type and xứ lý thời gian gửi dữ liệu lên gg sheets là 10s(dht11 or dht22...).........................
DHT dht11(DHTPIN, DHTTYPE);

//...............................MQ135.......................................

const float R0 = 10.0; // Giá trị R0 (điện trở của MQ135 trong không khí sạch)
MQ135 mq135_sensor(PIN_MQ135, R0);

//---------------------------------------------------// Đọc tín hiệu digital cảm biến dht11  //---------------------------------------------------

void Getting_DHT11_Sensor_Data() {
  // Đọc nhiệt độ hoặc độ ẩm mất khoảng 250 mili giây!
 // Đọc cảm biến cũng có thể chậm hơn tới 2 giây (cảm biến rất chậm)
  Humd = dht11.readHumidity();
  // Đọc nhiệt độ theo độ C (mặc định)
  Temp = dht11.readTemperature();

  // Kiểm tra xem đọc có bị lỗi không và để thử lại
  if (isnan(Humd) || isnan(Temp)) {
    Serial.println();
    Serial.println(F("Failed to read from DHT sensor!"));
    Serial.println();

    Status_Read_Sensor = "Failed";
    Temp = 0.00;
    Humd = 0;
  } else {
    Status_Read_Sensor = "Success";
  }
}

//---------------------------------------------------// Đọc tín hiệu analog cảm biến mq135 //---------------------------------------------------
void Getting_MQ135_Sensor_Data(){
  co2_ppm = mq135_sensor.getCorrectedPPM(Temp, Humd);
  // Tính toán nồng độ khói và benzen dựa trên nồng độ CO2
  smoke_ppm = co2_ppm * 0.1;        // Giá trị giả định cho khói
  benzen_ppm = co2_ppm * 0.05;      // Giá trị giả định cho benzen khi ngoai trời khoảng 10ppm trở lên
  if (isnan(co2_ppm) || isnan(smoke_ppm) || isnan(benzen_ppm)) {
    Serial.println();
    Serial.println(F("Failed to read from MQ135 sensor!"));
    Serial.println();

    co2_ppm = 0.00;
    smoke_ppm = 0.00;
    benzen_ppm = 0.00;
  } else {
    Status_Read_Sensor = "Success";
  }
}


//-----------------------------------------------------------------// XỬ LÝ THỜI GIAN THỰC //-----------------------------------------------------------------
RTC_DS3231 rtc; // Đối tượng RTC
//char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
bool isWithinTime(int startHour, int startMinute, int endHour, int endMinute) {
    DateTime now = rtc.now();  // Lấy thời gian hiện tại từ RTC
    // Kiểm tra nếu thời gian hiện tại nằm trong khoảng từ startHour:startMinute đến endHour:endMinute
    if ((now.hour() > startHour || (now.hour() == startHour && now.minute() >= startMinute)) &&
        (now.hour() < endHour || (now.hour() == endHour && now.minute() <= endMinute))) {
        return true;
    }
    return false;
}
//đoạn này test thời gian thực trong vòngg loop
//DateTime now = rtc.now();
//Serial.print(now.year(), DEC);
//Serial.print('/');
//Serial.print(now.month(), DEC);
//Serial.print('/');
//Serial.print(now.day(), DEC);
//Serial.print(" (");
//Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
//Serial.print(") ");
//Serial.print(now.hour(), DEC);
//Serial.print(':');
//Serial.print(now.minute(), DEC);
//Serial.print(':');
//Serial.print(now.second(), DEC);
//Serial.println();

//--------------------------------------------------------------------// XỬ LÝ HTTP_Req() chế độ viết(gửi) dữ liệu ở các chế độ //----------------------------------------------------------------------
void http_Req(String str_modes, String str_uid) {
  if (WiFi.status() == WL_CONNECTED) {
    String http_req_url = "";

    //.................................................Tạo liên kết để thực hiện yêu cầu HTTP tới Google Trang tính...............
    if (str_modes == "atc") {
      http_req_url  = Web_App_URL + "?sts=atc";
      http_req_url += "&uid=" + str_uid;
    }
    if (str_modes == "reg") {
      http_req_url = Web_App_URL + "?sts=reg";
      http_req_url += "&uid=" + str_uid;
    }
    if (str_modes == "atcfp") {
      http_req_url  = Web_App_URL + "?sts=atcfp";
      http_req_url += "&id=" + str_uid;
    }
    if (str_modes == "regfp") {
      http_req_url = Web_App_URL + "?sts=regfp";
      http_req_url += "&id=" + str_uid;
    }

    //.................................................Gửi yêu cầu HTTP đến Google Trang tính....................
    Serial.println();
    Serial.println("-------------");
    Serial.println("Sending request to Google Sheets...");
    Serial.print("URL : ");
    Serial.println(http_req_url);
    
    //................................................ Tạo  đối tượng HTTPClient là "http"..................
    HTTPClient http;

    //.................................................. Yêu cầu HTTP GET......................................
    http.begin(http_req_url.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    // ..............................................Nhận mã trạng thái HTTP...................................
    int httpCode = http.GET(); 
    Serial.print("HTTP Status Code : ");
    Serial.println(httpCode);

    //.............................................. Nhận phản hồi từ trang tính Google.............................
    String payload;
    if (httpCode > 0) {
      payload = http.getString();
      Serial.println("Payload : " + payload);  
    }
    
    Serial.println("-------------");
    http.end();
    //..............................................................................................................................................................................
    
    String sts_Res = getValue(payload, ',', 0);

    //...........................Các điều kiện được thực hiện dựa trên phản hồi từ Google Trang tính  trong Google Apps Script.
    if (sts_Res == "OK") {
      //.................................................................................................................................................
      if (str_modes == "atc" || str_modes == "atcfp") {
        atc_Info = getValue(payload, ',', 1);
        
        if (atc_Info == "TI_Successful") {
          atc_Name = getValue(payload, ',', 2);
          atc_Date = getValue(payload, ',', 3);
          atc_Time_In = getValue(payload, ',', 4);

          //::::::::::::::::::Tạo giá trị vị trí để hiển thị "Tên" trên màn hình LCD sao cho nó được căn giữa.........................
          int name_Lenght = atc_Name.length();
          int pos = 0;
          if (name_Lenght > 0 && name_Lenght <= lcdColumns) {
            pos = map(name_Lenght, 1, lcdColumns, 0, (lcdColumns / 2) - 1);
            pos = ((lcdColumns / 2) - 1) - pos;
          } else if (name_Lenght > lcdColumns) {
            atc_Name = atc_Name.substring(0, lcdColumns);
          }
          delay(500);
          lcd.setCursor(pos,0);
          lcd.print(atc_Name);
          lcd.setCursor(0,1);
          lcd.print("Date    : ");
          lcd.print(atc_Date);
          lcd.setCursor(0,2);
          lcd.print("Time IN :   ");
          lcd.print(atc_Time_In);
          lcd.setCursor(0,3);
          lcd.print("Time Out:   ");
          delay(5000);
          lcd.clear();
          delay(500);
        }

        if (atc_Info == "TO_Successful") {
          atc_Name = getValue(payload, ',', 2);
          atc_Date = getValue(payload, ',', 3);
          atc_Time_In = getValue(payload, ',', 4);
          atc_Time_Out = getValue(payload, ',', 5);

          //::::::::::::::::::Tạo giá trị vị trí để hiển thị "Tên" trên màn hình LCD sao cho nó được căn giữa..........................
          int name_Lenght = atc_Name.length();
          int pos = 0;
          if (name_Lenght > 0 && name_Lenght <= lcdColumns) {
            pos = map(name_Lenght, 1, lcdColumns, 0, (lcdColumns / 2) - 1);
            pos = ((lcdColumns / 2) - 1) - pos;
          } else if (name_Lenght > lcdColumns) {
            atc_Name = atc_Name.substring(0, lcdColumns);
          }
          //::::::::::::::::::

          lcd.clear();
          delay(500);
          lcd.setCursor(pos,0);
          lcd.print(atc_Name);
          lcd.setCursor(0,1);
          lcd.print("Date    : ");
          lcd.print(atc_Date);
          lcd.setCursor(0,2);
          lcd.print("Time IN :   ");
          lcd.print(atc_Time_In);
          lcd.setCursor(0,3);
          lcd.print("Time Out:   ");
          lcd.print(atc_Time_Out);
          delay(5000);
          lcd.clear();
        }

        if (atc_Info == "atcInf02") {
          lcd.clear();
          delay(500);
          lcd.setCursor(1,0);
          lcd.print("You have completed");
          lcd.setCursor(2,1);
          lcd.print("your attendance");
          lcd.setCursor(2,2);
          lcd.print("for today");
          delay(2000);
          lcd.clear();
        }

        if (atc_Info == "atcInf01") {
          lcd.clear();
          delay(500);
          lcd.setCursor(1,0);
          lcd.print("You have completed");
          lcd.setCursor(2,1);
          lcd.print("your attendance");
          lcd.setCursor(2,2);
          lcd.print("in at this");
          lcd.setCursor(2,3);
          lcd.print("time slot");
          delay(2000);
          lcd.clear();
        }

        if (atc_Info == "atcErr01") {
          lcd.clear();
          delay(500);
          lcd.setCursor(6,0);
          lcd.print("Error !");
          lcd.setCursor(4,1);
          lcd.print("Your card or");
          lcd.setCursor(4,2);
          lcd.print("key chain is");
          lcd.setCursor(3,3);
          lcd.print("not registered");
          delay(2000);
          lcd.clear();
        }

        atc_Info = "";
        atc_Name = "";
        atc_Date = "";
        atc_Time_In = "";
        atc_Time_Out = "";
      }
      //.........................................................................................................................................................................

      //..................................................................................................................................................................
      if (str_modes == "reg" || str_modes == "regfp") {
        reg_Info = getValue(payload, ',', 1);
        if (reg_Info == "R_Successful") {
          lcd.clear();
          delay(500);
          lcd.setCursor(2,0);
          lcd.print("The UID of your");
          lcd.setCursor(0,1);
          lcd.print("card or keychain has");
          lcd.setCursor(1,2);
          lcd.print("been successfully");
          lcd.setCursor(6,3);
          lcd.print("uploaded");
          delay(5000);
          lcd.clear();
          delay(500);
        }

        if (reg_Info == "regErr01") {
          lcd.clear();
          delay(500);
          lcd.setCursor(6,0);
          lcd.print("Error !");
          lcd.setCursor(0,1);
          lcd.print("The UID of your card");
          lcd.setCursor(0,2);
          lcd.print("or keychain has been");
          lcd.setCursor(5,3);
          lcd.print("registered");
          delay(5000);
          lcd.clear();
          delay(500);
        }

        reg_Info = "";
      }
      //..................................................................................................................................................................
      digitalWrite(BUZZER_PIN, HIGH); //bật còi trong
      delay(1000);
      digitalWrite(BUZZER_PIN, LOW);//tắt còi trong
    }
    //----------------------------------------
  } else {
    lcd.clear();
    delay(500);
    lcd.setCursor(6,0);
    lcd.print("Error !");
    lcd.setCursor(1,1);
    lcd.print("WiFi disconnected");
    delay(3000);
    lcd.clear();
    delay(500);
  }
}

// Hàm để in thông tin học sinh ra Serial Monitor
void printStudentData() {
  Serial.println("Thông tin học sinh:");
  Serial.println("ID - UID - Name - Time In - Time Out");
  
  // Duyệt qua mảng học sinh và in thông tin
  for (int i = 0; i < studentCount; i++) {
    Serial.print("ID: ");
    Serial.print(students[i].id);
    Serial.print(", UID: ");
    Serial.print(students[i].uid);
    Serial.print(", Name: ");
    Serial.print(students[i].name);
    Serial.print(", Time In: ");
    Serial.print(students[i].timeIn);
    Serial.print(", Time Out: ");
    Serial.println(students[i].timeOut); // Thời gian ra (có thể rỗng)
  }
  Serial.println("-------------");
}
//--------------------------------------------------------------------// Đọc dữ liệu từ google sheets về để cảnh báo() //---------------------------------------------------------------------------
bool readDataSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    // Tạo URL để đọc dữ liệu từ Google Sheets
    String Read_Data_URL = Web_App_URL + "?sts=read";
    Serial.println();
    Serial.println("-------------");
    Serial.println("Đọc dữ liệu từ Google Sheets...");
    Serial.print("URL : ");
    Serial.println(Read_Data_URL);

    HTTPClient http;
    http.begin(Read_Data_URL.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET(); 
    Serial.print("Mã trạng thái HTTP : ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Nội dung nhận được : " + payload); 

      // Kích thước bộ nhớ JSON cho thư viện ArduinoJson
      const size_t capacity = JSON_ARRAY_SIZE(10) + JSON_OBJECT_SIZE(5) * 10;
      DynamicJsonDocument doc(capacity);

      // Phân tích chuỗi JSON
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("Lỗi phân tích JSON: ");
        Serial.println(error.c_str());
        return false;
      }

      studentCount = 0;
      for (JsonArray row : doc.as<JsonArray>()) {
        if (row.size() >= 5) {
          String nameStr = row[0].as<String>();
          String uidStr = row[1].as<String>();
          String dateStr = row[2].as<String>();
          String timeInStr = row[3].as<String>();
          String timeOutStr = row[4].as<String>();

          students[studentCount].id = studentCount + 1;
          strcpy(students[studentCount].uid, uidStr.c_str());
          strcpy(students[studentCount].name, nameStr.c_str());
          strcpy(students[studentCount].timeIn, timeInStr.c_str());
          strcpy(students[studentCount].timeOut, timeOutStr.c_str());

          studentCount++;
          if (studentCount >= MAX_STUDENTS) break;
        }
      }

      // In thông tin học sinh ra Serial
      printStudentData();
    }

    http.end(); // Kết thúc HTTP
    Serial.println("-------------");
    return studentCount > 0; // Trả về true nếu có học sinh
  }
  return false;
}


//--------------------------------------------------------------------// Hàm gửi dữ liệu lên google shees của các cảm biến //-----------------------------------------------
void sendDataToGoogleSheets() {
  // Gọi hàm gửi dữ liệu lên Google Sheets
  //Getting_DHT11_Sensor_Data();
  //Read_Switches_State();
  //Getting_MQ135_Sensor_Data();

  if (WiFi.status() == WL_CONNECTED) {
    //digitalWrite(On_Board_LED_PIN, HIGH);
    String Send_Data_URL = Web_App_URL + "?dht=write";
    Send_Data_URL += "&srs=" + Status_Read_Sensor;
    Send_Data_URL += "&temp=" + String(avgTemp);
    Send_Data_URL += "&humd=" + String(avgHumd);
    Send_Data_URL += "&co2_ppm=" + String(avgCo2);
    Send_Data_URL += "&smoke_ppm=" + String(avgSmoke);
    Send_Data_URL += "&benzen_ppm=" + String(avgBenzen);

    HTTPClient http;
    http.begin(Send_Data_URL.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    Serial.print("HTTP Status Code : ");
    Serial.println(httpCode);
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Payload : " + payload);
    }
    http.end();
    //digitalWrite(On_Board_LED_PIN, LOW);
    Serial.println("-------------");
  }
}

//--------------------------------------------------------------------// hàm gửi tin nhắn đến telegram bot//---------------------------------------------------
String urlEncode(String str) {
  String encoded = "";
  char c;
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      encoded += "%" + String(c, HEX);
    }
  }
  return encoded;
}
void sendMessage(const String& message) {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage?chat_id=" + chatID + "&text=" + urlEncode(message);
  http.begin(url);
  int httpCode = http.GET();  // Gửi yêu cầu GET đến Telegram API

  if (httpCode > 0) {
    Serial.println("Message sent");
  } else {
    Serial.print("Error on sending message, HTTP code: ");
    Serial.println(httpCode);
  }
  http.end();
}

//-----------------------------------------------------------------VOID SETUP()-----------------------------------------------------------------
void setup(){

  Serial.begin(115200);
  Wire.begin();  // Khởi tạo I2C
  Serial.println();
  delay(1000);

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT); // khởi tạo chân cho còi
  digitalWrite(BUZZER_PIN, LOW);//tắt còi khi khởi động

  pinMode(BUZZER1_PIN, OUTPUT); // khởi tạo chân cho còi
  digitalWrite(BUZZER1_PIN, LOW);//tắt còi khi khởi động

  // Initialize LCD.
  lcd.init();
  // turn on LCD backlight.
  lcd.backlight();
  
  lcd.clear();

  delay(500);

  // Init SPI bus.
  SPI.begin();      
  // Init MFRC522.
  mfrc522.PCD_Init(); 
  //init dht11 và không có mq135 là vì mq135 sử dụng tín hiệu alalog nên không cần
  dht11.begin();
  // khởi tạo cảm biến vân tay
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
  if (finger.verifyPassword()) {
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint Ready");
    delay(500);
    Serial.println("Fingerprint Ready");
    lcd.clear();
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error");
  }

  if (!rtc.begin()) {
    Serial.println("Không tìm thấy RTC!");
    while (1); // Dừng chương trình nếu không tìm thấy RTC
  }

  if (rtc.lostPower()) {
    Serial.println("RTC mất nguồn, cài đặt lại thời gian!");
    // Cài đặt lại thời gian nếu RTC bị mất nguồn
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  delay(500);

  lcd.setCursor(5,0);
  lcd.print("ESP32 RFID");
  lcd.setCursor(3,1);
  lcd.print("Google  Sheets");
  lcd.setCursor(1,2);
  lcd.print("Attendance  System");
  lcd.setCursor(4,3);
  lcd.print("by  CTA");
  delay(3000);
  lcd.clear();

  //.............................................Set Wifi to STA mode
  Serial.println();
  Serial.println("-------------");
  Serial.println("WIFI mode : STA");
  WiFi.mode(WIFI_STA);
  Serial.println("-------------"); 

  //...........................................Connect to Wi-Fi (STA).
  Serial.println();
  Serial.println("------------");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int connecting_process_timed_out = 20; //--> 20 = 20 seconds.
  connecting_process_timed_out = connecting_process_timed_out * 2;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");

    lcd.setCursor(0,0);
    lcd.print("Connecting to SSID");
    delay(250);

    lcd.clear();
    delay(250);
    
    if (connecting_process_timed_out > 0) connecting_process_timed_out--;
    if (connecting_process_timed_out == 0) {
      delay(1000);
      ESP.restart();
    }
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("------------");

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi connected");
  delay(2000);
  
  //.........................................................

  lcd.clear();
  delay(500);
}

//-----------------------------------------------------------------VOID LOOP()-----------------------------------------------------------------
void loop() {
  unsigned long currentMillis = millis();// Lấy thời gian hiện tại
  unsigned long currentMillis1 = millis();// Lấy thời gian hiện tại
  unsigned long currentMillis2 = millis();// Lấy thời gian hiện tại

  //................. Gọi hàm xử lý các công việc liên tục
  handleButtons();

  // Kiểm tra thời gian để quét RFID là 10ms
  handleUID();  // Gọi hàm xử lý đọc UID RFID
  handleUIDFp(); // Gọi hàm xử lý đọc UID vân tay

  //...........................................Đo dữ liệu mỗi 1 giây
  if (currentMillis - lastMeasureTime >= measureInterval) {
    lastMeasureTime = currentMillis;
    
    // Đọc giá trị từ cảm biến
    Getting_DHT11_Sensor_Data();
    Getting_MQ135_Sensor_Data();
    
    // Lưu dữ liệu vào mảng
    tempArray[arrayIndex] = Temp;
    humdArray[arrayIndex] = Humd;
    co2Array[arrayIndex] = co2_ppm;
    smokeArray[arrayIndex] = smoke_ppm;
    benzenArray[arrayIndex] = benzen_ppm;

    arrayIndex++;

    // Sau 10 lần đo thì tính trung bình
    if (arrayIndex >= 10) {
      arrayIndex = 0;
      count++;
      avgTemp = 0;
      avgHumd = 0;  
      avgCo2 = 0;
      avgSmoke = 0;
      avgBenzen = 0;

      // Tính trung bình
      for (int i = 0; i < 10; i++) {
        avgTemp += tempArray[i];
        avgHumd += humdArray[i];
        avgCo2 += co2Array[i];
        avgSmoke += smokeArray[i];
        avgBenzen += benzenArray[i];
      }
      avgTemp /= 10;
      avgHumd /= 10;
      avgCo2 /= 10;
      avgSmoke /= 10;
      avgBenzen /= 10;

      // So sánh với ngưỡng để kích hoạt còi cảnh báo và tin nhắn telegram bot
      if (avgTemp > 40 || avgHumd > 80 || avgCo2 > 300 || avgSmoke > 160 || avgBenzen > 80) {
        // Tạo chuỗi chứa giá trị trung bình cảm biến và gửi qua Telegram
        if (avgTemp > 40) {
          String message1 = "Cảnh báo vượt ngưỡng:";
          message1 += "Nhiệt độ: " + String(avgTemp) + "C";
          sendMessage(message1); // Gửi tin nhắn
        }
        delay(2000);
        if (avgHumd > 80) {
          String message1 = "Cảnh báo vượt ngưỡng:";
          message1 += "Độ ẩm: " + String(avgHumd) + "%";
          sendMessage(message1); // Gửi tin nhắn
        }
        delay(2000);
        if (avgCo2 > 300) {
          String message1 = "Cảnh báo vượt ngưỡng:";
          message1 += "CO2: " + String(avgCo2) + " ppm" ;
          sendMessage(message1); // Gửi tin nhắn
        }
        delay(2000);
        if (avgSmoke > 160) {
          String message1 = "Cảnh báo vượt ngưỡng:";
          message1 += "Khói: " + String(avgSmoke) + " ppm";
          sendMessage(message1); // Gửi tin nhắn
        }
        delay(2000);
        if (avgBenzen > 80) {
          String message1 = "Cảnh báo vượt ngưỡng:";
          message1 += "Benzen: " + String(avgBenzen) + " ppm";
          sendMessage(message1); // Gửi tin nhắn
        }

        digitalWrite(BUZZER_PIN, HIGH); // Bật còi cảnh báo trong
        digitalWrite(BUZZER1_PIN, HIGH);// Bật còi cảnh báo ngoài
        delay(10000); // Bật còi trong 10 giây
        digitalWrite(BUZZER_PIN, LOW); // Tắt còi
        digitalWrite(BUZZER1_PIN, LOW); // Tắt còi
      }

      //........................................................DHT11
      Serial.println();
      Serial.println("-------------");
      Serial.print(F("Status_Read_Sensor : "));
      Serial.print(Status_Read_Sensor);
      Serial.print(F(" | Humidity : "));
      Serial.print(avgHumd);
      Serial.print(F("% | Temperature : "));
      Serial.print(avgTemp);
      Serial.println(F("°C"));
      Serial.println("-------------");

      //.........................................................MQ135
      Serial.println();
      Serial.println("-------------");
      Serial.print(F("Status_Read_Sensor : "));
      Serial.print(Status_Read_Sensor);

      Serial.print(F(" | co2_ppm : "));
      Serial.print(co2_ppm);
      Serial.println(F("ppm"));

      Serial.print(F(" | smoke_ppm : "));
      Serial.print(smoke_ppm);
      Serial.println(F("ppm"));

      Serial.print(F(" | benzen_ppm : "));
      Serial.print(benzen_ppm);
      Serial.println(F("ppm"));

      Serial.println("-------------");

    }
  }

  //...........................................Gửi dữ liệu lên Google Sheets mỗi 1 phút (sau 6 lần tính trung bình)
  if (currentMillis1 - lastSendTime >= sendInterval && count >= 6) {
    lastSendTime = currentMillis1;
    count = 0;
    sendDataToGoogleSheets();
  } 

  // Kiểm tra thời gian cảnh báo
  if (isWithinTime(7, 30, 7, 45) || isWithinTime(14, 30, 14, 45) || isWithinTime(17, 30, 18, 00)) {
    // moi 2 phut thi doc du lieu 1 lan
    if (currentMillis2 - previousMillis >= interval) {
      previousMillis = currentMillis2; // Cập nhật thời gian trước đó
      readDataSheet(); // Đọc dữ liệu từ Google Sheets
      if (studentCount > 0) {
        // Hiển thị danh sách học sinh còn lại trên LCD
        lcd.setCursor(0, 0);
        lcd.print("Học sinh còn lại:");
        delay(1000); // Hiển thị trong 1 giây trước khi thay đổi
        lcd.clear();
        // In ra danh sách học sinh còn lại
        for (int i = 0; i < studentCount; ++i) {
          lcd.clear(); // Xóa LCD trước khi hiển thị học sinh tiếp theo
          lcd.setCursor(0, 0);
          lcd.print("ID: ");
          lcd.println(students[i].id);
          lcd.setCursor(0, 1);
          lcd.print("Name: ");
          lcd.println(students[i].name);
          lcd.setCursor(0, 2);
          lcd.print("Time In: ");
          lcd.println(students[i].timeIn);
          lcd.setCursor(0, 3);
          lcd.print("Time Out: ");
          lcd.println(students[i].timeOut);
          delay(3000);
          lcd.clear(); // Xóa LCD trước khi hiển thị học sinh tiếp theo

          String message = "Thông tin học sinh:\n";
          message += "ID: " + String(students[i].id) + "\n";
          message += "UID: " + String(students[i].uid) + "\n";
          message += "Tên: " + String(students[i].name) + "\n";
          message += "Thời gian vào: " + String(students[i].timeIn) + "\n";
          message += "Thời gian ra: " + String(students[i].timeOut) + "\n";
          message += "-----------------------\n";
          // Gửi tin nhắn
          sendMessage(message);
          delay(1000);
        }

        digitalWrite(BUZZER_PIN, HIGH); // Bật còi cảnh báo trong
        digitalWrite(BUZZER1_PIN, HIGH);// Bật còi cảnh báo ngoài
        delay(10000); // Bật còi trong 10 giây
        digitalWrite(BUZZER_PIN, LOW); // Tắt còi
        digitalWrite(BUZZER1_PIN, LOW); // Tắt còi
      }
    }
  }
  
  //..........................................hiển thị chế độ mode và thông tin cảm biến
  if (modes == "atc") {
    lcd.setCursor(5,0);
    lcd.print("ATTENDANCE");
    lcd.setCursor(0,1);
    lcd.print("T:");
    lcd.print(avgTemp);
    lcd.setCursor(7,1);
    lcd.print("C");
    lcd.setCursor(9,1);
    lcd.print(",H:");
    lcd.print(avgHumd);
    lcd.setCursor(18,1);
    lcd.print("%");
    lcd.setCursor(0,3);
    lcd.print("Co2:");
    lcd.print(avgCo2);
    lcd.setCursor(9,3);
    lcd.print("ppm");
    lcd.setCursor(0,2);
    lcd.print("Sm:");
    lcd.print(avgSmoke);
    lcd.setCursor(8,2);
    lcd.print(",Bz:");
    lcd.print(avgBenzen);
    lcd.setCursor(17,2);
    lcd.print("ppm");
    delay(2000);
  }
  
  if (modes == "reg"){
    lcd.setCursor(4,0);
    lcd.print("REGISTRATION");
    lcd.setCursor(0,1);
    lcd.print("T:");
    lcd.print(avgTemp);
    lcd.setCursor(7,1);
    lcd.print("C");
    lcd.setCursor(9,1);
    lcd.print(",H:");
    lcd.print(avgHumd);
    lcd.setCursor(18,1);
    lcd.print("%");
    lcd.setCursor(0,3);
    lcd.print("Co2:");
    lcd.print(avgCo2);
    lcd.setCursor(9,3);
    lcd.print("ppm");
    lcd.setCursor(0,2);
    lcd.print("Sm:");
    lcd.print(avgSmoke);
    lcd.setCursor(8,2);
    lcd.print(",Bz:");
    lcd.print(avgBenzen);
    lcd.setCursor(17,2);
    lcd.print("ppm");
    delay(2000);
  }
  delay(500); // Đợi một thời gian ngắn để tránh quét lại cùng một thẻ quá nhanh
}
