/*********************************************************************************/
/*!
@file           Conductor.cpp

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

#include "Conductor.h"

#if HAS_SCORE
#include "Score.h"
#include "Cfg.h"
#endif

playMode_t CConductor::m_playMode = PB_PLAY_MODE_listen;

CConductor::CConductor()
{
    int i;
#if HAS_SCORE
    m_scoreWin = 0;
#endif

    m_songEventQueue = new CQueue<CMidiEvent>(1000);
    m_wantedChordQueue = new CQueue<CChord>(1000);
    m_savedNoteQueue = new CQueue<CMidiEvent>(200);
    m_savedNoteOffQueue = new CQueue<CMidiEvent>(200);
    m_playing = false;
    m_transpose = 0;
    setSpeed(1.0);
    m_boostVolume = 0;
    m_activeChannel = 0;
    m_leadLagAdjust = 0;
    m_skill = 0;
    m_silenceTimeOut = 0;
    m_realTimeEventBits = 0;
    setPianistChannels(1-1,1-1);

    for ( i = 0; i < MAX_MIDI_CHANNELS; i++)
    {
        m_muteChannels[i] = false;
    }
    setStartTimeSig(0,0);
    rewind();
    testWrongNoteSound(false);
}

CConductor::~CConductor()
{
    delete m_songEventQueue;
    delete m_wantedChordQueue;
    delete m_savedNoteQueue;
    delete m_savedNoteOffQueue;
}


//! add a midi event to be analysed and displayed on the score
void CConductor::midiEventInsert(CMidiEvent event)
{
    m_songEventQueue->push(event);
}

//! first check if there is space to add a midi event
int CConductor::midiEventSpace()
{
    return m_songEventQueue->space();

}
void CConductor::channelSoundOff(int channel)
{
    if (channel < 0 || channel >= MAX_MIDI_CHANNELS)
    {
        return;
    }

    CMidiEvent midi;
    midi.controlChangeEvent(0, channel, MIDI_ALL_NOTES_OFF, 0);
    playMidiEvent(midi);
    // remove the sustain pedal as well
    midi.controlChangeEvent(0, channel, MIDI_SUSTAIN, 0);
    playMidiEvent(midi);
}

void CConductor::allSoundOff()
{
    int channel;

    for ( channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
    {
        if (channel != m_pianistGoodChan)
            channelSoundOff(channel);
    }
    m_savedNoteQueue->clear();
    m_savedNoteOffQueue->clear();
}

void CConductor::resetAllChannels()
{
    int channel;

    CMidiEvent midi;
    for ( channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
    {
        midi.controlChangeEvent(0, channel, MIDI_RESET_ALL_CONTROLLERS, 0);
        playMidiEvent(midi);
    }
}

void CConductor::muteChannel(int channel, bool state)
{
    if (channel < 0 || channel >= MAX_MIDI_CHANNELS)
        return;

    m_muteChannels[ channel] = state;
    if (state == true)
        channelSoundOff(channel); // Fixme this is called too often
}

void CConductor::mutePart(int part, bool state)
{
    int channel;

    if ( part < MAX_MIDI_CHANNELS)
    {
        muteChannel(part, state);
        return;
    }


    for ( channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
    {
        muteChannel( channel, state);
    }

    if (state == true)
        channelSoundOff(channel);
}

/* calculate the new solo_volume */
int CConductor::calcBoostVolume(int channel, int volume)
{
    int returnVolume;
    bool activePart;

    if (volume == -1)
        volume = m_savedMainVolume[channel];
    else
        m_savedMainVolume[channel] = volume;

    if ( m_boostVolume == 0 )
        return volume;
    returnVolume = volume;
    activePart = false;
    if (m_activeChannel == CNote::bothHandsChan())
    {
        if (m_playMode == PB_PLAY_MODE_listen) // only boost one hand in listen mode
        {
            if (channel == CNote::leftHandChan() && CNote::getActiveHand() != PB_PART_right)
                activePart = true;
            if (channel == CNote::rightHandChan() && CNote::getActiveHand() != PB_PART_left)
                activePart = true;
        }
        else // otherwise allways bost both hands
        {
            if (channel == CNote::leftHandChan() || channel == CNote::rightHandChan())
                activePart = true;
        }
    }
    else
    {
        if (channel == m_activeChannel)
            activePart= true;
    }
    if (activePart)
    {
        if (returnVolume == 0 )
            ;               /* Don't adjust the volume if zero */
        else if (m_boostVolume < 0 )
            returnVolume = (returnVolume * (m_boostVolume + 100)) / 100;
        else
            returnVolume += m_boostVolume;
    }
    else
    {
        if (m_boostVolume > 0)
            returnVolume = (returnVolume * (100 - m_boostVolume)) / 100;
    }
    if (returnVolume > 127)
        returnVolume = 127;
    return returnVolume;
}

