#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

//------------ Function Declarations ----------------------
int readButtons();
void oledPrint(char *, int, bool);
void settingsMenu();
void printMenuItem(int);
void updateDisplayStatus(float,float);
void updateDeviceStatus(float);
void setTimer();
void showCancelled();
void timeToChar(char *,int);
void setThreshold();

//---------------------------------------------------------

// Pin Settings
const int UP_BUTTON = 8;
const int DOWN_BUTTON = 7;
const int ENTER_BUTTON = 6;
const int RELAY = 5;
const int GREEN_LED = 4;
const int YELLOW_LED = 3;
const int RED_LED = 2;
const int SENSOR = 9;

// Button Settings
const int UP = 1;
const int DOWN = 2;
const int ENTER = 3;
const int CANCEL = -1;

// Mode Settings
const int FAN_OFF = 0;
const int FAN_ON = 1;
const int FAN_AUTO = 2;
const int FAN_TIMED = 3;

const int longpress = 1000; // ms for long press to be registered

const int SCREEN_WIDTH = 128; // O		oled.clearDisplay();LED oled width, in pixels
const int SCREEN_HEIGHT = 64; // OLED oled height, in pixels



// Declaration for an SSD1306 oled connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Dust sensor variables
const int ratioCount = 4;  

unsigned long sampleTime=30000; // How long to pull and average values from the sensor
unsigned long timer_timeout = 0; // This is the ms value of when the timer expires. 0 = timer not set.
unsigned long startTime = 0;

// User Settings Globals
float threshold = 4.0;
int current_mode = FAN_AUTO;
int activation_counter = 0;

void setup() 
{
	Serial.begin(115200);
	// put your setup code here, to run once:
	pinMode(UP, INPUT);
	pinMode(DOWN, INPUT);
	pinMode(ENTER, INPUT);
	pinMode(RELAY,OUTPUT);
	pinMode(RED_LED, OUTPUT);
	pinMode(YELLOW_LED, OUTPUT);
	pinMode(GREEN_LED, OUTPUT);
    pinMode(SENSOR,INPUT);
	oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);

	oledPrint("DUSTBOT",2,true);
	oledPrint("[Version 2.0]",1,false);
	oledPrint("",1,false);
	oledPrint("^_^",2,false);

	for(int i = 1;i < 4; i++)
	{
		for(int z = 4; z > 1; z--)
		{
			digitalWrite(z,HIGH);
			delay(100);
			digitalWrite(z,LOW);
		}
		for(int z = 3; z < 4; z++)
		{
			digitalWrite(z,HIGH);
			delay(100);
			digitalWrite(z,LOW);
		}

	}


	while(digitalRead(ENTER_BUTTON)==LOW) 
	{
		//Chill
	}
	delay(100);
	current_mode = FAN_AUTO;
    startTime=millis(); // Starts a timer


}

