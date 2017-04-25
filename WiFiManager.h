/**************************************************************
   WiFiManager is a library for the ESP8266/Arduino platform
   (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal
   inspired by:
   http://www.esp8266.com/viewtopic.php?f=29&t=2520
   https://github.com/chriscook8/esp-arduino-apboot
   https://github.com/esp8266/Arduino/tree/esp8266/hardware/esp8266com/esp8266/libraries/DNSServer/examples/CaptivePortalAdvanced
   Built by AlexT https://github.com/tzapu
   Licensed under MIT license
 **************************************************************/

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#define FW_VERSION F("v1.0.1")

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <memory>

extern "C" {
  #include "user_interface.h"
}


const char HTTP_HEAD[] PROGMEM            = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><meta name=\"author\" content=\"Alex Fedorov AKA FedorComander fedor@anuta.org\"/><title>{v}</title>";
const char HTTP_STYLE[] PROGMEM           = "<style>.error {color: #ad0d0d; width: 100%; text-align: center; margin-bottom: 10px; padding: 0px;} a { text-decoration: none; color:#f2ffff;} a:hover { text-decoration: underline; } a:visited { text-decoration:none; color:#f2ffff}  h1 {font-size:20px; text-align:center;} .c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:95%;} body{background-color:#000; text-align: center;font-family:verdana;color:#f2ffff} button{border:0;border-radius:0.3rem;background-color:#ad0d0d;color:#f2ffff;line-height:2.4rem;font-size:1.2rem;width:100%;} button:hover{ background-color: #d01414; } .q{float: right;width: 64px;text-align: right;} .l{background: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAPElEQVR42mNgoAUoZmL6v5qPDwWDxMjWTJIh2BQjG0q0AcSK084AZC8UMTLuI2gAvsAjKjBHDRgWBpALAOpN3snwpI+zAAAAAElFTkSuQmCC\") no-repeat left center;background-size: 1em;}</style>";
const char HTTP_SCRIPT[] PROGMEM          = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char HTTP_HEAD_END[] PROGMEM        = "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";
const char HTTP_PORTAL_OPTIONS[] PROGMEM  = "<form action=\"/wifi\" method=\"get\"><button>Join WIFI Network</button></form><br/><form action=\"/ap\" method=\"get\"><button>Create WIFI Network</button></form><br/><form action=\"/update\" method=\"get\"><button>Firmware Update</button></form><br/><form action=\"/r\" method=\"post\"><button>Factory Reset</button></form><br/><div style='width:100%; text-align: center; font-size:12px; color: #444'>Firmware {fv} by FedorComander</div>";
const char HTTP_ITEM[] PROGMEM            = "<div><a href='#p' onclick='c(this)'>{v}</a>&nbsp;<span class='q {i}'>{r}%</span></div>";
const char HTTP_FORM_START[] PROGMEM      = "<form method='get' action='wifisave'><input id='s' name='s' length=32 placeholder='SSID'><br/><input id='p' name='p' length=64 type='password' placeholder='PASSWORD'><br/>";
const char HTTP_FORM_PARAM[] PROGMEM      = "<br/><input id='{i}' name='{n}' length={l} placeholder='{p}' value='{v}' {c}>";
const char HTTP_FORM_END[] PROGMEM        = "<br/><button type='submit'>Save</button></form>";
const char HTTP_SCAN_LINK[] PROGMEM       = "<br/><div class=\"c\"><a href=\"/wifi\">Scan</a></div>";
const char HTTP_SAVED[] PROGMEM           = "<div>Credentials Saved<br />Trying to connect KISS-WIFI adapter to the client network.<br />If it fails reconnect to KISS-WIFI access point to try again</div>";
const char HTTP_END[] PROGMEM             = "</div></body></html>";
const char HTTP_FORM_START_AP[] PROGMEM   = "<form method='get' action='apsave'><input type='password' id='p1' name='p1' length=64 placeholder='PASSWORD' value='{p1}'><br/><input id='p2' name='p2' length=64 type='password' placeholder='RETYPE PASSWORD' value='{p2}'><br/>";
const char HTTP_SAVED_AP[] PROGMEM        = "<div style='width:100%; text-align:center'>AP password has been saved.<br /><br />When KISS-WIFI runs in AP mode, you can connect to configuration tool from any browser using following url <br/><br/><a href='http://kiss.fc'>http://kiss.fc</a></div>";
const char HTTP_UPDATE_FORM[] PROGMEM     = "<form method='POST' action='' enctype='multipart/form-data'><input type='file' name='update' style='width:160px'/><button>Update</button></form>";
const char HTTP_UPDATE_SUCCESS[] PROGMEM  = "<div style='width:100%; text-align:center'>Update complete. Rebooting.</div>";
const char HTTP_UPDATE_REFRESH[] PROGMEM  = "<meta http-equiv=\"refresh\" content=\"15;URL=/\">";

