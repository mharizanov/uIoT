/*
Exampe uiot.ini:


# uIoT gateway .ini file
[network]
 mac = 42:31:42:21:30:31
 
 # use DHCP
 # (0 means do not use dhcp, 1 means use dhcp)
 usedhcp = 0 
 
 gateway = 192.168.1.1
 ip   =   192.168.1.88      
 dns = 8.8.8.8
 
 #hisip contains website resolved website IP address
 #use when no DNS is available
 hisip = 213.138.101.177
 
 #use the resolved IP address rather than looking it up via DNS? 
 usehisip = 0
 
[rfm]
 #band 8=868Mhz, 4=433Mhz, 9=915Mhz
 band=8
 nodeid=20
 group=210
 
[emoncms]
 api = XXXXXXXX__API__XXXXXXXXX
 website = emoncms.org
 basedir = 
 
 
*/



byte rx()
{
  SPDR = 0xFF;
  loop_until_bit_is_set(SPSR, SPIF);
  return SPDR;
}

void tx(byte d)
{
  SPDR = d;
  loop_until_bit_is_set(SPSR, SPIF);
}

void spi_init()
{
  // Set direction register for SCK and MOSI pin.
  // MISO pin automatically overrides to INPUT.
  // When the SS pin is set as OUTPUT, it can be used as
  // a general purpose output port (it doesn't influence
  // SPI operations).

  pinMode(SCK, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(SS, OUTPUT);
  
  digitalWrite(SCK, LOW);
  digitalWrite(MOSI, LOW);
  digitalWrite(SS, HIGH);

  // Warning: if the SS pin ever becomes a LOW INPUT then SPI 
  // automatically switches to Slave, so the data direction of 
  // the SS pin MUST be kept as OUTPUT.
  SPCR |= _BV(MSTR);
  SPCR |= _BV(SPE);

  pinMode(ETHERNET_SELECT, OUTPUT);
  digitalWrite(ETHERNET_SELECT, HIGH); // deselect ENC28J60

  pinMode(RFM12_SELECT, OUTPUT);
  digitalWrite(RFM12_SELECT, HIGH); // deselect RFM12B  

  pinMode(SD_SELECT, OUTPUT);
  digitalWrite(SD_SELECT, HIGH); // deselect SD card

}


void strobe(int pin)
{
  digitalWrite(pin, HIGH);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  digitalWrite(pin, HIGH);
}

char* skipSpace(char* str)
{
  char *cp = str;
  while (isspace(*cp))
    ++cp;
  return cp;
}

void removeTrailingWhiteSpace(char* str)
{
  char *cp = str + strlen(str) - 1;
  while (cp >= str && isspace(*cp))
    *cp-- = '\0';
}


bool getMACAddress(char* buffer, uint8_t* mac){
  
  int i = 0;
  char* cp = buffer;
  memset(mac, 0, 6);

  while (*cp != '\0' && i < 6) {
    if (*cp == ':' || *cp == '-') {
      ++i;
      ++cp;
      continue;
    }
    if (isdigit(*cp)) {
      mac[i] *= 16; // working in hex!
      mac[i] += (*cp - '0');
    }
    else {
      if (isxdigit(*cp)) {
	mac[i] *= 16; // working in hex!
	mac[i] += (toupper(*cp) - 55); // convert A to 0xA, F to 0xF
      }
      else {
	memset(mac, 0, sizeof(mac));
	return false;
      }
    }
    ++cp;
  }
  return true;
}

bool getIPAddress( char* buffer, uint8_t* ip)
{
  int i = 0;
  char* cp = buffer;
  ip[0] = ip[1] = ip[2] = ip[3] = 0;
  while (*cp != '\0' && i < 4) {
    if (*cp == '.') {
      ++i;
      ++cp;
      continue;
    }
    if (isdigit(*cp)) {
      ip[i] *= 10;
      ip[i] += (*cp - '0');
    }
    else {
      ip[0] = ip[1] = ip[2] = ip[3] = 0;
      return false;
    }
    ++cp;
  }
  return true;
}



void readConfig(int err, char * fp)
{
  if (err == 0)
  {  
    Serial.print(F("Reading File ")); Serial.print(fp); Serial.print(F(" ;err = ")); Serial.println(err);
    str.reset();
    int bytes_read;
    
    do
    {
	PFFS.read_file(str.buf, sizeof(str.buf), &bytes_read);
        int i;
        for(i=0;i<bytes_read && (str.buf[i]!='\n' && str.buf[i]!='\r');i++) {}  //Scan to find the end of line, if exists at all
        str.buf[i]='\0';                                                  //Terminate the line
        if(str.buf[i+1]=='\n' || str.buf[i+1]=='\r')                            //If the next char is NL or CR, skip it
          i++;
        pf_lseek(fs.fptr - (bytes_read-i-1));                       //Position the file pointer at the newline character so that next time we read the next line

        char * keyStart=skipSpace(str.buf);                            
        
        if(*keyStart!='#') {  //Lines starting with # are comments. Ignore them

        char *ep = strchr(str.buf, '=');  //Find '=', if any, in the line
         if (ep != NULL) { 
           *ep='\0';
           ep++;
           ep=skipSpace(ep);
           removeTrailingWhiteSpace(ep);
           removeTrailingWhiteSpace(keyStart);           
           parseLine(keyStart,ep);       // see if we can make sense of this KEY/VALUE pair    
         }
        }
    }
    while (bytes_read == sizeof(str.buf));

  }
  else
  {
    Serial.print(F("Error code ")); Serial.print(err); Serial.print(F(" while opening ")); Serial.println(fp);
  }
}

void parseLine(char * keyStart, char* ep) {

           Serial.print(F("Key: "));
           Serial.print(keyStart);

           Serial.print(F("; Value: "));
           Serial.print(ep);
     
           Serial.println();

  
           if (strcmp_P(keyStart, PSTR("mac")) == 0) {
              byte tmp[6];
              if(getMACAddress(ep,tmp)){    //MAC is valid
                if(!memcmp(storage.mymac,tmp,6)==0) {  //is MAC different from current config? if not, do not raise 'dirty' flag
                    memcpy(storage.mymac,tmp,6);
                    Serial.println(F("MAC changed"));
                    needsave=1;
                }
              }
           }

           if (strcmp_P(keyStart, PSTR("ip")) == 0) {
              byte tmp[4];
              if(getIPAddress(ep,tmp)){    //IP is valid
                if(memcmp(storage.ip,tmp,4)==0) {  //is IP different from current config? if not, do not raise 'dirty' flag
                    memcpy(storage.ip,tmp,4);
                    Serial.println(F("IP changed"));                    
                    needsave=1;
                }
              }
           }
           
           if (strcmp_P(keyStart, PSTR("gateway")) == 0) {
              byte tmp[4];
              if(getIPAddress(ep,tmp)){    //IP is valid
                if(!memcmp(storage.gw,tmp,4)==0) {  //is IP different from current config? if not, do not raise 'dirty' flag
                    memcpy(storage.gw,tmp,4);
                    Serial.println(F("GW changed"));                    
                    needsave=1;
                }
              }
           }
           if (strcmp_P(keyStart, PSTR("dns")) == 0) {
              byte tmp[4];
              if(getIPAddress(ep,tmp)){    //IP is valid
                if(!memcmp(storage.dns,tmp,4)==0) {  //is IP different from current config? if not, do not raise 'dirty' flag
                    memcpy(storage.dns,tmp,4);
                    Serial.println(F("DNS changed"));                    
                    needsave=1;
                }
              }
           }
           
           if (strcmp_P(keyStart, PSTR("hisip")) == 0) {
              byte tmp[4];
              if(getIPAddress(ep,tmp)){    //IP is valid
                if(!memcmp(storage.hisip,tmp,4)==0) {  //is IP different from current config? if not, do not raise 'dirty' flag
                    memcpy(storage.hisip,tmp,4);
                    Serial.println(F("HISIP changed"));                    
                    needsave=1;
                }
              }
           }

           if (strcmp_P(keyStart, PSTR("api")) == 0) {
             if(!strcmp(storage.api,ep)==0) {
                strcpy(storage.api,ep);
                Serial.println(F("API changed"));                                
                needsave=1;
             }
           }

           if (strcmp_P(keyStart, PSTR("website")) == 0) {
             if(!strcmp(storage.website,ep)==0) {
                strcpy(storage.website,ep);
                Serial.println(F("Website changed"));                
                needsave=1;
             }
           }

           if (strcmp_P(keyStart, PSTR("basedir")) == 0) {
             if(!strcmp(storage.basedir,ep)==0) {
                strcpy(storage.basedir,ep);
                Serial.println(F("basedir changed"));                
                needsave=1;
             }
           }

           if (strcmp_P(keyStart, PSTR("band")) == 0) {
             byte tmp=atoi(ep);

/*             
RF12_433MHZ     1   ///< RFM12B 433 MHz frequency band.
RF12_868MHZ     2   ///< RFM12B 868 MHz frequency band.
RF12_915MHZ     3   ///< RFM12B 915 MHz frequency band.
*/             
             if (tmp==4) tmp=1;
             if (tmp==8) tmp=2;
             if (tmp==9) tmp=3;
             
             if(storage.band!=tmp) {
                storage.band=tmp;
                Serial.println(F("band changed"));                
                needsave=1;
             }
           }


           if (strcmp_P(keyStart, PSTR("group")) == 0) {
             if(storage.group!=atoi(ep)) {
                storage.group=atoi(ep);
                Serial.println(F("group changed"));                
                needsave=1;
             }
           }

           if (strcmp_P(keyStart, PSTR("nodeid")) == 0) {
             if(storage.nodeID!=atoi(ep)) {
                storage.nodeID=atoi(ep);
                Serial.println(F("nodeid changed"));                
                needsave=1;
             }
           }


           if (strcmp_P(keyStart, PSTR("usehisip")) == 0) {
             if(storage.usehisip!=atoi(ep)) {
                storage.usehisip=atoi(ep);
                needsave=1;
             }
           }
           
           if (strcmp_P(keyStart, PSTR("usedhcp")) == 0) {
             if(storage.usedhcp!=atoi(ep)) {
                storage.usedhcp=atoi(ep);
                    Serial.println(F("usedhcp changed"));                
                needsave=1;
             }
           }           
           
}