/* send boost volume by adusting all channels */
void CConductor::outputBoostVolume()
{
    int chan;

    for ( chan =0; chan <MAX_MIDI_CHANNELS; chan++ )
    {
        if (chan == m_pianistGoodChan)
            continue;
        CMidiEvent midi;
        midi.controlChangeEvent(0, chan, MIDI_MAIN_VOLUME, calcBoostVolume(chan,-1));
        playMidiEvent(midi);
    }
}

void CConductor::transpose(int transpose)
{
    if (m_transpose != transpose)
    {
        allSoundOff();
        m_transpose = transpose;
        if (m_transpose >  12)
            m_transpose = 12;
        if (m_transpose < -12)
            m_transpose = -12;
        m_scoreWin->transpose(m_transpose);
    }
}

void CConductor::autoMute()
{
    mutePart(PB_PART_all, false);

    if (m_playMode != PB_PLAY_MODE_listen)
    {
        if (m_activeChannel == CNote::bothHandsChan())
        {
            if (CNote::getActiveHand() == PB_PART_both || CNote::getActiveHand() == PB_PART_left)
                mutePart(CNote::leftHandChan(), true);
            if (CNote::getActiveHand() == PB_PART_both || CNote::getActiveHand() == PB_PART_right)
                mutePart(CNote::rightHandChan(), true);
        }
    }
}

void CConductor::setActiveHand(whichPart_t hand)
{
    if (CNote::getActiveHand() == hand)
        return;
    CNote::setActiveHand(hand);
    autoMute();
    outputBoostVolume();
    m_wantedChord = m_savedwantedChord;

    if (m_wantedChord.trimOutOfRangeNotes(m_transpose)==0)
        fetchNextChord();
    else
    {
        int note;
        int i;

        // Reset the note colours
        for(i = 0; i < m_savedwantedChord.length(); i++)
        {
            note = m_savedwantedChord.getNote(i).pitch();
            m_scoreWin->setPlayedNoteColour(note, Cfg::noteColour(), m_chordDeltaTime);
        }
        for(i = 0; i < m_wantedChord.length(); i++)
        {
            note = m_wantedChord.getNote(i).pitch();
            m_scoreWin->setPlayedNoteColour(note, Cfg::playedStoppedColour(), m_chordDeltaTime);
        }
    }
}

void CConductor::setActiveChannel(int channel)
{
    m_activeChannel = channel;
    outputBoostVolume();
    resetWantedChord();
    fetchNextChord();
    autoMute();
}


void CConductor::playMusic(bool start)
{
    m_playing = start;
    allSoundOff();
    resetAllChannels();
    if (start)
    {
        autoMute();
        CMidiEvent event;

        event.controlChangeEvent(0, m_pianistGoodChan, MIDI_MAIN_VOLUME, 127);
        playMidiEvent(event); // Play the midi note or event

        /*
        const unsigned char gsModeEnterData[] =  {0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};

        for (size_t i = 0; i < arraySize(gsModeEnterData); i++)
        {
            event.collateRawByte(0, gsModeEnterData[i]);
            playMidiEvent(event);
        }
        event.outputCollatedRawBytes(0);
        playMidiEvent(event);
        */
    }
}

void CConductor::playTransposeEvent(CMidiEvent event)
{
    if (m_transpose != 0 && event.channel() != MIDI_DRUM_CHANNEL &&
                (event.type() == MIDI_NOTE_ON || event.type() == MIDI_NOTE_OFF) )
        event.transpose(m_transpose);

    if (event.type() == MIDI_NOTE_ON && m_muteChannels[event.channel()] == true &&
                                CChord::isNotePlayable(event.note(), m_transpose) == true)
        return; // mute the note by not playing it

    // boost any volume events
    if (event.type() == MIDI_CONTROL_CHANGE && event.data1() == MIDI_MAIN_VOLUME)
        event.setDatat2(calcBoostVolume(event.channel(), event.data2() ));

    playMidiEvent(event); // Play the midi note or event
}

void CConductor::outputStavedNotes()
{
    // The saved notes off are note needed any more
    // (as the are also in the savedNoteQueue
    m_savedNoteOffQueue->clear();

    // output any the saved up notes
    while (m_savedNoteQueue->length() > 0)
        playTransposeEvent(m_savedNoteQueue->pop());
}

