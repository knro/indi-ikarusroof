/*
 INDI Ikarus Roof driver.
 
 Contruction Information: https://ikarusobs.com/about/construction.html

 Copyright (C) 2015-2020 Jasem Mutlaq (mutlaqja@ikarustech.com) 

 Motor Open and Close commands are sent to a web-enabled relay using Curl
 Limit Switches are conntected to mains to cut off the power to the motor
 once actuated. The limit switches are NC (Normally Closed) meaning that power
 is ON when they are NOT actutated. We detect if a limit switch is actutated once the 
 power is CUT OFF. Two phone chargers are connected to the limit switches. They act as a 
 poor-man digital sensor. They output 5v when on and using a simple voltage divider, they are 
 connected to Raspberry PI GPIO v3.3 inputs.
 
 1. Roof Fully Closed (Parked): CLOSE Limit Switch is OFF --> CLOSE Charger OFF --> CLOSE GPIO LOW --> Parked
 2. Roof Fully Opened (Unparked): OPEN Limit Switch is OFF --> OPEN Charger OFF --> OPEN GPIO LOW --> Unparked
 3. Roof in between #1 and #2: CLOSE and OPEN limit switches are BOTH ON --> Both Charges ON --> Both GPIOs ON --> Unknown State
 
 The driver also control an Air Conditioner unit (AC). The AC is turned off when unparking the dome and is turned on again when parking is complete.
 A solid state relay is connected to the AC mains and is controlled from Raspberry PI GPIO.
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <indicom.h>

#include <wiringPi.h>

#include <curl/curl.h>

#include "config.h"

#include "ikarus_roof.h"

std::unique_ptr<IkarusRoof> myroof(new IkarusRoof());

// GPIO PINS
#define FULL_OPEN_PIN   19
#define FULL_CLOSED_PIN 12
#define AC_PIN          16

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

char * escapeXML(const char *s, unsigned int MAX_BUF_SIZE)
{
        char *buf = (char *) malloc(sizeof(char)*MAX_BUF_SIZE);
        char *out = buf;
        unsigned int i=0;

        for (i=0; i <= strlen(s); i++)
        {
            switch (s[i])
            {
                case '&':
                    strncpy(out, "&amp;", 5);
                    out+=5;
                    break;
                case '\'':
                    strncpy(out, "&apos;", 6);
                    out+=6;
                    break;
                case '"':
                    strncpy(out, "&quot;", 6);
                    out+=6;
                    break;
                case '<':
                    strncpy(out, "&lt;", 4);
                    out+=4;
                    break;
                case '>':
                    strncpy(out, "&gt;", 4);
                    out+=4;
                    break;
                default:
                    strncpy(out++, s+i, 1);
                    break;
            }

        }

        return buf;
}

void ISGetProperties(const char *dev)
{
        myroof->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        myroof->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
       myroof->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
       myroof->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    myroof->ISSnoopDevice(root);
}

IkarusRoof::IkarusRoof()
{
  fullOpenLimitSwitch   = ISS_OFF;
  fullClosedLimitSwitch = ISS_OFF;
  
  wiringPiSetupGpio() ;
  
  pinMode (FULL_OPEN_PIN, INPUT) ;
  pinMode (FULL_CLOSED_PIN, INPUT) ;
  
  // Air Conditioner Pin
  pinMode (AC_PIN, OUTPUT) ;

   SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
   
   setVersion(INDI_IKARUSROOF_VERSION_MAJOR, INDI_IKARUSROOF_VERSION_MINOR);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_NONE);
    
    IUFillSwitch(&ACControlS[0], "On", "", ISS_OFF);
    IUFillSwitch(&ACControlS[1], "Off", "", ISS_OFF);
    IUFillSwitchVector(&ACControlSP, ACControlS, 2, getDeviceName(), "AC_CONTROL", "AC", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    addAuxControls();

    return true;
}

bool IkarusRoof::SetupParms()
{
    // Check parking data
    InitPark();

    getLimitSwitchStatus();
    getLimitSwitchStatus();
    getLimitSwitchStatus();

    IUResetSwitch(&ACControlSP);
    if (digitalRead(AC_PIN))
        ACControlS[0].s = ISS_ON;
    else
        ACControlS[1].s = ISS_ON;
        
    // If both limit switches are off, we don't have parking state
    if (fullClosedLimitSwitch == ISS_OFF && fullOpenLimitSwitch == ISS_OFF)
    {
        ParkSP.s = IPS_IDLE;
        IUResetSwitch(&ParkSP);
        IDSetSwitch(&ParkSP, NULL);
        DEBUG(INDI::Logger::DBG_WARNING, "Parking status is unknown.");
    }
    // If is unparked but limit switch status indicates parked, we set it to parked.
    else if (fullClosedLimitSwitch == ISS_ON)
        SetParked(true);
    // If parked but limit switch status indicates unparked, we set it as thus
    else if (fullOpenLimitSwitch == ISS_ON)
        SetParked(false);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/

bool IkarusRoof::Connect()
{
    SetTimer(POLLMS);     //  start the timer
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
IkarusRoof::~IkarusRoof()
{

}

/************************************************************************************
 *
* ***********************************************************************************/
const char * IkarusRoof::getDefaultName()
{
        return "Ikarus Roof";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetupParms();
        
        defineSwitch(&ACControlSP);
    }
    else
        deleteProperty(ACControlSP.name);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::Disconnect()
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
void IkarusRoof::TimerHit()
{
    if(isConnected() == false)
        return;

   getLimitSwitchStatus();

   if (DomeMotionSP.s == IPS_BUSY)
   {
       // Roll off is opening
       if (DomeMotionS[DOME_CW].s == ISS_ON)
       {
           if (getFullOpenedLimitSwitch())
           {
               DEBUG(INDI::Logger::DBG_SESSION, "Roof is open.");
               sendRelayCommand(DOME_CW, MOTION_STOP);
               SetParked(false);
           }
       }
       // Roll Off is closing
       else if (DomeMotionS[DOME_CCW].s == ISS_ON)
       {
           if (getFullClosedLimitSwitch())
           {
               DEBUG(INDI::Logger::DBG_SESSION, "Roof is closed.");
               sendRelayCommand(DOME_CCW, MOTION_STOP);
               SetParked(true);
               
               // Turn on AC
               setAC(true);
           }
       }      
   }
   else
   {
       // Case #1 Both switches are on which is impossible, so we ignore this
       if (getFullClosedLimitSwitch() && getFullClosedLimitSwitch())
       {
           SetTimer(POLLMS);
           return;
       }
           
       // Case #2 Unparked but Limit Switch indicates fully closed
       if (ParkS[0].s == ISS_OFF && getFullClosedLimitSwitch())
       {
           SetParked(true);
       }
       // Case #3 Roof is parked but limit switch indicates fully open
       else if (ParkS[1].s == ISS_OFF && getFullOpenedLimitSwitch())
       {
           SetParked(false);
       }
       // Case #4 Roof is either fully closed/open but both limit switches are off
       else if ( (ParkS[0].s == ISS_ON || ParkS[1].s == ISS_ON) &&
                getFullOpenedLimitSwitch() == false && getFullClosedLimitSwitch() == false)
       {
           IUResetSwitch(&ParkSP);
           IDSetSwitch(&ParkSP, NULL);
           DEBUG(INDI::Logger::DBG_SESSION, "Roof was opened manually. Park state unknown.");
       }
   }

   SetTimer(POLLMS);
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState IkarusRoof::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
        if (dir == DOME_CW && fullOpenLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully opened.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CW && getWeatherState() == IPS_ALERT)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Weather conditions are in the danger zone. Cannot open roof.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && fullClosedLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully closed.");
            return IPS_ALERT;
        }

        fullOpenLimitSwitch   = ISS_OFF;
        fullClosedLimitSwitch = ISS_OFF;

        bool rc = sendRelayCommand(dir, operation);

        if (rc == false)
            return IPS_ALERT;
        else
            return IPS_BUSY;
    }
    else
    {
        return (Dome::Abort() ? IPS_OK : IPS_ALERT);

    }

    return IPS_ALERT;

}