void loop() 
{

	unsigned long duration; 
	unsigned long lpo=0;
	float ratio = 0.0;
	float concentration = 0.0;
	int button_pressed = 0;
	bool LED_ON = false;
	unsigned long timer = 0;

	switch(current_mode) 
	{

		case FAN_ON:
			digitalWrite(RELAY,HIGH);
			oledPrint("FILTER ON",2,true);
			oled.setCursor(0,55);
			oledPrint("Long Press To Cancel",1,false);
			digitalWrite(GREEN_LED,HIGH);
			LED_ON = true;
			timer = millis();
			while(button_pressed != CANCEL)
			{
				if(millis() > timer + 1000) 
				{
					if(LED_ON)
					{
						digitalWrite(GREEN_LED,LOW);
						LED_ON = false;
						timer = millis();
					} 
					else
					{
						digitalWrite(GREEN_LED,HIGH);
						LED_ON = true;
						timer = millis();
					}
				}
				button_pressed = readButtons();
			}
			
			if(button_pressed == CANCEL)
			{
				showCancelled();
			}
			digitalWrite(RELAY,LOW);
			digitalWrite(GREEN_LED,LOW);
			current_mode = FAN_AUTO;
			break;

		case FAN_OFF:
			digitalWrite(RELAY,LOW);
			oledPrint("FILTER OFF",2,true);
			oled.setCursor(0,55);
			oledPrint("Long Press To Cancel",1,false);
			digitalWrite(RED_LED,HIGH);
			LED_ON = true;
			while(button_pressed != CANCEL)
			{
				if(millis() > timer + 1000) 
				{
					if(LED_ON)
					{
						digitalWrite(RED_LED,LOW);
						LED_ON = false;
						timer = millis();
					} 
					else
					{
						digitalWrite(RED_LED,HIGH);
						LED_ON = true;
						timer = millis();
					}
				}
				button_pressed = readButtons();
			}
			if(button_pressed == CANCEL)
			{
				showCancelled();
			}
			digitalWrite(RED_LED,LOW);
			current_mode = FAN_AUTO;
			break;

		case FAN_TIMED:
			char output[6];
			digitalWrite(RELAY,HIGH);
			oledPrint("TIMER ON",2,true);	
			oled.setCursor(0,55);
			oledPrint("Long Press To Cancel",1,false);
			oled.setCursor(0,20);
			oledPrint("Time Remaining",1,false);
			oled.setCursor(0,34);

			timeToChar(output, ceil((float)(timer_timeout-millis())/(float)60000));
			oledPrint(output,2,false);
			timer = millis();

			while(button_pressed != CANCEL and millis() < timer_timeout)
			{
				button_pressed = readButtons();

				// Refresh Display every 1 second
				if(millis() > timer+1000) 
				{
					if(LED_ON)
					{
						digitalWrite(GREEN_LED,LOW);
						digitalWrite(RED_LED,HIGH);
						LED_ON = false;
					}
					else
					{
						digitalWrite(RED_LED,LOW);
						digitalWrite(GREEN_LED,HIGH);
						LED_ON = true;
					}
					oled.writeFillRect(0,34,SCREEN_WIDTH,15,BLACK);
					oled.setCursor(0,34);
					timeToChar(output, ceil((float)(timer_timeout-millis())/(float)60000));
					oledPrint(output,2,false);

					timer = millis();
				}
			}
			digitalWrite(GREEN_LED,LOW);
			digitalWrite(RED_LED,LOW);
			if(button_pressed == CANCEL)
			{
				// Timer was Cancelled
				digitalWrite(RELAY,LOW);
				showCancelled();
			}
			startTime=millis();
			current_mode = FAN_AUTO;
			break;

		case FAN_AUTO:
			oledPrint("DUST LEVEL",2,true);
			oled.setCursor(0,40);
			oledPrint("Sampling....",1,false);

			while(current_mode == FAN_AUTO)
			{
				// Start collecting Pulse durations from the Sensor
				duration = pulseIn(SENSOR, LOW, 100000);
				lpo=lpo+duration;

				// Check for button press
				if(readButtons() == ENTER)
				{
					settingsMenu();
					lpo = 0;
					oledPrint("DUST LEVEL",2,true);
					oled.setCursor(0,40);
					oledPrint("Sampling....",1,false);
				}


				// Check if timer has expired
				if(millis() > (startTime + sampleTime))
				{
					// Calculate particulate ratio and concentration

					ratio=lpo/(sampleTime*10.0);   
					lpo=0;
					startTime=millis();
					concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // From Datasheet

					updateDisplayStatus(ratio,concentration);
					updateDeviceStatus(ratio);

				}
			}
			break;
	}
}

void timeToChar(char* output, int total_minutes)
{
	char tmp[6] = "";
	
	int hours = total_minutes/60;
	int minutes = total_minutes%60;

	strcpy(output,"");
	itoa(hours,tmp,10);
	strcpy(output,tmp);
	strcat(output,":");
	if(minutes < 10)
	{
		strcat(output,"0");
	}
	itoa(minutes,tmp,10);
	strcat(output, tmp);
}

