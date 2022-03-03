#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPS.h>

/* Create new software serials */
SoftwareSerial GPS_S(4, 3);
SoftwareSerial GSM_S(6, 7);
/* Create lcd I2C object */
LiquidCrystal_I2C LCD_O(0x27, 20, 4);
/* Create gps object */
TinyGPS GPS_O;

/* Constant parameters */
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

static void send_sms(const char *message)
{
        GSM_S.println("AT+CMGF=1");
        delay(1000);
        GSM_S.println("AT+CMGS=\"+972539585611\"\r");
        delay(1000);
        GSM_S.println(message);
        delay(100);
        GSM_S.println((char)26);
}

static inline size_t get_full_message_len(const char *mess, const char *loc_mess)
{
        const size_t loc_size = 2 * sizeof(float);
        const int add_chars = 3;
        
        return strlen(mess) + strlen(loc_mess) + loc_size + add_chars + 1;
}

static inline void get_location(float *latitude, float *longitude)
{
        GPS_O.encode(GPS_S.read());
        GPS_O.f_get_position(latitude, longitude);
}

static void inform_parent(const char *message)
{
        const char *loc_mess = "\nYour son's location is (lat, long):";
        const size_t full_len = get_full_message_len(message, loc_mess);
        char full_message[full_len];
        float longi, lati;

        get_location(&lati, &longi);        
        snprintf(full_message, full_len, "%s%s %f, %f", message, loc_mess, lati, longi);
        lcd_cprint("Sending SMS", 1000);
        send_sms(full_message);
        lcd_cprint("SMS sent", 1000);
}

static void beep_warning()
{
        int i;

        for (i=0; i<5; i++) {
                digitalWrite(_buzzer_pin, HIGH);
                delay(300);
                digitalWrite(_buzzer_pin, LOW);
                delay(300);
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
        lcd_cprint("Drive safely", 1500);
        
        return 0;
}

static bool is_illegal_speed(float kmh)
{
        const int max_speed = 30;

        return kmh > max_speed;
}

static inline float get_current_speed()
{
        GPS_O.encode(GPS_S.read());
        
        return  GPS_O.f_speed_kmph();
}

static int handle_illegal_speed()
{
        warn_driver("Driving fast");
        inform_parent("Your son has passed the driving speed limit.");
        
        return 1;
}

/*
 * Return non-zero value when illegal speed detected
 */
static int speed_test()
{
        if (is_illegal_speed(get_current_speed()))
                return handle_illegal_speed();
        else
                return 0;
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

static int tilt_test()
{
        if (is_sus_tilt())
                return handle_sus_tilt();
        else
                return 0;
}

/*
 * Return non-zero value when illegal move was detected
 */
static int monitor_driver()
{
        int retval;
        
        lcd_cprint("Driving monitor", 500);
         
        if (!(retval = alcohol_test()))
                while (!(retval = speed_test()))
                        if ((retval = tilt_test()))
                                break;
        return retval;
}

static bool are_available_serials()
{
        return (GPS_S.available() && GSM_S.available());
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

void setup() 
{
        const int baud = 9600;
 
        GPS_S.begin(baud);
        GSM_S.begin(baud);
        init_tilt_sensor();
        init_buzzer();
        init_lcd();
}

void loop() 
{
        if (are_available_serials()) {
                if (monitor_driver())
                        stop_arduino();
        } else {
                handle_unavailable_serial();       
        }
}