void CConductor::resetWantedChord()
{
    m_wantedChord.clear();
    m_chordDeltaTime = m_playingDeltaTime;
    m_pianistSplitPoint = MIDDLE_C;
    m_followPlayingTimeOut = CMidiFile::ppqnAdjust(Cfg2::playZoneLate() * SPEED_ADJUST_FACTOR);

    outputStavedNotes();
    m_followState = PB_FOLLOW_searching;
 }


void CConductor::fetchNextChord()
{
    m_followState = PB_FOLLOW_searching;
    m_followPlayingTimeOut = m_cfg_playZoneLate * SPEED_ADJUST_FACTOR;

    outputStavedNotes();

    do // Remove notes or chords that are out of our range
    {
        if (m_wantedChordQueue->length() == 0)
        {
            m_wantedChord.clear();
            m_pianistSplitPoint = MIDDLE_C;
            return;
        }
        m_wantedChord = m_wantedChordQueue->pop();
        m_savedwantedChord = m_wantedChord;
        m_chordDeltaTime -= m_wantedChord.getDeltaTime() * SPEED_ADJUST_FACTOR;
    }
    while (m_wantedChord.trimOutOfRangeNotes(m_transpose)==0);

    // count the good notes so that the live precentage looks OK
    m_rating.totalNotes(m_wantedChord.length());
    setEventBits( EVENT_BITS_forceFullRredraw);

    // now find the split point
    int lowestTreble = MIDDLE_C + 50;
    int highestBase =  MIDDLE_C - 50;
    CNote note;

    // Find where to put the split point
    for(int i = 0; i < m_wantedChord.length(); i++)
    {
        note = m_wantedChord.getNote(i);
        if (note.part() == PB_PART_right && note.pitch() < lowestTreble)
            lowestTreble = note.pitch();
        if (note.part() == PB_PART_left && note.pitch() > highestBase)
            highestBase = note.pitch();
    }

    //put the split point in the middle
    m_pianistSplitPoint = (lowestTreble + highestBase) /2;
}

bool CConductor::validatePianistNote(CMidiEvent& inputNote)
{
    if ( deltaAdjust(m_chordDeltaTime) <= -m_cfg_playZoneEarly)
        return false;

    return m_wantedChord.searchChord(inputNote.note(), m_transpose);
}

bool CConductor::validatePianistChord()
{
    if (m_badNoteLines.length() >= 2)
        return false;

    if (m_skill>=3)
    {
        if (m_goodPlayedNotes.length() == m_wantedChord.length())
            return true;
    }
    else
    {
        if (m_goodPlayedNotes.length() >= 1)
            return true;

    }
    return false;
}

void CConductor::pianistInput(CMidiEvent inputNote)
{
    bool goodSound = true;

    if (m_testWrongNoteSound)
        goodSound = false;

    inputNote.setChannel(m_pianistGoodChan);

    if (inputNote.type() == MIDI_NOTE_ON)
    {
        whichPart_t hand;
        hand = (inputNote.note() >= m_pianistSplitPoint)? PB_PART_right : PB_PART_left;

        if ( validatePianistNote(inputNote) == true)
        {
            m_goodPlayedNotes.addNote(hand, inputNote.note());
            m_goodNoteLines.addNote(hand, inputNote.note());
#if HAS_SCORE
            m_scoreWin->setPlayedNoteColour(inputNote.note(),
                        (m_followPlayingTimeOut)? Cfg::playedGoodColour():Cfg::playedBadColour(),
                        m_chordDeltaTime);
#endif
            if (validatePianistChord() == true)
            {
                m_goodPlayedNotes.clear();
                fetchNextChord();
            }
        }
        else
        {
            if (m_playing == true)
            {
                goodSound = false;

                m_badNoteLines.addNote(hand, inputNote.note());
                m_rating.wrongNotes(1);
            }
            else
                m_goodNoteLines.addNote(hand, inputNote.note());
        }
    }
    else if (inputNote.type() == MIDI_NOTE_OFF)
    {
        m_goodNoteLines.removeNote(inputNote.note());
        m_badNoteLines.removeNote(inputNote.note());
        bool hasNote = m_goodPlayedNotes.removeNote(inputNote.note());

#if HAS_SCORE
        if (hasNote)
            m_scoreWin->setPlayedNoteColour(inputNote.note(),
                    (m_followPlayingTimeOut)? Cfg::noteColour():Cfg::playedStoppedColour(),
                    m_chordDeltaTime);
#endif

        outputSavedNotesOff();
    }

    // change the sound to the right or wrong sound
    int pianoSound = (goodSound == true) ? m_cfg_rightNoteSound : m_cfg_wrongNoteSound;

    if (pianoSound != m_lastSound)
    {
        m_lastSound = pianoSound;

        CMidiEvent midiSound;
        midiSound.programChangeEvent(0,inputNote.channel(),pianoSound);
        playMidiEvent( midiSound );
    }

    playMidiEvent( inputNote );
}

