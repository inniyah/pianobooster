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

#define OPTION_DEBUG_CONDUCTOR     0
#if OPTION_DEBUG_CONDUCTOR
#define ppDEBUG_CONDUCTOR(args)     ppDebug args
#else
#define ppDEBUG_CONDUCTOR(args)
#endif


#include "Conductor.h"
#include "Score.h"
#include "Cfg.h"

playMode_t CConductor::m_playMode = PB_PLAY_MODE_listen;

CConductor::CConductor()
{
    int i;

    m_scoreWin = 0;

    m_songEventQueue = new CQueue<CMidiEvent>(1000);
    m_wantedChordQueue = new CQueue<CChord>(1000);
    m_savedNoteQueue = new CQueue<CMidiEvent>(200);
    m_savedNoteOffQueue = new CQueue<CMidiEvent>(200);
    m_playing = false;
    m_transpose = 0;
    m_latencyFix = 0;
    m_leadLagAdjust = 0;
    setSpeed(1.0);
    setLatencyFix(0);
    m_boostVolume = 0;
    m_pianoVolume = 0;
    m_activeChannel = 0;
    m_skill = 0;
    m_silenceTimeOut = 0;
    m_realTimeEventBits = 0;
    m_mutePianistPart = false;
    setPianistChannels(1-1,2-1);
    cfg_timingMarkersFlag = false;

    for ( i = 0; i < MAX_MIDI_CHANNELS; i++)
    {
        m_muteChannels[i] = false;
    }
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
        channelSoundOff(channel); // fixme this is called too often
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
        else // otherwise always boost both hands
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
    if (channel == m_activeChannel)
        activePart= true;

    //if (channel == 5)  activePart= true; // for debuging

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
        // The piano volume can reduce the volume of the music
    if (m_pianoVolume>0)
        returnVolume = (returnVolume * (100 - m_pianoVolume)) / 100;;

    if (returnVolume > 127)
        returnVolume = 127;
    return returnVolume;
}

