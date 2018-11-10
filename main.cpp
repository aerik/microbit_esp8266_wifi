/*
* From http://bluexmas.tistory.com/791, no license specified
* Revisions:
* 	Fixed setRxBufferSize to 255 as it's only a uint8
*   Still out of memory with 255, using 128
*/

#include "MicroBit.h"
 
MicroBit uBit;
 
void initWiFi();
int ATWiFi();
int resetWiFi();
int setUARTWiFi();
int scanWiFi();
int getIPWiFi();
int modeWiFi(int mode);
int connectWiFi(ManagedString ssid, ManagedString pass); 
int getWebPageWiFi(ManagedString URL, ManagedString page);
int getVersionWiFi();
int startServerWiFi();
int waitForWiFi(ManagedString target, int retry, int pause);
//ManagedString waitForWiFiString(ManagedString target, int retry, int pause);
int find(ManagedString c, ManagedString s);
void debug(ManagedString s);
 
#define Tx MICROBIT_PIN_P0
#define Rx MICROBIT_PIN_P1
#define DEBUG 1
 
int main() {
    uBit.init();
    uBit.display.print("Starting");
    initWiFi();
    modeWiFi(1);//station mode (?)
    connectWiFi("vr.ap.1", "vetrocket2010");
    getIPWiFi();
    uBit.display.print("OK");
    startServerWiFi();
    release_fiber();
}
 
void initWiFi() {
    uBit.serial.redirect(Tx, Rx);
    uBit.serial.baud(115200);
    uBit.serial.setRxBufferSize(128);//even 255 seems too big
}
 
int ATWiFi() {
    uBit.serial.send("AT\r\n", SYNC_SPINWAIT);
    return waitForWiFi("OK", 150, 10);
}
 
int getVersionWiFi() {
    uBit.serial.send("AT+GMR\r\n", SYNC_SPINWAIT);
    return waitForWiFi("OK", 200, 10);
}
 
int resetWiFi() {
    uBit.serial.send("AT+RST\r\n", SYNC_SPINWAIT);
    return waitForWiFi("OK", 1000, 10);
}
 
int setUARTWiFi() {
    uBit.serial.send("AT+UART_CUR=115200,8,1,0,0\r\n", SYNC_SPINWAIT);
    return waitForWiFi("OK", 200, 10);
}
 
int scanWiFi() {
    uBit.serial.send("AT+CWLAP\r\n", SYNC_SPINWAIT);
    return waitForWiFi("OK", 500, 50);
}
 
int modeWiFi(int mode) {
    ManagedString cmd = "AT+CWMODE_CUR=" + ManagedString(mode) + "\r\n";
    uBit.serial.send(cmd, SYNC_SPINWAIT);
    return waitForWiFi("OK", 200, 10);
}
 
int connectWiFi( ManagedString ssid, ManagedString pass) {
    ManagedString cmd = "AT+CWJAP_CUR=\"" + ssid + "\",\"" + pass + "\"\r\n";
    uBit.serial.send(cmd, SYNC_SPINWAIT);
    return waitForWiFi("OK", 200, 20);
}
 
int getIPWiFi() {
    uBit.serial.send("AT+CIFSR\r\n", SYNC_SPINWAIT);
    return waitForWiFi("OK", 200, 10);
}
 
int getWebPageWiFi(ManagedString URL, ManagedString page) {
    ManagedString cmd = "AT+CIPSTART=\"TCP\",\"" + URL + "\",80\r\n";
    uBit.serial.send(cmd, SYNC_SPINWAIT);
     
    if (waitForWiFi("OK", 100, 20) == 0)
        return 0;
         
    ManagedString http = "GET " + page + " HTTP/1.0\r\nHost:" + URL + "\r\n\r\n";
    cmd = "AT+CIPSEND=" +  ManagedString(http.length()) + "\r\n";
    uBit.serial.send(cmd, SYNC_SPINWAIT);
     
    int retry;
    ManagedString s;
    s = "";
    retry = 40;
     
    do {
        uBit.sleep(100);
        s = s + uBit.serial.read(500, ASYNC);
        retry--;
    } while (find(">", s) == 0 && retry != 0);
     
    uBit.serial.send(http, SYNC_SPINWAIT);
    retry = 100;
     
    do {
        uBit.sleep(100);
        s = s + uBit.serial.read(500, ASYNC);
        retry--;
    } while (s.length() < 500 && retry != 0);
     
    if (DEBUG)debug("\n\rPage\n\r" + s + "\n\r");
    return 1;
}
 
