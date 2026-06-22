#include <SPI.h>
#include <MFRC522.h>

//Definicion de pines
#define RST  9
#define SS   10
#define SCK  12
#define MISO 13
#define MOSI 11

MFRC522 mfrc522(SS, RST);

void setup() 
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== TEST RFID ===");

  SPI.begin(SCK, MISO, MOSI, SS);

  mfrc522.PCD_Init();

  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);

  Serial.print("RFID Version: 0x");
  Serial.println(v, HEX);

  if (v == 0x00 || v == 0xFF) {
    Serial.println("No hay comunicacion SPI");
  } else {
    Serial.println("RC522 detectado");
  }

  Serial.println("Acerca una tarjeta...");
}

void loop() 
{
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  Serial.print("UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  mfrc522.PICC_HaltA();
}