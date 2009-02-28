/*********************************************************************************/
/*!
@file           Cfg.h

@brief          Contains all the configuration Information.

@author         L. J. Barman

    Copyright (c)   2008, L. J. Barman, all rights reserved

    This file is part of the PianoBooster application

    PianoBooster is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PianoBooster is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PianoBooster.  If not, see <http://www.gnu.org/licenses/>.

*/
/*********************************************************************************/

#ifndef __CFG_H__
#define __CFG_H__

class CColour
{
public:
    CColour() { red = green = blue = 0; }


    CColour(double r, double g, double b)
    {
        red = static_cast<float>(r);
        green = static_cast<float>(g);
        blue = static_cast<float>(b);
    }
    float red, green, blue;

    bool operator==(CColour colour)
    {
        if (red == colour.red && green == colour.green && blue == colour.blue)
            return true;
        return false;
    }
};

#define YELLOW_BACKGROUND   0
#define BLACK_BACKGROUND    1
/*!
 * @brief   Contains all the configuration Information.
 */
class Cfg
{
public:
    static const float staveStartX()         {return 20;}
    static const float staveEndX()           {return m_staveEndX;}
    static const float playZoneX()           {return scrollStartX() + ( staveEndX() - scrollStartX())* 0.4f;}
    static const float clefX()               {return staveStartX() + 20;}
    static const float timeSignatureX()       {return clefX() + 25;}
    static const float keySignatureX()       {return timeSignatureX() + 25;}
    static const float scrollStartX()        {return keySignatureX() + 64;}
    static const float pianoX()              {return 25;}

    static const float staveThickness()      {return 1;}

    static const int playZoneEarly()     {return m_playZoneEarly;} // 20 Was 25
    static const int playZoneLate()      {return m_playZoneLate;}
    static const int silenceTimeOut()    {return 8000;} // the time in msec before everything goes quiet
    static const int chordNoteGap()      {return 10;} // all notes in a cord must be spaced less than this a gap


    static const CColour menuColour()        {return CColour(0.1, 0.6, 0.6);}
    static const CColour menuSelectedColour(){return CColour(0.7, 0.7, 0.1);}

#if BLACK_BACKGROUND
    static const CColour staveColour()           {return CColour(0.1, 0.7, 0.1);} // green
    static const CColour staveColourDim()        {return CColour(0.2, 0.5, 0.2);} // grey
    static const CColour noteColour()            {return CColour(0.1, 0.9, 0.1);} // green
    static const CColour noteColourDim()         {return CColour(0.3, 0.6, 0.3);} // green
    //static const CColour playedGoodColour()    {return CColour(0.6, 0.6, 1.0);} // grey
    static const CColour playedGoodColour()      {return CColour(0.5, 0.6, 1.0);} // purple 0.6, 0.6, 1.0
    static const CColour playedBadColour()       {return CColour(0.8, 0.3, 0.8);} // orange 0.7, 0.0, 0.0
    static const CColour playedStoppedColour()   {return CColour(1.0, 0.8, 0.0);} // bright orange
    static const CColour backgroundColour()      {return CColour(0.0, 0.0, 0.0);} // black
    static const CColour barMarkerColour()       {return CColour(0.5, 0.5, 0.5);} // grey
    static const CColour beatMarkerColour()       {return CColour(0.3, 0.3, 0.3);} // grey
#endif

#if YELLOW_BACKGROUND
    //static const CColour staveColour()         {return CColour(0.0, 0.0, 0.0);} // black
    //static const CColour staveColour()         {return CColour(0.0, 1.0, 0.0);} // green
    static const CColour staveColour()         {return CColour(0.6, 0.6, 1.0);} //purple
    static const CColour noteColour()         {return CColour(0.6, 0.6, 1.0);} //purple
    static const CColour playedGoodColour()       {return CColour(0.6, 0.6, 1.0);} //purple
    static const CColour playedBadColour()       {return CColour(0.7, 0.3, 0.0);} // orange
    static const CColour playedStoppedColour()   {return CColour(1.0, 0.6, 0.0);} // orange
    static const CColour barMarkerColour()       {return CColour(0.9, 0.9, 1.0);} // grey
    static const CColour beatMarkerColour()       {return CColour(0.8, 0.8, 1.0);} // grey

#endif


    static void setStaveEndX(float x)
    {
         m_staveEndX = x;
    }
    static int getAppX(){return m_appX;}
    static int getAppY(){return m_appY;}
    static int getAppWidth(){return m_appWidth;}
    static int getAppHeight(){return m_appHeight;}
    static void setAppDimentions(int x, int y,int width, int height)
    {
        m_appX = x;
        m_appY = y;
        m_appWidth = width;
        m_appHeight = height;
    }



    static int defualtWrongPatch() {return 7;} // Starts at 1
    static int defualtRightPatch() {return 1;} // Starts at 1

    static int logLevel;
    static bool smallScreen;
    static bool quickStart;
    static bool experimentalTempo;
    static bool experimentalSwapInterval;

private:
    static float m_staveEndX;
    static int m_appX, m_appY, m_appWidth, m_appHeight;
    static const int m_playZoneEarly;
    static const int m_playZoneLate;


};

#endif //__CFG_H__
