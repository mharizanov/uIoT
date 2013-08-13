/*
  NanodeRF_multinode

  Relay's data recieved from wireless nodes to emoncms
  Decodes reply from server to set software real time clock
  Relay's time data to emonglcd - and any other listening nodes.
  Looks for 'ok' reply from request to verify data reached emoncms

  emonBase Documentation: http://openenergymonitor.org/emon/emonbase

  Authors: Trystan Lea and Glyn Hudson
  Part of the: openenergymonitor.org project
  Licenced under GNU GPL V3
  http://openenergymonitor.org/emon/license

  EtherCard Library by Jean-Claude Wippler and Andrew Lindsay
  JeeLib Library by Jean-Claude Wippler

  THIS SKETCH REQUIRES:
  
  Libraries in the standard arduino libraries folder
	- EtherCard		https://github.com/jcw/ethercard/

*/

#define UNO       //anti crash wachdog reset only works with Uno (optiboot) bootloader, comment out the line if using delianuova

#include "RF12uiot.h"	  
#include <avr/wdt.h>

#define MYNODE 30           
#define freq RF12_868MHZ     // frequency
#define group 210            // network group

//---------------------------------------------------------------------
// The PacketBuffer class is used to generate the json string that is send via ethernet - JeeLabs
//---------------------------------------------------------------------
class PacketBuffer : public Print {
public:
    PacketBuffer () : fill (0) {}
    const char* buffer() { return buf; }
    byte length() { return fill; }
    void reset()
    { 
      memset(buf,NULL,sizeof(buf));
      fill = 0; 
    }
    virtual size_t write (uint8_t ch)
        { if (fill < sizeof buf) buf[fill++] = ch; }
    byte fill;
    char buf[150];
    private:
};
PacketBuffer str;

//--------------------------------------------------------------------------
// Ethernet
//--------------------------------------------------------------------------
#include <EtherCard.h>		//https://github.com/jcw/ethercard 

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x42,0x31,0x42,0x21,0x30,0x31 };

// 1) Set this to the domain name of your hosted emoncms - leave blank if posting to IP address 
char website[] PROGMEM = "emoncms.org";

// or if your posting to a static IP server:
static byte hisip[] = { 213,138,101,177 };

// change to true if you would like the sketch to use hisip
boolean use_hisip = false;  

// 2) If your emoncms install is in a subdirectory add details here i.e "/emoncms3"
char basedir[] = "";

// 3) Set to your account write apikey 
char apikey[] = "ff95de87c7ea7b114cdd99060a890274";

//IP address of remote sever, only needed when posting to a server that has not got a dns domain name (staticIP e.g local server) 
byte Ethernet::buffer[700];
static uint32_t timer;

const int statusLED = 5;                   // uIoT indicator LED

int ethernet_error = 0;                   // Etherent (controller/DHCP) error flag
int rf_error = 0;                         // RF error flag - high when no data received 
int ethernet_requests = 0;                // count ethernet requests without reply                 

int dhcp_status = 0;
int dns_status = 0;

int data_ready=0;                         // Used to signal that emontx data is ready to be sent
unsigned long last_rf;                    // Used to check for regular emontx data - otherwise error

char line_buf[50];                        // Used to store line of http reply header

unsigned long time60s = -50000;

static int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

//**********************************************************************************************************************
// SETUP
//**********************************************************************************************************************
void setup () {
  
  pinMode(statusLED, OUTPUT); digitalWrite(statusLED,HIGH);       
  delay(1000);
  
  Serial.begin(9600);
  Serial.println("\n[webClient]");
  Serial.print(freeRam());
  Serial.println(" bytes of RAM free");
 
  rf12_initialize(MYNODE, freq,group);  //RFM12b initialization MUST happen before ethernet..
  last_rf = millis()-40000;                                       // setting lastRF back 40s is useful as it forces the ethernet code to run straight away
 
  if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0) {	//for use with NanodeRF
    Serial.println( "Failed to access Ethernet controller");
    ethernet_error = 1;  
  }
  
  dhcp_status = 0;
  dns_status = 0;
  ethernet_requests = 0;
  ethernet_error=0;
  rf_error=0;
  
  digitalWrite(statusLED,LOW);                                    // Green LED off - indicate that setup has finished   
  #ifdef UNO
  wdt_enable(WDTO_8S); 
  #endif;
  
}

