/*********************************************************************************/
/*!
@file           Piano.cpp

@brief          xxx.

@author         L. J. Barman

    Copyright (c)   2008-2009, L. J. Barman, all rights reserved

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

#include <cstring>
#include "Util.h"
#include "Piano.h"
#include "Cfg.h"
#include "StavePosition.h"
#include "Settings.h"

#define PIANO_LINE_LENGTH_LONG   64
#define PIANO_LINE_LENGTH_SHORT   48

static const float minNameGap = 14.0;


void CPiano::spaceNoteBunch(unsigned int bottomIndex, unsigned int topIndex)
{
    unsigned int midPoint;
    int i;
    float lastY;
    float gap;

    int range;

    range = topIndex - bottomIndex;
    if (range <= 1)
        return;

    midPoint = range/2 + bottomIndex;

    if (range%2 == 0) // test for an even number of notes
    {
        gap = m_noteNameList[midPoint].posY - m_noteNameList[midPoint -1].posY;
        if  (gap < minNameGap)
        {
            gap = (minNameGap - gap)/2;
            m_noteNameList[midPoint].posY += gap; // then move the middle two notes apart by the same amout
            m_noteNameList[midPoint-1].posY -= gap;
        }
    }

    lastY = m_noteNameList[midPoint].posY;

    // Search from the middle upwads
    for (i = midPoint + 1; i < static_cast<int>(topIndex); i++)
    {
        gap =  m_noteNameList[i].posY - lastY;
         // If the gap is too small make it bigger
        if (gap < minNameGap)
             m_noteNameList[i].posY = lastY +  minNameGap;
        lastY =  m_noteNameList[i].posY;
    }

    lastY = m_noteNameList[midPoint].posY;

    // now go the other way
    for (i = midPoint - 1; i >= 0; i--)
    {
        gap =  lastY - m_noteNameList[i].posY;
        if (gap < minNameGap)
             m_noteNameList[i].posY = lastY - minNameGap;
        lastY =  m_noteNameList[i].posY;
    }
}

void CPiano::drawPianoInputNoteNames()
{
    unsigned int i;

    if (m_noteNameListLength >8) // too many notes so don't name any of them
        return;

    for (i = 0; i < m_noteNameListLength; i++)
    {
        drawNoteName(m_noteNameList[i].pitch, Cfg::playZoneX() - PIANO_LINE_LENGTH_SHORT - 14, m_noteNameList[i].posY, m_noteNameList[i].type);
    }
}

void CPiano::drawPianoInputLines(CChord* chord, CColour colour, int lineLength)
{
    int i;

    drColour(colour);

    CStavePos stavePos;

    for ( i = 0; i < chord->length(); i++)
    {
        int pitch = chord->getNote(i).pitch();
        stavePos.notePos(chord->getNote(i).part(), pitch);
        if (stavePos.getStaveIndex() >= MAX_STAVE_INDEX || stavePos.getStaveIndex() <= MIN_STAVE_INDEX )
            continue;

        glLineWidth (3.0);

        if (stavePos.getAccidental() != 0)
        {
            glEnable (GL_LINE_STIPPLE);
            glLineStipple (1, 0x0f0f);  /*  dashed  */
            glLineWidth (3.0);
        }

        float posY;
        posY = stavePos.getPosYAccidental();

        oneLine(Cfg::playZoneX() - lineLength, posY, Cfg::playZoneX(), posY);
        glDisable (GL_LINE_STIPPLE);
    }
}

void CPiano::spaceNoteNames()
{
    unsigned int bottomIndex;
    unsigned int topIndex;
    bool foundBunch = false;

    m_noteNameList[0].posY = m_noteNameList[0].posYOriginal;

    if (m_noteNameListLength <= 1)
        return;

    bottomIndex = m_noteNameListLength;
    topIndex = 0;
    foundBunch = false;
    for (unsigned int i=1; i < m_noteNameListLength; i++)
    {
        m_noteNameList[i].posY = m_noteNameList[i].posYOriginal;
        if (m_noteNameList[i].posY - m_noteNameList[i-1].posY < minNameGap)
        {
            if (bottomIndex>i) bottomIndex = i;
            if (topIndex<i) topIndex = i;
            foundBunch = true;
        }
        else
        {
            if (foundBunch == true)
                spaceNoteBunch(bottomIndex -1, topIndex +1);

            // reset everything ready for the next bunch
            bottomIndex = m_noteNameListLength;
            topIndex = 0;
            foundBunch = false;
        }
    }
    if (foundBunch == true)
        spaceNoteBunch(bottomIndex -1, topIndex +1);

    // finally try again incase moveing things have created a fresh bunch
    spaceNoteBunch(0, m_noteNameListLength);
}

void CPiano::addNoteNameItem(float posY, int pitch, int type)
{
    noteNameItem_t  noteNameItem;
    int i;

    if (m_noteNameListLength >= arraySize(m_noteNameList))
        return;

    noteNameItem.posY = noteNameItem.posYOriginal = posY;
    noteNameItem.type = type;
    noteNameItem.pitch = pitch;

    // Sort the entries low to high
    for (i = m_noteNameListLength - 1; i >= 0; i--)
    {
        if (m_noteNameList[i].pitch <= pitch)
            break;
        // move the previous entry up one possition
        m_noteNameList[i+1] = m_noteNameList[i];
    }
    if (m_noteNameList[i].pitch == pitch)
        return; // ignore duplicates
    m_noteNameList[i+1] = noteNameItem;
    m_noteNameListLength++;
    spaceNoteNames();
}

void CPiano::addPianistNote(whichPart_t part, int note, bool good)
{
    CStavePos stavePos;
    float posY;

    if (good == true)
        m_goodChord.addNote(part, note);
    else
        m_badChord.addNote(part, note);

    stavePos.notePos(part, note);
    posY = stavePos.getPosYAccidental();
    addNoteNameItem(posY, note, 0);
}

void CPiano::removeNoteNameItem(int pitch)
{
    unsigned int i;
    bool foundMatch = false;

    for (i=0; i < m_noteNameListLength; i++)
    {
        if (foundMatch == true)
            m_noteNameList[i-1] = m_noteNameList[i]; // found a match so move every thing else up

        if (m_noteNameList[i].pitch == pitch)
            foundMatch = true;
    }
    if (m_noteNameListLength>0)
        m_noteNameListLength--;
    spaceNoteNames();
}

// retruns true only if the note is in the bad note list
bool CPiano::removePianistNote(int note)
{
    removeNoteNameItem( note);
    m_goodChord.removeNote(note);
    return m_badChord.removeNote(note);
}
void CPiano::noteNameListClear()
{
    m_noteNameListLength = 0;
}

// Counts the number of notes the pianist has down
int CPiano::pianistAllNotesDown()
{
    return m_goodChord.length() + m_badChord.length();
}

int CPiano::pianistBadNotesDown()
{

    return m_badChord.length();
}

void CPiano::clear()
{
    m_goodChord.clear();
    m_badChord.clear();
    noteNameListClear();
}

void CPiano::drawPianoInput()
{
    bool showNoteName = m_settings->showNoteNames();
    int lineLength = (showNoteName) ? PIANO_LINE_LENGTH_SHORT : PIANO_LINE_LENGTH_LONG;

    if (m_goodChord.length() > 0)
        drawPianoInputLines(&m_goodChord, Cfg::pianoGoodColour(), lineLength);

    if (m_badChord.length() > 0)
        drawPianoInputLines(&m_badChord, Cfg::pianoBadColour(), lineLength);

    if (showNoteName)
        drawPianoInputNoteNames();
}
