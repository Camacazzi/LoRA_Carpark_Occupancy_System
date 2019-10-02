#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#if !defined(DISABLE_INVERT_IQ_ON_RX)
#error This example requires DISABLE_INVERT_IQ_ON_RX to be set. Update \
       config.h in the lmic library to set it.
#endif


/* TODO
4.Edit the "main.cpp" to "Set center frequency" on your Raspberry Pi.
change
uint32_t freq = 868100000; //in Mhz! (868.1)
to
uint32_t freq = 915000000; //in Mhz! (915.0)
*/

// How often to send a packet. Note that this sketch bypasses the normal
// LMIC duty cycle limiting, so when you change anything in this sketch
// (payload length, frequency, spreading factor), be sure to check if
// this interval should not also be increased.
// See this spreadsheet for an easy airtime and duty cycle calculator:
// https://docs.google.com/spreadsheets/d/1voGAtQAjC1qBmaVuP1ApNKs1ekgUjavHuVQIXyYSvNc 
#define TX_INTERVAL 4000 


// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,
    .dio = {2, 6, 7},
};


// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

void onEvent (ev_t ev) {
}

osjob_t txjob;
osjob_t timeoutjob;
static void tx_func (osjob_t* job);

char current_receive[255];
int car old = 0;
int carpresent;
//char carin_buffer = "00000000";
char carin_buffer[128];

int sync_bay_counter = 0;

// Enable rx mode and call func when a packet is received
void rx(osjobcb_t func) {
  LMIC.osjob.func = func;
  LMIC.rxtime = os_getTime(); // RX _now_
  // Enable "continuous" RX (e.g. without a timeout, still stops after
  // receiving a packet)
  os_radio(RADIO_RXON);
  Serial.println("RX");
}

static void rxtimeout_func(osjob_t *job) {
  digitalWrite(LED_BUILTIN, LOW); // off
}

static void rx_func (osjob_t* job) {
  // Blink once to confirm reception and then keep the led on
  digitalWrite(LED_BUILTIN, LOW); // off
  delay(10);
  digitalWrite(LED_BUILTIN, HIGH); // on

  // Timeout RX (i.e. update led status) after 3 periods without RX
  os_setTimedCallback(&timeoutjob, os_getTime() + ms2osticks(3*TX_INTERVAL), rxtimeout_func);

  Serial.print("Got ");
  Serial.print(LMIC.dataLen);
  Serial.println(" bytes");
  Serial.write(LMIC.frame, LMIC.dataLen);
  Serial.println();
  snprintf(current_receive, LMIC.dataLen+1, "%s", LMIC.frame);
  if(strncmp(current_receive, "SYNC", 4) == 0){
    //SYNC PACKET RECEIVED, CAN ASSUME POSITION HASNT CHANGED
    memcpy(&carin_buffer[0], &current_receive[14], LMIC.dataLen * sizeof(char));
    carpresent = atoi(carin_buffer);
    sync_bay_counter = 0;
  }
  else if(strncmp(current_receive, "BAY", 3) == 0){
    //LOOK TO SEE IF CAR HAS LEFT OR ENTERED
    memcpy(&carin_buffer[0], &current_receive[7], LMIC.dataLen * sizeof(char));
    carpresent = atoi(carin_buffer);
    sync_bay_counter = 0;    
    
    
    //TODO SET UP OUTPUT TO TELL PI NEW BAY STATUS


  }
  
  //TODO: write to raspberry pi current free occupancy
  Serial.println();
  Serial.print("DR=");
  Serial.println(LMIC.datarate); //see region specs eg 2=SF7, 3=SF9
  Serial.print("txpow=");
  Serial.println(LMIC.txpow); //dbM
  Serial.print("rssi=");
  Serial.println(LMIC.rssi);  //dbM ?
  Serial.print("snr=");
  Serial.println(LMIC.snr);
  Serial.println();

  // Restart RX
  rx(rx_func);
}

// log text to USART and toggle LED
static void tx_func (osjob_t* job) {
  os_radio(RADIO_RST);

  rx(rx_func);

  //os_setTimedCallback(job, os_getTime() + ms2osticks(TX_INTERVAL + random(500), tx_func);
  os_setTimedCallback(job, os_getTime() + ms2osticks(TX_INTERVAL), tx_func);
}

// application entry point
void setup() {
  Serial.begin(9600); //115200);
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
  os_setCallback(&txjob, tx_func);
}

void loop() {
  // execute scheduled jobs and events
  os_runloop_once();
  sync_bay_counter++;
  if(sync_bay_counter >= 10){
    //TODO: IN NODE HAS BEEN LOST
  }
}