/* send boost volume by adjusting all channels */
void CConductor::outputBoostVolume()
{
    int chan;

    for ( chan =0; chan <MAX_MIDI_CHANNELS; chan++ )
    {
        if (chan == m_pianistGoodChan || chan == m_pianistBadChan)
            continue;
        CMidiEvent midi;
        midi.controlChangeEvent(0, chan, MIDI_MAIN_VOLUME, calcBoostVolume(chan,-1));
        playMidiEvent(midi);
    }
    outputPianoVolume();
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

void CConductor::activatePianistMutePart()
{
    mutePart(PB_PART_all, false);
    if (m_playMode != PB_PLAY_MODE_listen && m_mutePianistPart == true)
    {
        if (m_activeChannel == CNote::bothHandsChan())
        {
            if (CNote::getActiveHand() == PB_PART_both || CNote::getActiveHand() == PB_PART_left)
                mutePart(CNote::leftHandChan(), true);
            if (CNote::getActiveHand() == PB_PART_both || CNote::getActiveHand() == PB_PART_right)
                mutePart(CNote::rightHandChan(), true);
        }
        else
            mutePart(m_activeChannel, true);
    }
}

void CConductor::mutePianistPart(bool state)
{
    m_mutePianistPart = state;
    activatePianistMutePart();
}

void CConductor::setActiveHand(whichPart_t hand)
{
    if (CNote::getActiveHand() == hand)
        return;
    CNote::setActiveHand(hand);
    activatePianistMutePart();
    outputBoostVolume();
    m_wantedChord = m_savedwantedChord;

    if (m_wantedChord.trimOutOfRangeNotes(m_transpose)==0)
        fetchNextChord();

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
    findSplitPoint();
    forceScoreRedraw();
}

void CConductor::setActiveChannel(int channel)
{
    m_activeChannel = channel;
    outputBoostVolume();
    resetWantedChord();
    fetchNextChord();
    activatePianistMutePart();
}


void CConductor::outputPianoVolume()
{
    CMidiEvent event;
    int volume = 127;
    // if piano volume is between -100 and 0 reduce the volume accordangly
    if (m_pianoVolume < 0)
        volume = (volume * (100 + m_pianoVolume)) / 100;

    event.controlChangeEvent(0, m_pianistGoodChan, MIDI_MAIN_VOLUME, volume);
    playMidiEvent(event); // Play the midi note or event
    event.controlChangeEvent(0, m_pianistBadChan, MIDI_MAIN_VOLUME, volume);
    playMidiEvent(event); // Play the midi note or event

}
void CConductor::testWrongNoteSound(bool enable)
{
    m_testWrongNoteSound = enable;
    CMidiEvent event;


    if (m_cfg_rightNoteSound>=0) // ignore if set to -1 ("None")
    {
        event.programChangeEvent(0, m_pianistGoodChan, m_cfg_rightNoteSound);
        playMidiEvent( event );
    }
    if (m_cfg_wrongNoteSound>=0)
    {
        event.programChangeEvent(0, m_pianistBadChan, m_cfg_wrongNoteSound);
        playMidiEvent( event );
    }
}


void CConductor::playMusic(bool start)
{
    m_playing = start;
    allSoundOff();
    if (start)
    {
        resetAllChannels();
        activatePianistMutePart();

        testWrongNoteSound(false);
        if (seekingBarNumber())
            resetWantedChord();




ppTrace("setLatencyFix %d", getLatencyFix()); // Fixme


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

    if (event.type() == MIDI_NOTE_ON && isChannelMuted(event.channel()) == true &&
                                CChord::isNotePlayable(event.note(), m_transpose) == true)
        return; // mute the note by not playing it

    // boost any volume events
    if (event.type() == MIDI_CONTROL_CHANGE && event.data1() == MIDI_MAIN_VOLUME)
        event.setDatat2(calcBoostVolume(event.channel(), event.data2() ));

    // Don't output note on if we are seeking to bar
    if (!seekingBarNumber())
        playMidiEvent(event); // Play the midi note or event
    else
    {
        if (event.type() == MIDI_PROGRAM_CHANGE || event.type() == MIDI_CONTROL_CHANGE)
            playMidiEvent(event); // Play the midi note or event
    }
}

void CConductor::outputSavedNotes()
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
    ppDEBUG_CONDUCTOR(("resetWantedChord m_chordDeltaTime %d m_playingDeltaTime %d", m_chordDeltaTime, m_playingDeltaTime ));

    m_followPlayingTimeOut = false;
    m_chordDeltaTime = m_playingDeltaTime;
    m_pianistTiming = m_chordDeltaTime;
    m_pianistSplitPoint = MIDDLE_C;

    outputSavedNotes();
    m_followState = PB_FOLLOW_searching;
}

// switch modes if we are playing well enough (ie don't slow down if we are playing late)
void CConductor::setFollowSkillAdvanced(bool enable)
{
    if (getLatencyFix() == 0)
    {
        m_followSkillAdvanced = enable;
        m_stopPoint = (enable) ? m_cfg_stopPointAdvanced: m_cfg_stopPointBeginner ;
    }
    else
    {
        // behave differently if we are using the latency fix
        m_cfg_earlyNotesPoint = m_cfg_imminentNotesOffPoint; //disable the earily notes
        m_followSkillAdvanced = true;
    }
}


void CConductor::findSplitPoint()
{
    // find the split point
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
    m_pianistSplitPoint = ((lowestTreble + highestBase) /2 ) + m_transpose;
}

void CConductor::fetchNextChord()
{
    m_followState = PB_FOLLOW_searching;
    m_followPlayingTimeOut = false;

    outputSavedNotes();

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
        m_pianistTiming = m_chordDeltaTime;
    }
    while (m_wantedChord.trimOutOfRangeNotes(m_transpose)==0);

    // now find the split point
    findSplitPoint();
}


