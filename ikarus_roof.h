/*
 INDI Ikarus Roof driver.
 
 Contruction Information: https://ikarusobs.com/about/construction.html

 Copyright (C) 2015-2020 Jasem Mutlaq (mutlaqja@ikarustech.com) 

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

#ifndef IKARUSROOF_H
#define IKARUSROOF_H

#include <indiccd.h>
#include <iostream>

#include <indidome.h>

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>

class IkarusRoof : public INDI::Dome
{

    public:
        IkarusRoof();
        virtual ~IkarusRoof();

        virtual bool initProperties();
        const char *getDefaultName();
        bool updateProperties();
        
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

      protected:

        bool Connect();
        bool Disconnect();
        void TimerHit();
        
        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);
        virtual IPState Park();
        virtual IPState UnPark();                
        virtual bool Abort();

        virtual bool getLimitSwitchStatus();
        virtual bool getFullOpenedLimitSwitch();
        virtual bool getFullClosedLimitSwitch();

        virtual bool sendRelayCommand(DomeDirection dir, DomeMotionCommand operation);

    private:

        ISState fullOpenLimitSwitch;
        ISState fullClosedLimitSwitch;
        
        ISwitch ACControlS[2];
        ISwitchVectorProperty ACControlSP;
        
        bool open_dir_change, close_dir_change;

        double MotionRequest;

        bool SetupParms();
        
        // Turn on/off observatory AC
        void setAC(bool enable);
};

#endif
