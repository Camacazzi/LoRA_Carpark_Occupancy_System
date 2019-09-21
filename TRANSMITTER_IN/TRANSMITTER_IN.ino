 /*
 * Ultrasonic reading code created by Rui Santos, https://randomnerdtutorials.com
 */

#include <lmic.h>
//#include <lmic/oslmic.h>
#include <hal/hal.h>
#include <SPI.h>

#if !defined(DISABLE_INVERT_IQ_ON_RX)
#error This example requires DISABLE_INVERT_IQ_ON_RX to be set. Update \
       config.h in the lmic library to set it.
#endif

#define TX_INTERVAL 4000 
//#define NODE_TYPE 1
#define CAR_TRIGGER_DURATION 12
#define SYNC_DURATION 40

 
int trigPin = 3;    // Trigger
int echoPin = 4;    // Echo
long duration, cm, inches;
int count = 0;
int carin = 0;
int type = 0;
char transmit[32];
int delaycount = 0;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,
    .dio = {2, 6, 7},
};

void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

void onEvent (ev_t ev) {
}

osjob_t txjob;
osjob_t timeoutjob;
static void tx_func (osjob_t* job);

// Transmit the given string and call the given function afterwards
void tx(const char *str, osjobcb_t func) {
  os_radio(RADIO_RST); // Stop RX first
  delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet
  LMIC.dataLen = 0;
  while (*str)
    LMIC.frame[LMIC.dataLen++] = *str++;
  //LMIC.osjob.func = func;
  os_radio(RADIO_TX);
  Serial.println("TX");
}

static void txdone_func (osjob_t* job) {
  //rx(rx_func);
}

// log text to USART and toggle LED
static void tx_func(osjob_t* job) {
  //Two transmission types, 0 for sync packet, 1 for data packet.
  //New car has entered!
  if(type){
    snprintf(transmit, sizeof(transmit), "IN\\%i", carin);
    printf("Transmitting packet: %s", carintransmit);
    tx(transmit, txdone_func);
  }
  else{
    //Send sync packet
    printf("Sending sync packet");
    snprintf(transmit, sizeof(transmit), "SYNC//IN//%i", carin);
    tx(transmit, txdone_func);
  }
}

 
void setup() {
  //Serial Port begin
  Serial.begin (9600);
  //Define inputs and outputs for sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);


  Serial.println("Starting");
  #ifdef VCC_ENABLE
  // For Pinoccio Scout boards
  pinMode(VCC_ENABLE, OUTPUT);
  digitalWrite(VCC_ENABLE, HIGH);

  
  Serial.println(LMIC.freq); //from http://wiki.dragino.com/index.php?title=Lora_Shield
  
  delay(1000);
  #endif

  pinMode(LED_BUILTIN, OUTPUT);

  // initialize runtime env
  os_init();

  // Set up these settings once, and use them for both TX and RX

  //see also void LMIC_setDrTxpow (dr_t dr, s1_t txpow)
  
  // Use a frequency in the g3 which allows 10% duty cycling.
  // TODO select different frequencies
  LMIC.freq = 915525000;

  // Set data rate and transmit power (note: txpow seems to be ignored by the library)
  // for loraWAN  LMIC_setDrTxpow(DR_SF7,14);
    
  // Maximum TX power use 27;
  //TODO cycle through different txpowers
  LMIC.txpow = 2; //2 to 15
  // Use a medium spread factor. This can be increased up to SF12 for
  // better range, but then the interval should be (significantly)
  // lowered to comply with duty cycle limits as well.
  LMIC.datarate = DR_SF7; //was DR_SF9;  //other choices eg DR_SF7 lowest energy w BW125
  // This sets CR 4/5, BW125 (except for DR_SF7B, which uses BW250)
  LMIC.rps = updr2rps(LMIC.datarate);

  Serial.println("Started");
  Serial.flush();

  // setup initial job
  //os_setCallback(&txjob, tx_func);
}
 
void loop() {
  type = 0;
  // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
 
  // Read the signal from the sensor: a HIGH pulse whose
  // duration is the time (in microseconds) from the sending
  // of the ping to the reception of its echo off of an object.
  pinMode(echoPin, INPUT);
  duration = pulseIn(echoPin, HIGH);
 
  // Convert the time into a distance
  cm = (duration/2) / 29.1;     // Divide by 29.1 or multiply by 0.0343
  
  Serial.print(cm);
  Serial.print("cm");
  Serial.println();
  //Logic is intended to be robust to small variations in distance
  //If object is close, increment count
  if(cm < 30){
    count++;
  }
  //If object is far, decrement count if count is greater than 0
  else if(count > 0){
    count--;
  }
  //An object has sat infront of the sensor for long enough, consider it to be a car
  if(count >=CAR_TRIGGER_DURATION){
    carin++;
    //Set type to 1 for data transmission
    type = 1;
    count = 0;
    delaycount = 0;
    os_setCallback(&txjob, tx_func);
    Serial.print("Car has entered, car:");
    Serial.print(carin);
    Serial.println();
    os_runloop_once();
  }
  
  delay(250);
  
  //If go through 4 delays without sending, send sync.
  if(!type){
    delaycount++;
  }
  if(delaycount >= SYNC_DURATION){
    type = 0;
    delaycount = 0;
    os_setCallback(&txjob, tx_func);
    os_runloop_once();
  }
  
  
}
