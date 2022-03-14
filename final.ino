#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPS.h>

/* Create new software serials */
SoftwareSerial GPS_S(3, 4);
SoftwareSerial GSM_S(7, 6);
/* Create lcd I2C object */
LiquidCrystal_I2C LCD_O(0x27, 20, 4);
/* Create gps object */
TinyGPS GPS_O;

/* Constant parameters */
const int _baud = 9600;
const int _buzzer_pin = 2;
const int _tilt_pin = 5;
const int _mq3_pin = A0;


static float get_alcohol_value()
{
        return analogRead(_mq3_pin);
}

static bool is_drunk()
{
        /* Min alcohol level that we consider drunk */
        const int drunk = 20;

        return (get_alcohol_value() > drunk);
}

/*
 * Clear lcd screen then print the passed message and delay 
 * for the amount of milliseconds that were passed.
 */
static void lcd_cprint(const char *message, unsigned long ms)
{
        LCD_O.clear();
        LCD_O.print(message);
        delay(ms);
}

static void init_alcohol_test()
{
        lcd_cprint("Alcohol Test", 1500);
        lcd_cprint("1", 1000);
        lcd_cprint("1..2", 1000);
        lcd_cprint("1..2..3", 1000);
        lcd_cprint("START EXHALING", 3000);
}

/*
 * Since the fact that arduino nano can handle only one 
 * additional serial, we must use this function to be able
 * to start the GSM serial. If start = true it'll end the GPS
 * Serial and start a new GSM serial. Otherwise it will end 
 * the GSM serial and start a new GPS serial.
 */
static void handle_gsm_serial(bool start)
{
        if (start) {
                GPS_S.end();
                GSM_S.begin(_baud);
        } else {
                GSM_S.end();
                GPS_S.begin(_baud);
        }
        delay(200);
}

static void send_sms(String message)
{
        handle_gsm_serial(true);
        GSM_S.println("AT+CMGF=1");
        delay(1000);
        GSM_S.println("AT+CMGS=\"+972539585611\"\r");
        delay(1000);
        GSM_S.println(message);
        delay(100);
        GSM_S.println((char)26);
        handle_gsm_serial(false);
}

static void get_location(float *flat, float *flon)
{
        while (1) {
                if (GPS_S.available() && GPS_O.encode(GPS_S.read())) {
                        GPS_O.f_get_position(flat, flon);
                        break;
                }
        }
}

static void inform_parent(String message)
{
        const String loc = "\nYour son's location is (lat, long): ";
        const String seperator = ", ";
        String full_message;
        float flon, flat;

        get_location(&flat, &flon);
        full_message = String(message + loc + flat + seperator + flon);
        lcd_cprint("Sending SMS", 1000);
        send_sms(full_message);
        lcd_cprint("SMS sent", 1000);
}

static void beep_warning()
{
        int i;

        for (i=0; i<5; i++) {
                digitalWrite(_buzzer_pin, HIGH);
                delay(100);
                digitalWrite(_buzzer_pin, LOW);
                delay(100);
        }
}

static void warn_driver(const char *message)
{
        beep_warning();
        lcd_cprint(message, 1500);
        lcd_cprint("Informing Parent", 1500);
}

static int handle_drunk_driver()
{
        warn_driver("You are drunk");
        inform_parent("Your son wants to drive while being drunk.");

        return 1;
}

/*
 * Return non-zero value when drunk 
 */
static int alcohol_test()
{
        int i;
  
        init_alcohol_test();

        for (i=0; i<4000; i++)
                if (is_drunk())
                        return handle_drunk_driver();
                                                                    
        lcd_cprint("Drive safely", 100);
        
        return 0;
}

static bool is_illegal_speed(float kmh)
{
        const int max_speed = 30;

        return kmh > max_speed;
}

static int handle_illegal_speed()
{
        warn_driver("Driving fast");
        inform_parent("Your son has passed the driving speed limit.");
        
        return 1;
}

static inline bool is_sus_tilt() 
{
        return digitalRead(_tilt_pin);
}

static int handle_sus_tilt()
{
        warn_driver("Suspicious tilt");
        inform_parent("A suspicious tilt was detcted while your son is driving.");

        return 1;
}

/*
 * Return non-zero value when illegal move was detected
 */
static int monitor_driver()
{
        int retval;
        
        lcd_cprint("Driving monitor", 500);
         
        if (!(retval = alcohol_test()))
                while (1) {
                        if (GPS_S.available() && GPS_O.encode(GPS_S.read()))
                                if (is_illegal_speed(GPS_O.f_speed_kmph()))
                                        return handle_illegal_speed();
                        if (is_sus_tilt())
                                return handle_sus_tilt();
                }
        return retval;
}

static void stop_arduino()
{
        while (1)
                lcd_cprint("Restart Arduino", 60000);
}

static void handle_unavailable_serial()
{
        LCD_O.clear();
        LCD_O.print("Unavailable");
        LCD_O.setCursor(0, 1);
        LCD_O.print("Serial");
        LCD_O.setCursor(0, 0);
        delay(2000);
        lcd_cprint("Trying again in:", 1500);
        lcd_cprint("1", 1000);
        lcd_cprint("1..2", 1000);
        lcd_cprint("1..2..3", 1000);
}

static void init_lcd()
{
        LCD_O.init();
        LCD_O.backlight();
}

static void init_tilt_sensor()
{
        pinMode(_tilt_pin, INPUT);
}

static void init_buzzer()
{
        pinMode(_buzzer_pin, OUTPUT);
}

static bool is_available_serial()
{
        return (GPS_S.available() > 0);
}

void setup() 
{
        GPS_S.begin(_baud);
        init_tilt_sensor();
        init_buzzer();
        init_lcd();
}

void loop() 
{
        if (is_available_serial()) {
                if (monitor_driver())
                        stop_arduino();
        } else {
                handle_unavailable_serial();       
        }
}