void setTimer()

	// returns true if timer is to be set
	// returns false if cancelled
{
	timer_timeout = 0;
	int total_minutes = 10;
	int minutes = 0;
	int hours = 0;
	char tmp[22] = "";
	char output[6] = "";
	int button_pressed = 0;

	oledPrint("SET TIME",2,true);
	oled.setCursor(0,25);
	oledPrint("0:10",3,false);
	oled.setCursor(0,55);
	oledPrint("Long Press To Cancel",1,false);
	while(button_pressed != ENTER && button_pressed != CANCEL)
	{
		button_pressed = readButtons();
		if(button_pressed == UP) 
		{
			total_minutes = total_minutes + 10;
		}
		if(button_pressed == DOWN)
		{
			if(total_minutes > 10) 
			{
				total_minutes = total_minutes - 10;
			}
		}

		hours = total_minutes/60;
		minutes= total_minutes%60;
		oled.writeFillRect(0,25,SCREEN_WIDTH,25,BLACK);
		oled.setCursor(0,25);
		strcpy(tmp,"");
		itoa(hours,tmp,10);
		strcpy(output,tmp);
		strcat(output,":");
		if(minutes == 0)
		{
			strcpy(tmp,"00");
		} 
		else
		{
			itoa(minutes,tmp,10);
		}
		strcat(output,tmp);
		oledPrint(output,3,false);

	}
	current_mode = FAN_TIMED;
	timer_timeout = (total_minutes * 60000) + millis();

}

void setThreshold()
{
	bool value_changed = false;
	float new_threshold = threshold;
	char output[6] = "";
	oledPrint("THRESHOLD",2,true);
	oled.setCursor(0,25);
	dtostrf(new_threshold,3,1,output);
	oledPrint(output,3,false);
	oled.setCursor(0,56);
	oledPrint("Long Press To Cancel",1,false);
	int button_pressed = 0;
	while(button_pressed != ENTER && button_pressed != CANCEL)
	{
		button_pressed = readButtons();
		if(button_pressed == UP)
		{
			new_threshold = new_threshold + 0.1;
			if(new_threshold > 90.0)
			{
				new_threshold = 90.0;
			}
			value_changed = true;
		}

		if(button_pressed == DOWN)
		{
			new_threshold = new_threshold -0.1;
			if(new_threshold < 0.0)
			{
				new_threshold = 0.0;
			}
			value_changed = true;
		}

		if(value_changed)
		{
			oled.writeFillRect(0,25,SCREEN_WIDTH,30,BLACK);
			oled.setCursor(0,25);
			dtostrf(new_threshold,3,1,output);
			oledPrint(output,3,false);
		}


	}
	threshold = new_threshold;

}


void printMenuItem(int item)
{
	oled.writeFillRect(0,15,SCREEN_WIDTH,40,BLACK);
	oled.setCursor(0,25);
	switch(item) {
  		case 1:
			oledPrint("FAN ON",2,false);
			break;
  		case 2:
			oledPrint("FAN OFF",2,false);
			break;
  		case 3:
			oledPrint("SET TIMER",2,false);
			break;
  		case 4:
			oledPrint("THRESHOLD",2,false);
			break;

	}
}
void settingsMenu()

{
	int current_item = 1;
	int selected_item = 0;
	int max_items = 4;
	bool exit = false;

	oledPrint("Select Option",1,true);
	oled.setCursor(0,56);
	oledPrint("Long Press to Exit",1,false);
	printMenuItem(current_item);

	while(selected_item != max_items && !exit)
	{
		int button_pressed = readButtons();
		switch(button_pressed)
		{
			case 0:
				break;

			case UP:
				if(current_item == 1) 
				{
					current_item=max_items;
				} 
				else
				{
					current_item = current_item - 1;
				}
				printMenuItem(current_item);
				break;

			case DOWN:
				if(current_item == max_items) 
				{
					current_item = 1;
				} 
				else
				{
					current_item = current_item + 1;
				}
				printMenuItem(current_item);
				break;

			case CANCEL:
				exit = true;
				showCancelled();
				break;

			case ENTER:
				exit = true;
				switch(current_item) {
					case 1:
						current_mode = FAN_ON;
						break;
				
					case 2:
						current_mode = FAN_OFF;
						break;

					case 3:
						setTimer();
						break;
				
					case 4:
						setThreshold();
						break;
				}
				break;
			
		}
	}
}