int startServerWiFi() { 
    uBit.serial.send("AT+CIPMUX=1\r\n", SYNC_SPINWAIT);
    if (waitForWiFi("OK", 100, 20) == 0) return 0;
    uBit.serial.send("AT+CIPSERVER=1,80\r\n",
                                      SYNC_SPINWAIT);
    if (waitForWiFi("OK", 100, 20) == 0) return 0;
    ManagedString s;
    for (;;) {
        s="";
        do {
         uBit.sleep(100);
         if(s>500)s="";
         s = uBit.serial.read(128, ASYNC);
        } while (find("+IPD", s) == 0);
     
        if (DEBUG)debug("\n\rClient Connected\n\r" + s + "\n\r");
             
        int b = find("+IPD", s);
        s = s.substring(b + 1, s.length());
        b = find(",", s);
        s = s.substring(b + 1, s.length());
        b = find(",", s);
        ManagedString id = s.substring(0, b);
         
        if (DEBUG)debug("\n\rTCP id:" + id + "\n\r");
             
        ManagedString headers = "HTTP/1.0 200 OK\r\n";
        headers = headers + "Server: micro:bit\r\n";
        headers = headers + "Content-type: text/html\r\n\r\n";
        // ManagedString html = "<html><head><title>Temperature</title></head><body>{\"humidity\":82%,\"airtemperature\":23.5C}</p></body></html>\r\n";
         
        char html [100];
        sprintf (html, "<html><head><title>Temperature</title></head><body>{\"airtemperature\":%dC}</p></body></html>\r\n", uBit.thermometer.getTemperature());
         
        //ManagedString html = buffer;
        ManagedString data = headers + html;
        ManagedString cmd = "AT+CIPSEND=" + id + "," + ManagedString(data.length()) + "\r\n";
        uBit.serial.send(cmd, SYNC_SPINWAIT);
         
        s = "";
        int retry = 40;
         
        do {
            uBit.sleep(100);
            s = s + uBit.serial.read(128, ASYNC);
            retry--;
        } while (find(">", s) == 0 && retry != 0);
     
        uBit.serial.send(data, SYNC_SPINWAIT);
        if (waitForWiFi("OK", 100, 100) == 0) return 0;
             
        if (DEBUG)debug("\n\rData Sent\n\r");
             
        cmd = "AT+CIPCLOSE=" + id + "\r\n";
        uBit.serial.send(cmd, SYNC_SPINWAIT);
        if (waitForWiFi("OK", 100, 100) == 0) return 0;
    }
}
 
void debug(ManagedString s) {
    uBit.serial.redirect(USBTX, USBRX);
    uBit.serial.send(s, SYNC_SPINWAIT);
    uBit.serial.redirect(Tx, Rx);
}
 
int find(ManagedString c, ManagedString s) {
    int i;
    for (i = 0; i < (s.length() - c.length()); i++) {
        if (c == s.substring(i, c.length())) break;
    }
    if (i == (s.length() - c.length())) return 0;
    return i;
}
/*
ManagedString waitForWiFiString(ManagedString target, int retry, int pause) {
    ManagedString s;
    do {
        uBit.sleep(pause);
        if(s.length()>128)s="";
        s = s + uBit.serial.read(128, ASYNC); retry--;
    } while (find(target, s) == 0 && retry != 0);
     
    if (DEBUG)debug("\n\r" + s + "\n\r");
    return s;
}
*/

int waitForWiFi(ManagedString target, int retry, int pause) {
    ManagedString s;
    do {
        uBit.sleep(pause);
        if(s.length()>128)s="";
        s = s + uBit.serial.read(128, ASYNC); retry--;
    } while (find(target, s) == 0 && retry != 0);
     
    if (DEBUG)debug("\n\r" + s + "\n\r");
    return retry;
}