#define WIFI_MANAGER_MAX_PARAMS 10

class WiFiManagerParameter {
  public:
    WiFiManagerParameter(const char *custom);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom);

    const char *getID();
    const char *getValue();
    const char *getPlaceholder();
    int         getValueLength();
    const char *getCustomHTML();
  private:
    const char *_id;
    const char *_placeholder;
    char       *_value;
    int         _length;
    const char *_customHTML;

    void init(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom);

    friend class WiFiManager;
};


class WiFiManager
{
  public:
    WiFiManager();

    boolean       autoConnect(char const *apName, char const *apPassword = NULL);

    // get the AP name of the config portal, so it can be used in the callback
    String        getConfigPortalSSID();

    void          resetSettings();

    //sets timeout before webserver loop ends and exits even if there has been no setup.
    //usefully for devices that failed to connect at some point and got stuck in a webserver loop
    //in seconds setConfigPortalTimeout is a new name for setTimeout
    void          setConfigPortalTimeout(unsigned long seconds);
    void          setTimeout(unsigned long seconds);

    //sets timeout for which to attempt connecting, usefull if you get a lot of failed connects
    void          setConnectTimeout(unsigned long seconds);


    void          setDebugOutput(boolean debug);
    //defaults to not showing anything under 8% signal quality if called
    void          setMinimumSignalQuality(int quality = 8);
    //sets a custom ip /gateway /subnet configuration
    void          setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);
    //sets config for a static IP
    void          setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);
    //called when AP mode and config portal is started
    void          setAPCallback( void (*func)(WiFiManager*) );
    //called when AP mode and config portal is working...
    void          setConfigCallback( void (*func)(void) );
    //called when settings have been changed and connection was successful
    void          setSaveConfigCallback( void (*func)(void) );
    //adds a custom parameter
    void          addParameter(WiFiManagerParameter *p);
    //if this is set, it will exit after config, even if connection is unsucessful.
    void          setBreakAfterConfig(boolean shouldBreak);
    //if this is set, try WPS setup when starting (this will delay config portal for up to 2 mins)
    //TODO
    //if this is set, customise style
    void          setCustomHeadElement(const char* element);
    //if this is true, remove duplicated Access Points - defaut true
    void          setRemoveDuplicateAPs(boolean removeDuplicates);
    // Separate setup web server
    
    void startWeb();
    void loop();
    void readEEPROM();
    void writeEEPROM();
    String getAPPassword();
  
 
  private:
    std::unique_ptr<DNSServer>        dnsServer;
    std::unique_ptr<ESP8266WebServer> server;
    
    String apPassword = "kisskiss";

    void          setupConfigPortal();
   
    void          startWPS();

    const char*   _apName                 = "no-net";
    const char*   _apPassword             = NULL;
    String        _ssid                   = "";
    String        _pass                   = "";
    unsigned long _configPortalTimeout    = 0;
    unsigned long _connectTimeout         = 0;
    unsigned long _configPortalStart      = 0;

    IPAddress     _ap_static_ip;
    IPAddress     _ap_static_gw;
    IPAddress     _ap_static_sn;
    IPAddress     _sta_static_ip;
    IPAddress     _sta_static_gw;
    IPAddress     _sta_static_sn;

    int           _paramsCount            = 0;
    int           _minimumQuality         = -1;
    boolean       _removeDuplicateAPs     = true;
    boolean       _shouldBreakAfterConfig = false;
    boolean       _tryWPS                 = false;

    const char*   _customHeadElement      = "";

    int           status = WL_IDLE_STATUS;
    int           connectWifi(String ssid, String pass);
    uint8_t       waitForConnectResult();

    void          handleRoot();
    void          handleWifi(boolean scan);
    void          handleWifiSave();
    void          handleInfo();
    void          handleReset();
    void          handleNotFound();
    void          handle204();
    void          handleAP();
    void          handleAPSave();
    void          handleUpdateForm();
    boolean       captivePortal();
    

    // DNS server
    const byte    DNS_PORT = 53;

    //helpers
    int           getRSSIasQuality(int RSSI);
    boolean       isIp(String str);
    String        toStringIp(IPAddress ip);

    boolean       connect;
    boolean       _debug = true;

    void (*_apcallback)(WiFiManager*) = NULL;
    void (*_savecallback)(void) = NULL;
    void (*_configcallback)(void) = NULL;

    WiFiManagerParameter* _params[WIFI_MANAGER_MAX_PARAMS];

    template <typename Generic>
    void          DEBUG_WM(Generic text);

    template <class T>
    auto optionalIPFromString(T *obj, const char *s) -> decltype(  obj->fromString(s)  ) {
      return  obj->fromString(s);
    }
    auto optionalIPFromString(...) -> bool {
      DEBUG_WM("NO fromString METHOD ON IPAddress, you need ESP8266 core 2.1.0 or newer for Custom IP configuration to work.");
      return false;
    }
};

#endif