void updateDeviceStatus(float ratio)			

	// Update Indicator LED and Fan Status
{

	if(ratio < threshold/2) 
	{
		digitalWrite(RED_LED,LOW);
		digitalWrite(YELLOW_LED,LOW);
		digitalWrite(GREEN_LED,HIGH);
		activation_counter--;
		if(activation_counter < 0) 
		{
			activation_counter = 0;
			digitalWrite(RELAY,LOW);
		}

	}
	if((ratio > threshold/2) && (ratio < threshold))
	{
		digitalWrite(RED_LED,LOW);
		digitalWrite(YELLOW_LED,HIGH);
		activation_counter++;
		digitalWrite(GREEN_LED,LOW);
		if(current_mode == FAN_AUTO) 
		{
			digitalWrite(RELAY, LOW);
		}

	}
	if(ratio > threshold) 
	{
		digitalWrite(RED_LED,HIGH);
		digitalWrite(YELLOW_LED,LOW);
		digitalWrite(GREEN_LED,LOW);
		activation_counter = activation_counter + 2;
		if(activation_counter > 9) 
		{
			activation_counter = 10;
		}
		if(activation_counter > 5)
		{
			digitalWrite(RELAY, HIGH);
		}
	}

}

void updateDisplayStatus(float ratio,float concentration) 
{
	char tmp[20] = "";

	dtostrf(ratio,3,2,tmp);
	oled.writeFillRect(0,20,SCREEN_WIDTH,45,BLACK);
	oled.setCursor(0,25);
	strcat(tmp, "%");
	oledPrint(tmp,3,false);

	oled.setCursor(0,55);
	strcpy(tmp,"");
	dtostrf(concentration,3,2,tmp);
	strcat(tmp," pcs/L");
	oledPrint(tmp,1,false);

}

void oledPrint(char *inString,int size,bool clear)
{
	int16_t x1;
	int16_t y1;
	uint16_t width;
	uint16_t height;
	if(clear) 
	{
		oled.clearDisplay();
		oled.setCursor(0, 0);
	}
	uint16_t ypos = oled.getCursorY();
	oled.setTextSize(size);
	oled.getTextBounds(inString, 0, 0, &x1, &y1, &width, &height);
	oled.setTextColor(WHITE);

	//centers horiz and vert
    //oled.setCursor((SCREEN_WIDTH - width) / 2, (SCREEN_HEIGHT - height)/2);
	oled.setCursor((SCREEN_WIDTH - width) / 2, y1+ypos);

	oled.println(inString);
	oled.display();

}

int readButtons() 
{
	unsigned long presstime = 0;
	unsigned long start = millis();

	if(digitalRead(UP_BUTTON) == HIGH) 
	{
		delay(200);
		return UP;
	}

	if(digitalRead(DOWN_BUTTON) == HIGH) 
	{
		delay(200);
		return DOWN;
	}
	
	if(digitalRead(ENTER_BUTTON) == HIGH) 
	{
		while(digitalRead(ENTER_BUTTON)==HIGH && (millis() < start+1200))
		{
			// Do NothingON_MODE:
		}
		presstime =  millis() - start;
		if(presstime > longpress) 
		{
			return CANCEL;
		}
		return ENTER;
	}

	return 0;
}

void showCancelled()
{
	oled.clearDisplay();
	oled.setCursor(0,25);
	oledPrint("CANCELLED",2,false);
	while(digitalRead(ENTER_BUTTON) == HIGH)
	{
		// Wait for user to stop pressing button
	}
	delay(600);
}
