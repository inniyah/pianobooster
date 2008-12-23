/*********************************************************************************/
/*!
@file           TrackList.h

@brief          xxxx.

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

#ifndef __TRACK_LIST_H__
#define __TRACK_LIST_H__

#include <QString>
#include <QList>
#include "MidiEvent.h"

#define CONVENTION_LEFT_HAND_CHANNEL (3-1)
#define CONVENTION_RIGHT_HAND_CHANNEL (4-1)
#define GM_PIANO_PATCH        0 // The default grand piano sound

class CSong;
class QListWidget;

class CTrackListItem
{
public:
    QString name;
    int midiChannel;
};


class CTrackList
{
public:
    CTrackList()
    {
        m_song = 0;
        m_trackListWidget = 0;
        clear();
    }

    void init(CSong* songObj, QListWidget* trackListWidget);

    void refresh();
    void clear();

    void currentRowChanged(int currentRow);
    void examineMidiEvent(CMidiEvent event);
    bool hasPianoPart();
    int guessKeySignature(int chanA, int chanB);
    static QString getProgramName(int program);


private:
    QString getChannelProgramName(int chan);

    CSong* m_song;
    QListWidget* m_trackListWidget;
    QList<CTrackListItem> m_trackQtList;
    bool m_midiActiveChannels[MAX_MIDI_CHANNELS];
    int m_midiFirstPatchChannels[MAX_MIDI_CHANNELS];
    int m_noteFrequency[MAX_MIDI_CHANNELS][MAX_MIDI_NOTES];
};

#endif //__TRACK_LIST_H__