// Counts the number of notes the pianist has down
int CConductor::pianistNotesDown()
{
    return m_goodNoteLines.length() + m_badNoteLines.length();
}

void CConductor::followPlaying()
{

    if ( m_playMode == PB_PLAY_MODE_listen )
        return;

    if (m_wantedChord.length() == 0)
        fetchNextChord();

    if (m_wantedChord.length() == 0)
        return;

    if (deltaAdjust(m_chordDeltaTime) > -m_cfg_earlyNotesPoint )
        m_followState = PB_FOLLOW_earlyNotes;

    if ( m_playMode == PB_PLAY_MODE_followYou )
    {
        if (deltaAdjust(m_chordDeltaTime) > -m_cfg_earlyNotesPoint )
            m_followState = PB_FOLLOW_earlyNotes;
        if (deltaAdjust(m_chordDeltaTime) > -m_cfg_stopPoint )
            m_followState = PB_FOLLOW_waiting;
    }
    else // m_playMode == PB_PLAY_MODE_playAlong
    {
        if (deltaAdjust(m_chordDeltaTime) > m_cfg_playZoneLate )
        {
            missedNotesColour(Cfg::playedStoppedColour());
            fetchNextChord();
            m_rating.lateNotes(m_wantedChord.length() - m_goodPlayedNotes.length());
            setEventBits( EVENT_BITS_forceFullRredraw);
        }
    }
}

void CConductor::outputSavedNotesOff()
{
    while (m_savedNoteOffQueue->length() > 0)
        playTransposeEvent(m_savedNoteOffQueue->pop()); // Output the saved note off events
}

// untangle the sound incase there is any notes off just after we have stopped
void CConductor::findImminentNotesOff()
{
    int i;
    CMidiEvent event;
    int aheadDelta = 0;
    i = 0;

    event = m_nextMidiEvent;

    while (deltaAdjust(m_playingDeltaTime) + aheadDelta   > m_cfg_imminentNotesOffPoint)
    {
        if (event.type() == MIDI_NOTE_OFF )// fixme && isChannelMuted(event.channel()) == false)

            m_savedNoteOffQueue->push(event);
        if ( i >= m_songEventQueue->length())
            break;
        event = m_songEventQueue->index(i);
        aheadDelta -= event.deltaTime();
        i++;
    }
}

void CConductor::missedNotesColour(CColour colour)
{
#if HAS_SCORE
    int i;
    CNote note;
    for (i = 0; i < m_wantedChord.length(); i++)
    {
        note = m_wantedChord.getNote(i);
        if (m_goodPlayedNotes.searchChord(note.pitch(),m_transpose) == false)
            m_scoreWin->setPlayedNoteColour(note.pitch() + m_transpose, colour, m_chordDeltaTime);
    }
#endif
}