/************************************************************************************
 *
* ***********************************************************************************/
IPState IkarusRoof::Park()
{    
    bool rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Roll off is parking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState IkarusRoof::UnPark()
{
    bool rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc)
    {       
           // Turn of AC
           setAC(false);
           
           DEBUG(INDI::Logger::DBG_SESSION, "Roll off is unparking...");
           return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::Abort()
{
    // If both limit switches are off, then we're neither parked nor unparked.
    if (fullOpenLimitSwitch == false && fullClosedLimitSwitch == false)
    {
        IUResetSwitch(&ParkSP);
        ParkSP.s = IPS_IDLE;
        IDSetSwitch(&ParkSP, NULL);
    }

    // It will stop ALL
    bool rc =  sendRelayCommand(DOME_CW, MOTION_STOP);

    if (rc)
        setDomeState(DOME_IDLE);

    return rc;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::sendRelayCommand(DomeDirection dir, DomeMotionCommand operation)
{
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    char requestURL[MAXRBUF];

    // You must replace username:password with your credentials
    // dinrelay is the host name for the Din Relay. Either use that or an IP address
    // e.g. http://username:password@192.168.1.5
    if (operation == MOTION_STOP)
        strncpy(requestURL, "http://username:password@dinrelay/outlet?a=OFF", MAXRBUF);
    else
    {
        // Open
        if (dir == DOME_CW)
            strncpy(requestURL, "http://username:password@dinrelay/outlet?1=ON", MAXRBUF);
        // Close
        else
            strncpy(requestURL, "http://username:password@dinrelay/outlet?2=ON&3=ON", MAXRBUF);
    }

    curl = curl_easy_init();
    if(curl)
    {
      curl_easy_setopt(curl, CURLOPT_URL, requestURL);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
      res = curl_easy_perform(curl);

      if (res != 0)
      {
          char *error_str = escapeXML(curl_easy_strerror(res), MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "sendRelay error: %s", error_str);
          free(error_str);
          return false;
      }
      curl_easy_cleanup(curl);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::getFullOpenedLimitSwitch()
{
    return (fullOpenLimitSwitch == ISS_ON);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::getFullClosedLimitSwitch()
{
    return (fullClosedLimitSwitch == ISS_ON);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::getLimitSwitchStatus()
{
    static int prev_open_state=-1, prev_close_state=-1;
    
    // Read Limit Switch Status from Raspberry PI    
    int full_open_state   = -1;
    int full_closed_state = -1; 
    
    full_open_state   = digitalRead (FULL_OPEN_PIN);    
    full_closed_state = digitalRead (FULL_CLOSED_PIN);    
    
    DEBUGF(INDI::Logger::DBG_DEBUG, "full_open_state: %d full_closed_state: %d", full_open_state, full_closed_state);
    
    if (prev_open_state != full_open_state)
    {
        prev_open_state = full_open_state;
        return true;
    }
    
    if (prev_close_state != full_closed_state)
    {
        prev_close_state = full_closed_state;
        return true;
    }
        
    // If ON then limit swtich is OFF (i.e. NOT pressed)
    if (full_open_state)
            fullOpenLimitSwitch = ISS_OFF;
    else
            fullOpenLimitSwitch = ISS_ON;
    
    if (full_closed_state)
            fullClosedLimitSwitch = ISS_OFF;
    else
            fullClosedLimitSwitch = ISS_ON;
    
    
    DEBUGF(INDI::Logger::DBG_DEBUG, "fullOpenLimitSwitch: %s fullClosedLimitSwitch: %s", (fullOpenLimitSwitch == ISS_ON) ? "ON" : "OFF", (fullClosedLimitSwitch == ISS_ON) ? "ON" : "OFF");
    
    return true;
    
}

/************************************************************************************
 *
* ***********************************************************************************/
bool IkarusRoof::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
  if (strcmp(dev, getDeviceName()) == 0)
  {
      if (!strcmp(name, ACControlSP.name))
      {
          for (int i=0; i < n; i++)
          {
              if (!strcmp(names[i], "On") && states[i] == ISS_ON)
              {
                  setAC(true);
                  break;
              }
              else if (!strcmp(names[i], "Off") && states[i] == ISS_ON)
              {
                  setAC(false);
                  break;
              }
                  
          }
          
          return true;
      }
  }
  
  return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
void IkarusRoof::setAC(bool enable)
{
    IUResetSwitch(&ACControlSP);
    
    if (enable)
    {
        digitalWrite(AC_PIN, HIGH);
        DEBUG(INDI::Logger::DBG_SESSION, "AC turned on.");
        ACControlS[0].s = ISS_ON;
    }
    else
    {
        digitalWrite(AC_PIN, LOW);
        DEBUG(INDI::Logger::DBG_SESSION, "AC turned off.");
        ACControlS[1].s = ISS_ON;
    }
    
    ACControlSP.s = IPS_OK;
    IDSetSwitch(&ACControlSP, NULL);
}
