#include <PubSubClient.h>
#include <Bridge.h>
#include <YunClient.h>

#include <AFMotor.h>
#include <Servo.h>

#include <DHT.h>
#include <DHT_U.h>

#include <Console.h>

// Note this next value is only used if you intend to test against a local MQTT server
byte mqttServer[] = { 192, 168, 10, 49 };
char deviceID[] = "RCCar";
String pubTopic = String("open-it/status/") + deviceID;
String subTopic = String("open-it/order/") + deviceID;

YunClient yunClient;

/* MQTT communication */
// Uncomment this next line and comment out the line after it to test against a local MQTT server
void callback(char* topic, byte* payload, unsigned int length);	// subscription callback
PubSubClient client(mqttServer, 1883, callback, yunClient);

AF_DCMotor motor_left(3, MOTOR34_64KHZ);	// create motor #3, 64KHz pwm
AF_DCMotor motor_right(4, MOTOR34_64KHZ);	// create motor #4, 64KHz pwm

#define SERVO_PIN 10
Servo myservo;				// create servo object to control a servo
int servo_pos = 30;			// variable to store the servo position
int servo_move = 0;

#define DHT_PIN		2
#define DHT_TYPE	DHT11
// Initialize DHT sensor. 
// Note that older versions of this library took an optional third parameter to 
// tweak the timings for faster processors.  This parameter is no longer needed 
// as the current DHT reading algorithm adjusts itself to work on faster procs. 
DHT dht(DHT_PIN, DHT_TYPE); 

#define CDS_PIN	0			// the cell and 10K pulldown are connected to a0
int cds_cell;				// the analog reading from the sensor divider

#define TRIG_PIN	9		// Trigger Pin
#define ECHO_PIN	11		// Echo Pin
int maximumRange = 200;		// Maximum range needed
int minimumRange = 0;		// Minimum range needed
long duration, distance;	// Duration used to calculate distance
float humidity, temperature;
char topicStr[26];
	
void setup()
{
	pinMode(13, OUTPUT);

	// Bridge startup
	digitalWrite(13, HIGH);
	Bridge.begin();
	digitalWrite(13, LOW);
	Console.begin();
	
	dht.begin();

	myservo.attach(SERVO_PIN);			// attaches the servo on pin 10 to the servo object
	myservo.write(servo_pos);	// tell servo to go to position in variable 'servo_pos'

	pinMode(TRIG_PIN, OUTPUT);
	pinMode(ECHO_PIN, INPUT);
}

void loop()
{
	if( !client.connected() ) {
		Console.print("Trying to connect to : ");
		Console.println("mqtt_broker");

		// connection to MQTT server
		if( client.connect(deviceID) ) {
			Console.println("Successfully connected with MQTT");

			subTopic.toCharArray(topicStr, 26);
			client.subscribe(topicStr, 0);	// topic subscription
		}
	}
	client.loop();

	if( servo_move ) {
		if( servo_move == 1 ) {
			if( servo_pos > 0 )			servo_pos--;
		}
		else if( servo_move == 2 ) {
			if( servo_pos < 90 )	servo_pos++;
		}

		myservo.write(servo_pos);	// tell servo to go to position in variable 'servo_pos'
	}

	delay(50);
}