void CConductor::realTimeEngine(int mSecTicks)
{
    int type;
    int ticks; // Midi ticks

    //mSecTicks = 2; // for debuging only

    ticks = static_cast<int>(mSecTicks * m_userSpeed * 100000000.0 /m_midiTempo);

    while (checkMidiInput() > 0)
        pianistInput(readMidiInput());


    if (getfollowState() == PB_FOLLOW_waiting )
    {
        if (m_silenceTimeOut > 0)
        {
            m_silenceTimeOut -= mSecTicks;
            if (m_silenceTimeOut <= 0)
            {
                allSoundOff();
                m_silenceTimeOut = 0;
            }
        }


        if (m_followPlayingTimeOut > 0)
        {
            m_followPlayingTimeOut -= ticks;
            if (m_followPlayingTimeOut <= 0)
            {
                m_followPlayingTimeOut = 0;
                m_rating.lateNotes(m_wantedChord.length() - m_goodPlayedNotes.length());
                setEventBits( EVENT_BITS_forceFullRredraw);

                missedNotesColour(Cfg::playedStoppedColour());
                findImminentNotesOff();
                // Don't keep any saved notes off  if there are no notes down
                if (pianistNotesDown() == 0)
                    outputSavedNotesOff();
                m_silenceTimeOut = Cfg2::silenceTimeOut();
            }
        }
        return;
    }

    m_silenceTimeOut = 0;
    if (m_playing == false)
        return;


#if HAS_SCORE
    m_scoreWin->scrollDeltaTime(ticks);
#endif
    m_playingDeltaTime += ticks;
    m_chordDeltaTime +=ticks;

    followPlaying();

    while ( m_playingDeltaTime >= m_leadLagAdjust)
    {
        type = m_nextMidiEvent.type();

        if (m_songEventQueue->length() == 0 && type == MIDI_PB_EOF)
        {
            setEventBits(EVENT_BITS_playingStopped);
            m_playing = false;
            break;
        }

        if (type == MIDI_PB_tempo)
        {
            m_midiTempo = float (m_nextMidiEvent.data1()) * DEFAULT_PPQN / CMidiFile::getPulsesPerQuarterNote();
            ppDebug("Midi Tempo %f  %d", m_midiTempo, CMidiFile::getPulsesPerQuarterNote());
        }
        if (type == MIDI_PB_timeSignature)
        {
            m_currentTimeSigTop = m_nextMidiEvent.data1();
            m_currentTimeSigBottom = m_nextMidiEvent.data2();
            ppDebug("Midi Time Signature %f", m_midiTempo);
        }
        else if ( type != MIDI_NONE )   // this marks the end of the piece of music
        {
            int channel = m_nextMidiEvent.channel();

            // Is this this channel_muted
            //if (isChannelMuted(channel) == false) //fixme
            {
                if (getfollowState() >= PB_FOLLOW_earlyNotes && m_playMode == PB_PLAY_MODE_followYou)
                {
                    // Save up the notes until the pianist press the right key
                    if (m_savedNoteQueue->space()>0)
                        m_savedNoteQueue->push(m_nextMidiEvent);
                    else
                        ppDebug("Warning the m_savedNoteQueue is full");
                    if (type == MIDI_NOTE_OFF)
                    {
                        if (m_savedNoteOffQueue->space()>0)
                            m_savedNoteOffQueue->push(m_nextMidiEvent);
                        else
                            ppDebug("Warning the m_savedNoteOffQueue is full");
                    }
                }
                else
                    playTransposeEvent(m_nextMidiEvent); // Play the midi note or event
            }
        }
        if (m_songEventQueue->length() > 0)
            m_nextMidiEvent = m_songEventQueue->pop();
        else
        {
            m_nextMidiEvent.clear();
            break;
        }

        m_playingDeltaTime -= m_nextMidiEvent.deltaTime() * SPEED_ADJUST_FACTOR;

//ppTrace("Delta %d type 0x%x Note %d", m_nextMidiEvent.deltaTime(), m_nextMidiEvent.type(), m_nextMidiEvent.note());
    }
}

void CConductor::rewind()
{
    int chan;

    for ( chan = 0; chan < MAX_MIDI_CHANNELS; chan++)
    {
        m_savedMainVolume[chan] = 100;
    }
    m_lastSound = -1;
    m_rating.reset();
    m_midiTempo = 1000000 * 120 / 60;// 120 beats per minute is the default
    m_playingDeltaTime = 0;
    m_songEventQueue->clear();
    m_savedNoteQueue->clear();
    m_savedNoteOffQueue->clear();
    m_wantedChordQueue->clear();
    m_nextMidiEvent.clear();
    m_currentTimeSigTop = m_startTimeSigTop;
    m_currentTimeSigBottom = m_startTimeSigBottom;

    m_goodPlayedNotes.clear();  // The good notes the pianist plays
    m_goodNoteLines.clear();
    m_badNoteLines.clear();
    resetWantedChord();

    m_cfg_earlyNotesPoint = CMidiFile::ppqnAdjust(20); // was 10 playZoneEarly
    m_cfg_stopPoint = CMidiFile::ppqnAdjust(0); //was -3; // stop just after the beat
    m_cfg_imminentNotesOffPoint = CMidiFile::ppqnAdjust(-15);  // look ahead and find an Notes off comming up
    // Annie song 25

    m_cfg_playZoneEarly = CMidiFile::ppqnAdjust(Cfg2::playZoneEarly()); // when playing along
    m_cfg_playZoneLate = CMidiFile::ppqnAdjust(Cfg2::playZoneLate());
}

void CConductor::init()
{
    int channel;
    m_followState = PB_FOLLOW_searching;
    this->CMidiDevice::init();
    for ( channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
        muteChannel(channel, false);

#if HAS_SCORE
    assert(m_scoreWin);
    if (m_scoreWin)
        m_scoreWin->setInputChords(&m_goodNoteLines, &m_badNoteLines, &m_rating);
#endif
    setPianoSoundPatches(1-1, 24-1); // 6-1

    rewind();
}