//**********************************************************************************************************************
// LOOP
//**********************************************************************************************************************
void loop () {
 
  #ifdef UNO
  wdt_reset();
  #endif

  dhcp_dns();   // handle dhcp and dns setup 
  
  // Display error states on status LED
  if (ethernet_error==1 || rf_error==1 || ethernet_requests > 0) digitalWrite(statusLED,HIGH);
      else digitalWrite(statusLED,LOW);

  //-----------------------------------------------------------------------------------------------------------------
  // 1) On RF recieve
  //-----------------------------------------------------------------------------------------------------------------
  checkRF();
  
  //-----------------------------------------------------------------------------------------------------------------
  // 2) If no data is recieved from rf12 module the server is updated every 30s with RFfail = 1 indicator for debugging
  //-----------------------------------------------------------------------------------------------------------------
  if ((millis()-last_rf)>30000 && data_ready==0)
  {
    last_rf = millis();                                                 // reset lastRF timer
    str.reset();                                                        // reset json string
    str.print(basedir); str.print("/api/post.json?");
    str.print("apikey="); str.print(apikey);
    str.print("&json={rf_fail:1}\0");                                   // No RF received in 30 seconds so send failure 
    data_ready = 1;                                                     // Ok, data is ready
    rf_error=1;
  }

  //-----------------------------------------------------------------------------------------------------------------
  // 3) Send data via ethernet
  //-----------------------------------------------------------------------------------------------------------------

  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  if (data_ready) {
    
    Serial.print("Data sent: "); Serial.println(str.buf); // print to serial json string

    // Example of posting to emoncms.org http://emoncms.org 
    // To point to your account just enter your WRITE APIKEY 
    ethernet_requests ++;
    ether.browseUrl(PSTR("") ,str.buf, website, my_callback);
    data_ready = 0;
  }

   
  if (ethernet_requests > 10) delay(10000); // Reset the nanode if more than 10 request attempts have been tried without a reply

  if ((millis()-time60s)>60000)
  {
    time60s = millis();                                                 // reset lastRF timer
    str.reset();
    str.print(basedir); str.print("/time/local.json?"); str.print("apikey="); str.print(apikey);
    Serial.println("Time request sent");
    ether.browseUrl(PSTR("") ,str.buf, website, my_callback);
  }
}
//**********************************************************************************************************************

//-----------------------------------------------------------------------------------
// Ethernet callback
// recieve reply and decode
//-----------------------------------------------------------------------------------
static void my_callback (byte status, word off, word len) {
  int lsize =   get_reply_data(off);
  
  if (strcmp(line_buf,"ok")==0)
  {
    Serial.println("OK recieved"); ethernet_requests = 0; ethernet_error = 0;
  }
  else if(line_buf[0]=='t')
  {
    Serial.print("Time: ");
    Serial.println(line_buf);
    
    char tmp[] = {line_buf[1],line_buf[2],0};
    byte hour = atoi(tmp);
    tmp[0] = line_buf[4]; tmp[1] = line_buf[5];
    byte minute = atoi(tmp);
    tmp[0] = line_buf[7]; tmp[1] = line_buf[8];
    byte second = atoi(tmp);

    if (hour>0 || minute>0 || second>0) 
    {  
      char data[] = {'t',hour,minute,second};
      int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
      rf12_sendStart(0, data, sizeof data);
      rf12_sendWait(0);
    }
  }
}


