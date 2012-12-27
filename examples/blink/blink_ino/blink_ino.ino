void setup(){
 pinMode(4, OUTPUT); 
 pinMode(6, OUTPUT);
 digitalWrite(4,LOW);
 while(1) {
 digitalWrite(6,HIGH);   
 delay(50);
 digitalWrite(6,LOW);
 delay(1000);
 }
}
void loop()
{
}