/* Receive Lelylan message and confirm the physical change */
void callback(char* topic, byte* payload, unsigned int length)
{
	char msg_buf[100];

	int i;
	for( i = 0; i < length; i++ )	msg_buf[i] = payload[i];
	msg_buf[i] = '\0';
	String msg_str = String(msg_buf);
	
	// update the physical status and confirm the executed update
	Console.println(msg_str);
	if( msg_str == "MF" ) {
		motor_left.setSpeed(150);		// set the speed to 100/255
		motor_right.setSpeed(150);		// set the speed to 100/255
		motor_left.run(FORWARD);
		motor_right.run(FORWARD);
	}
	else if( msg_str == "MB" ) {
		motor_left.setSpeed(150);		// set the speed to 100/255
		motor_right.setSpeed(150);		// set the speed to 100/255
		motor_left.run(BACKWARD);
		motor_right.run(BACKWARD);
	}
	else if( msg_str == "ML" ) {
		motor_left.setSpeed(130);		// set the speed to 120/255
		motor_right.setSpeed(130);		// set the speed to 120/255
		motor_left.run(BACKWARD);
		motor_right.run(FORWARD);
	}
	else if( msg_str == "MR" ) {
		motor_left.setSpeed(130);		// set the speed to 120/255
		motor_right.setSpeed(130);		// set the speed to 120/255
		motor_left.run(FORWARD);
		motor_right.run(BACKWARD);
	}
	else if( msg_str == "MS" ) {
		motor_left.run(RELEASE);
		motor_right.run(RELEASE);

		servo_move = 0;
	}
	else if( msg_str == "C_D" )		servo_move = 1;
	else if( msg_str == "C_U" )		servo_move = 2;
	else if( msg_str == "C_C" ) {
		servo_pos = 30;
		myservo.write(servo_pos);		// tell servo to go to position in variable 'servo_pos'
	}
	else if( msg_str == "status" ) {
		pubTopic.toCharArray(topicStr, 26);
		getData();
	
		String json = buildJson();
		char jsonStr[200];
		
		json.toCharArray(jsonStr, 200);
		boolean pubresult = client.publish(topicStr, jsonStr);

		Console.print("attempt to send ");
		Console.println(jsonStr);
		Console.print("to ");
		Console.println(topicStr);
		
		if( pubresult )	Console.println("successfully sent");
		else			Console.println("unsuccessfully sent");
	}
}

String buildJson()
{
	String data = "{";

	data += "\n";
	data += "\"distance\": ";
	data += (int)distance;
	data += ",";
	data += "\n";
	data += "\"temperature\": ";
	data += (int)temperature;
	data += ",";
	data += "\n";
	data += "\"humidity\": ";
	data += (int)humidity;
	data += ",";
	data += "\n";
	data += "\"light\": ";
	data += (int)cds_cell;
	data += "\n";
	data += "}";

	return data;
}

void getData()
{
	// Reading temperature or humidity takes about 250 milliseconds!
	// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
	humidity = dht.readHumidity();
	// Read temperature as Celsius (the default)
	temperature = dht.readTemperature();

	// Check if any reads failed and exit early (to try again).
	if( isnan(humidity) || isnan(temperature) ) {
#ifdef	DEBUG_PRINT
		Console.println("Failed to read from DHT sensor!");
#endif
	}
	else {
#ifdef	DEBUG_PRINT
		Console.print("Humidity = ");
		Console.println(humidity, 1);
		Console.print("Temperature = ");
		Console.println(temperature, 1);
#endif
	}

	cds_cell = analogRead(CDS_PIN);  

#ifdef	DEBUG_PRINT
	Console.print("CdS Cell = ");
	Console.println(cds_cell);     // the raw analog reading
#endif

	/* The following TRIG_PIN/ECHO_PIN cycle is used to determine the
	distance of the nearest object by bouncing soundwaves off of it. */ 
	digitalWrite(TRIG_PIN, LOW); 
	delayMicroseconds(2); 
	digitalWrite(TRIG_PIN, HIGH);
	delayMicroseconds(10); 
	digitalWrite(TRIG_PIN, LOW);
	duration = pulseIn(ECHO_PIN, HIGH);
	
	//Calculate the distance (in cm) based on the speed of sound.
	distance = duration / 58.2;

#ifdef	DEBUG_PRINT
	if( (distance > maximumRange) || (distance < minimumRange) ) {
		/* out of range */
		Console.println("Ultra Sonic out of range");
	}
	else {
		Console.print("Distance = ");
		Console.println(distance);
	}
#endif
}