bool CConductor::validatePianistNote(CMidiEvent& inputNote)
{
    if ( m_chordDeltaTime <= -m_cfg_playZoneEarly)
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

    if (inputNote.type() == MIDI_NOTE_ON)
    {
        whichPart_t hand;
        hand = (inputNote.note() >= m_pianistSplitPoint)? PB_PART_right : PB_PART_left;

        if ( validatePianistNote(inputNote) == true)
        {
            m_goodPlayedNotes.addNote(hand, inputNote.note());
            m_goodNoteLines.addNote(hand, inputNote.note());
            int pianistTiming = (/*m_followPlayingTimeOut ||*/ !cfg_timingMarkersFlag) ? NOT_USED : m_pianistTiming;
            m_scoreWin->setPlayedNoteColour(inputNote.note(),
                        (!m_followPlayingTimeOut)? Cfg::playedGoodColour():Cfg::playedBadColour(),
                        m_chordDeltaTime, pianistTiming);


            if (validatePianistChord() == true)
            {
                if (m_chordDeltaTime < 0)
                    m_tempo.removePlayingTicks(-m_chordDeltaTime);

                m_goodPlayedNotes.clear();
                fetchNextChord();
                // count the good notes so that the live percentage looks OK
                m_rating.totalNotes(m_wantedChord.length());
                if (m_rating.getAccuracyValue() > 0.8)
                    setFollowSkillAdvanced(true); // change the skill level only when they are good enough
                else
                    setFollowSkillAdvanced(false);
                setEventBits( EVENT_BITS_forceRatingRedraw);
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
        if (m_badNoteLines.removeNote(inputNote.note()))
            goodSound = false;
        bool hasNote = m_goodPlayedNotes.removeNote(inputNote.note());

        if (hasNote)
            m_scoreWin->setPlayedNoteColour(inputNote.note(),
                    (!m_followPlayingTimeOut)? Cfg::noteColour():Cfg::playedStoppedColour(),
                    m_chordDeltaTime);

        outputSavedNotesOff();
    }


    if (goodSound == true || m_cfg_wrongNoteSound < 0)
    {
        if (m_cfg_rightNoteSound >= 0) // don't play anything if the sound is set to -1 (none)
        {
            inputNote.setChannel(m_pianistGoodChan);
            playMidiEvent( inputNote );
        }
    }
    else
    {
        inputNote.setChannel(m_pianistBadChan);
        playMidiEvent( inputNote );
    }


    /*
    // use the same channel for the right and wrong note
    int pianoSound = (goodSound == true) ? m_cfg_rightNoteSound : m_cfg_wrongNoteSound;

    if (pianoSound != m_lastSound)
    {
        m_lastSound = pianoSound;

        CMidiEvent midiSound;
        midiSound.programChangeEvent(0,inputNote.channel(),pianoSound);
        playMidiEvent( midiSound );
    }
    */

}

// Counts the number of notes the pianist has down
int CConductor::pianistNotesDown()
{
    return m_goodNoteLines.length() + m_badNoteLines.length();
}

void CConductor::addDeltaTime(int ticks)
{
    m_scoreWin->scrollDeltaTime(ticks);
    m_playingDeltaTime += ticks;
    m_chordDeltaTime +=ticks;
}

void CConductor::followPlaying()
{

    if ( m_playMode == PB_PLAY_MODE_listen )
        return;

    if (m_wantedChord.length() == 0)
        fetchNextChord();

    if (m_wantedChord.length() == 0)
        return;
/* fixme delete
    if (deltaAdjust(m_chordDeltaTime) > -m_cfg_earlyNotesPoint && !seekingBarNumber() )
        m_followState = PB_FOLLOW_earlyNotes;
/*/

    if (seekingBarNumber())
    {
        if (deltaAdjust(m_chordDeltaTime) > -m_stopPoint )
            fetchNextChord();
    }
    else if ( m_playMode == PB_PLAY_MODE_followYou)
    {
        if (deltaAdjust(m_chordDeltaTime) > -m_cfg_earlyNotesPoint )
            m_followState = PB_FOLLOW_earlyNotes;
        if (deltaAdjust(m_chordDeltaTime) > -m_stopPoint )
        {
            m_followState = PB_FOLLOW_waiting;
            // Throw away the time past the stop point (by adding a negative ticks)
            addDeltaTime( -m_stopPoint*SPEED_ADJUST_FACTOR - m_chordDeltaTime);
        }
    }
    else // m_playMode == PB_PLAY_MODE_playAlong
    {
        if (m_chordDeltaTime > m_cfg_playZoneLate )
        {
            missedNotesColour(Cfg::playedStoppedColour());
            fetchNextChord();
            m_rating.lateNotes(m_wantedChord.length() - m_goodPlayedNotes.length());
            setEventBits( EVENT_BITS_forceRatingRedraw);
        }
    }
}

void CConductor::outputSavedNotesOff()
{
    while (m_savedNoteOffQueue->length() > 0)
        playTransposeEvent(m_savedNoteOffQueue->pop()); // Output the saved note off events
}

// untangle the sound in case there is any notes off just after we have stopped
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
    int i;
    CNote note;
    for (i = 0; i < m_wantedChord.length(); i++)
    {
        note = m_wantedChord.getNote(i);
        if (m_goodPlayedNotes.searchChord(note.pitch(),m_transpose) == false)
            m_scoreWin->setPlayedNoteColour(note.pitch() + m_transpose, colour, m_chordDeltaTime);
    }
}

void CConductor::realTimeEngine(int mSecTicks)
{
    int type;
    int ticks; // Midi ticks

    //mSecTicks = 2; // for debugging only

    ticks = m_tempo.mSecToTicks(mSecTicks);

    if (!m_followPlayingTimeOut)
        m_pianistTiming += ticks;

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


        if (m_pianistTiming > m_cfg_playZoneLate)
        {
            m_tempo.insertPlayingTicks(ticks);

            if (m_followPlayingTimeOut == false)
            {
                m_followPlayingTimeOut = true;

                m_tempo.clearPlayingTicks();
                m_rating.lateNotes(m_wantedChord.length() - m_goodPlayedNotes.length());
                setEventBits( EVENT_BITS_forceRatingRedraw);

                missedNotesColour(Cfg::playedStoppedColour());
                findImminentNotesOff();
                // Don't keep any saved notes off if there are no notes down
                if (pianistNotesDown() == 0)
                    outputSavedNotesOff();
                m_silenceTimeOut = Cfg::silenceTimeOut();
            }
        }
        return;
    }

    m_silenceTimeOut = 0;
    if (m_playing == false)
        return;

    if (seekingBarNumber())
        ticks = 0;

    m_tempo.adjustTempo(&ticks);


    ticks = m_bar.addDeltaTime(ticks);

    if (seekingBarNumber())
        ticks = m_bar.goToBarNumer();

    if (m_bar.hasBarNumberChanged())
    {
        ppDEBUG_CONDUCTOR(("m_savedNoteQueue %d m_playingDeltaTime %d",m_savedNoteQueue->space() , m_playingDeltaTime ));
        ppDEBUG_CONDUCTOR(("getfollowState() %d  %d %d",getfollowState() , m_leadLagAdjust, m_songEventQueue->length() ));
        //fixme this did not work setEventBits( EVENT_BITS_forceBarNumberRedraw);
        setEventBits( EVENT_BITS_forceFullRedraw);
    }

    addDeltaTime(ticks);

    followPlaying();
    while ( m_playingDeltaTime >= m_leadLagAdjust)
    {
        type = m_nextMidiEvent.type();

        if (m_songEventQueue->length() == 0 && type == MIDI_PB_EOF)
        {
            ppLogInfo("The End of the song");
            setEventBits(EVENT_BITS_playingStopped);
            m_playing = false;
            break;
        }

        if (type == MIDI_PB_tempo)
        {
            m_tempo.setMidiTempo(m_nextMidiEvent.data1());
            m_leadLagAdjust = m_tempo.mSecToTicks( -getLatencyFix() );
        }
        else if (type == MIDI_PB_timeSignature)
        {
            m_bar.setTimeSig(m_nextMidiEvent.data1(), m_nextMidiEvent.data2());
            ppDebug("Midi Time Signature %d/%d", m_nextMidiEvent.data1(),m_nextMidiEvent.data2());

        }
        else if ( type != MIDI_NONE )   // this marks the end of the piece of music
        {
            int channel = m_nextMidiEvent.channel();

            // Is this this channel_muted
            //if (isChannelMuted(channel) == false) //fixme
            if (channel!= m_pianistGoodChan && channel!= m_pianistBadChan)
            {
                if (getfollowState() >= PB_FOLLOW_earlyNotes &&
                        m_playMode == PB_PLAY_MODE_followYou &&
                        !seekingBarNumber() &&
                        m_followSkillAdvanced == false)
                {
                    // Save up the notes until the pianist press the right key
                    if (m_savedNoteQueue->space()>0)
                        m_savedNoteQueue->push(m_nextMidiEvent);
                    else
                        ppLogWarn("Warning the m_savedNoteQueue is full");

                    if (type == MIDI_NOTE_OFF)
                    {
                        if (m_savedNoteOffQueue->space()>0)
                            m_savedNoteOffQueue->push(m_nextMidiEvent);
                        else
                            ppDebug("Warning the m_savedNoteOffQueue is full");
                    }
                }
                else
                {
                    playTransposeEvent(m_nextMidiEvent); // Play the midi note or event
                    ppDEBUG_CONDUCTOR(("playEvent() chan %d type %d note %d", m_nextMidiEvent.channel() , m_nextMidiEvent.type() , m_nextMidiEvent.note(), m_songEventQueue->length() ));
                }
            }
        }
        if (m_songEventQueue->length() > 0)
            m_nextMidiEvent = m_songEventQueue->pop();
        else
        {
            ppDEBUG_CONDUCTOR(("no data in song queue"));
            m_nextMidiEvent.clear();
            break;
        }

        m_playingDeltaTime -= m_nextMidiEvent.deltaTime() * SPEED_ADJUST_FACTOR;
        followPlaying();
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
    m_playingDeltaTime = 0;
    m_tempo.reset();

    m_songEventQueue->clear();
    m_savedNoteQueue->clear();
    m_savedNoteOffQueue->clear();
    m_wantedChordQueue->clear();
    m_nextMidiEvent.clear();
    m_bar.rewind();

    m_goodPlayedNotes.clear();  // The good notes the pianist plays
    m_goodNoteLines.clear();
    m_badNoteLines.clear();
    resetWantedChord();

    m_cfg_earlyNotesPoint = CMidiFile::ppqnAdjust(15); // was 10 playZoneEarly
    m_cfg_stopPointBeginner = CMidiFile::ppqnAdjust(-0); //was -3; // stop just after the beat
    m_cfg_stopPointAdvanced = CMidiFile::ppqnAdjust(-15); //was -3; // stop just after the beat
    m_cfg_imminentNotesOffPoint = CMidiFile::ppqnAdjust(-15);  // look ahead and find an Notes off coming up

    setFollowSkillAdvanced(false);

    // Annie song 25

    m_cfg_playZoneEarly = CMidiFile::ppqnAdjust(Cfg::playZoneEarly()) * SPEED_ADJUST_FACTOR; // when playing along
    m_cfg_playZoneLate = CMidiFile::ppqnAdjust(Cfg::playZoneLate()) * SPEED_ADJUST_FACTOR;
}

void CConductor::init()
{
    int channel;
    m_followState = PB_FOLLOW_searching;
    this->CMidiDevice::init();
    for ( channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
        muteChannel(channel, false);

    assert(m_scoreWin);
    if (m_scoreWin)
        m_scoreWin->setInputChords(&m_goodNoteLines, &m_badNoteLines, &m_rating);

    setPianoSoundPatches(1-1, 7-1); // 6-1

    rewind();
}
