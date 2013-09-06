void dhcp_dns()
{
  //-----------------------------------------------------------------------------------
  // Get DHCP address
  // Putting DHCP setup and DNS lookup in the main loop allows for: 
  // powering nanode before ethernet is connected
  //-----------------------------------------------------------------------------------
 // if (!ether.dhcpSetup()) dhcp_status = 0;    // if dhcp expired start request for new lease by changing status
  
  if (!dhcp_status && storage.usedhcp==1){
    
    #ifdef UNO
    wdt_disable();
    #endif 
    
    dhcp_status = ether.dhcpSetup();           // DHCP setup
    
    #ifdef UNO
    wdt_enable(WDTO_8S);
    #endif
    
    Serial.print(F("DHCP status: "));             // print
    Serial.println(dhcp_status);               // dhcp status
    
    if (dhcp_status){                          // on success print out ip's
      ether.printIp(F("IP:  "), ether.myip);
      ether.printIp(F("GW:  "), ether.gwip);  
        
      ether.copyIp(ether.dnsip, storage.dns);
      ether.printIp(F("DNS: "), ether.dnsip);
      
      if (storage.usehisip==true)
      {
        ether.copyIp(ether.hisip, storage.hisip);
        dns_status = 1;          
      }
      
    } else { ethernet_error = 1; Serial.println(F("DHCP failed")); }  
  }
  
  //-----------------------------------------------------------------------------------
  // Get server address via DNS
  //-----------------------------------------------------------------------------------
  if (dhcp_status && !dns_status && storage.usehisip==0){
    
    #ifdef UNO
    wdt_disable();
    #endif 
    
    Serial.print(F("Trying DNS resolve of:")); Serial.println(storage.website);
    dns_status = ether.dnsLookup(storage.website,true);    // Attempt DNS lookup, name in RAM
    
    #ifdef UNO
    wdt_enable(WDTO_8S);
    #endif;
    
    Serial.print(F("DNS status: "));             // print
    Serial.println(dns_status);               // dns status
    if (dns_status){
      ether.printIp(F("SRV: "), ether.hisip);      // server ip
    } else { ethernet_error = 1; }  
  }

}

