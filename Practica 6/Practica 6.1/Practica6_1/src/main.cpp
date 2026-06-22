#include <SPI.h>
#include <SD.h>

SPIClass spi = SPIClass(FSPI);  

#define SCK  12
#define MISO 13
#define MOSI 11
#define CS   10

File myFile;

void setup()
{
  Serial.begin(9600);
  Serial.print("Iniciando SD ...");

  spi.begin(SCK, MISO, MOSI, CS);

  if (!SD.begin(CS, spi)) 
  {
    Serial.println("No se pudo inicializar");
    return;
  }

  Serial.println("inicializacion exitosa");
  myFile = SD.open("archivo.txt");//abrimos el archivo
  
  if (myFile) 
  {
    Serial.println("archivo.txt:");
    while (myFile.available()) 
    {
      Serial.write(myFile.read());
    }
    
    myFile.close(); //cerramos el archivo
  } 
  else 
  {
    Serial.println("Error al abrir el archivo");
  }
}
void loop()
{
}