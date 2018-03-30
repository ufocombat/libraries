// Видеообзоры и уроки работы с ARDUINO на YouTube-канале IOMOIO: https://www.youtube.com/channel/UCmNXABaTjX_iKH28TTJpiqA

#include <OLED_I2C.h>                               // Подключаем библиотеку OLED_I2C для работы со шрифтами и графикой
OLED  myOLED(A4, A5, A4);                           // Определяем пины I2C интерфейса: UNO и NANO -- SDA - пин A4, SCL - пин A5; MEGA -- SDA - пин 20, SCL - пин 21
extern uint8_t RusFont[];                           // Подключаем русский шрифт
extern uint8_t SmallFont[];                         // Подключаем латинский шрифтом

void setup()
{
  myOLED.begin();                                    // Инициализируем библиотеку OLED_I2C
}
void loop()
{
  myOLED.clrScr();                                    // Стираем все с экрана
  myOLED.setFont(RusFont);                            // Инициализируем русский шрифт
  myOLED.print("Heccrbq ihban", CENTER, 0);           // Выводим надпись "Русский язык"
  myOLED.print("F < D U L T : P B Q", CENTER, 12);    // Выводим надпись "А Б В Г Д Е Ж З И Й"
  myOLED.print("R K V Y J G H C N E", CENTER, 22);    // Выводим надпись "К Л М Н О П Р С Т У"
  myOLED.print("A { W X I O } S M", CENTER, 32);      // Выводим надпись "Ф Х Ц Ч Ш Щ Ъ Ы Ь"
  myOLED.print("~ > Z", CENTER, 42);                  // Выводим надпись "Э Ю Я"
  myOLED.setFont(SmallFont);                          // Инициализируем латинский шрифт
  myOLED.print("IOMOIO", CENTER, 52);                 // Выводим надпись "IOMOIO"
  myOLED.update();                                    // Обновляем информацию на дисплее
  delay(3000);                                        // Пауза 3 секунды
  myOLED.clrScr();                                    // Стираем все с экрана
  myOLED.setFont(RusFont);                            // Инициализируем русский шрифт
  myOLED.print("Heccrbq ihban", CENTER, 0);           // Выводим надпись "Русский язык"
  myOLED.print("f , d u l t ; p b q", CENTER, 12);    // Выводим надпись "а б в г д е ж з и й"
  myOLED.print("r k v y j g h c n e", CENTER, 22);    // Выводим надпись "к л м н о п р с т у"
  myOLED.print("a [ w x i o ] s m", CENTER, 32);      // Выводим надпись "ф х ц ш щ ъ ы ь"
  myOLED.print("` . z", CENTER, 42);                  // Выводим надпись "э ю я"
  myOLED.setFont(SmallFont);                          // Инициализируем латинский шрифт
  myOLED.print("IOMOIO", CENTER, 52);                 // Выводим надпись "IOMOIO"
  myOLED.update();                                    // Обновляем информацию на дисплее  
  delay(3000);                                        // Пауза 3 секунды
}