void dhcp_dns()
{
  //-----------------------------------------------------------------------------------
  // Get DHCP address
  // Putting DHCP setup and DNS lookup in the main loop allows for: 
  // powering nanode before ethernet is connected
  //-----------------------------------------------------------------------------------
 // if (!ether.dhcpSetup()) dhcp_status = 0;    // if dhcp expired start request for new lease by changing status
  
  if (!dhcp_status){
    
    #ifdef UNO
    wdt_disable();
    #endif 
    
    dhcp_status = ether.dhcpSetup();           // DHCP setup
    
    #ifdef UNO
    wdt_enable(WDTO_8S);
    #endif
    
    Serial.print("DHCP status: ");             // print
    Serial.println(dhcp_status);               // dhcp status
    
    if (dhcp_status){                          // on success print out ip's
      ether.printIp("IP:  ", ether.myip);
      ether.printIp("GW:  ", ether.gwip);  
      
      static byte dnsip[] = {8,8,8,8};  
      ether.copyIp(ether.dnsip, dnsip);
      ether.printIp("DNS: ", ether.dnsip);
      
      if (use_hisip==true)
      {
        ether.copyIp(ether.hisip, hisip);
        dns_status = 1;          
      }
      
    } else { ethernet_error = 1; Serial.println("DHCP failed"); }  
  }
  
  //-----------------------------------------------------------------------------------
  // Get server address via DNS
  //-----------------------------------------------------------------------------------
  if (dhcp_status && !dns_status){
    
    #ifdef UNO
    wdt_disable();
    #endif 
    
    dns_status = ether.dnsLookup(website);    // Attempt DNS lookup
    
    #ifdef UNO
    wdt_enable(WDTO_8S);
    #endif;
    
    Serial.print("DNS status: ");             // print
    Serial.println(dns_status);               // dns status
    if (dns_status){
      ether.printIp("SRV: ", ether.hisip);      // server ip
    } else { ethernet_error = 1; }  
  }

}


int get_header_line(int line,word off)
{
  memset(line_buf,NULL,sizeof(line_buf));
  if (off != 0)
  {
    uint16_t pos = off;
    int line_num = 0;
    int line_pos = 0;
    
    while (Ethernet::buffer[pos])
    {
      if (Ethernet::buffer[pos]=='\n')
      {
        line_num++; line_buf[line_pos] = '\0';
        line_pos = 0;
        if (line_num == line) return 1;
      }
      else
      {
        if (line_pos<49) {line_buf[line_pos] = Ethernet::buffer[pos]; line_pos++;}
      }  
      pos++; 
    } 
  }
  return 0;
}

int get_reply_data(word off)
{
  memset(line_buf,NULL,sizeof(line_buf));
  if (off != 0)
  {
    uint16_t pos = off;
    int line_num = 0;
    int line_pos = 0;
    
    // Skip over header until data part is found
    while (Ethernet::buffer[pos]) {
      if (Ethernet::buffer[pos-1]=='\n' && Ethernet::buffer[pos]=='\r') break;
      pos++; 
    }
    pos+=2;
    while (Ethernet::buffer[pos])
    {
      if (line_pos<49) {line_buf[line_pos] = Ethernet::buffer[pos]; line_pos++;} else break;
      pos++; 
    }
    line_buf[line_pos] = '\0';
    return line_pos;
  }
  return 0;
}

void checkRF() {
  
   if (rf12_recvDone() && rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0) {
      
        digitalWrite(statusLED,HIGH);
        int node_id = (rf12_hdr & 0x1F);
        byte n = rf12_len;
                 
        str.reset();
        str.print(basedir); str.print("/api/post.json?");
        str.print("apikey="); str.print(apikey);
        str.print("&node=");  str.print(node_id);
        str.print("&csv=");
        for (byte i=0; i<n; i+=2)
        {
          unsigned int num = ((unsigned char)rf12_data[i+1] << 8 | (unsigned char)rf12_data[i]);
          if (i) str.print(",");
          str.print(num);
        }

        str.print("\0");  //  End of json string
        data_ready = 1; 
        last_rf = millis(); 
        rf_error=0;
        
        delay(20);
        digitalWrite(statusLED, LOW);
      
  }
 
}
