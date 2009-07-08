/*********************************************************************************/
/*!
@file           Conductor.h

@brief          The realtime midi engine. Send notes to the midi device and responds to the midi keyboard.

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

#ifndef __CONDUCTOR_H__
#define __CONDUCTOR_H__

#include "MidiEvent.h"
#include "Queue.h"
#include "MidiDevice.h"
#include "Cfg.h"
#include "Chord.h"
#include "Rating.h"
#include "Tempo.h"
#include "Bar.h"

class CScore;
class CPiano;
class CSettings;

typedef enum {
    PB_FOLLOW_searching,
    PB_FOLLOW_earlyNotes,
    PB_FOLLOW_waiting
} followState_t;

enum {
    PB_PLAY_MODE_listen,
    PB_PLAY_MODE_followYou,
    PB_PLAY_MODE_playAlong
};

typedef int playMode_t;

typedef enum {
    PB_STOP_POINT_MODE_automatic,
    PB_STOP_POINT_MODE_onTheBeat,
    PB_STOP_POINT_MODE_afterTheBeat
} stopPointMode_t;


/*!
 * @brief   xxxxx.
 */
class CConductor : public CMidiDevice
{
public:
    CConductor();
    ~CConductor();

    void init(CScore * scoreWin, CSettings* settings);


    //! add a midi event to be analysed and played
    void midiEventInsert(CMidiEvent event);

    //! first check if there is space to add a midi event
    int midiEventSpace();

    //! add a chord to be played by the pianist
    void chordEventInsert(CChord chord) {m_wantedChordQueue->push(chord);}

    //! first check if there is space to add a chord event
    int chordEventSpace() { return m_wantedChordQueue->space();}

    void rewind();

    void realTimeEngine(int mSecTicks);
    void playMusic(bool start);
    bool playingMusic() {return m_playing;}


    float getSpeed() {return m_tempo.getSpeed();}
    void setSpeed(float speed)
    {
        m_tempo.setSpeed(speed);
        m_leadLagAdjust = m_tempo.mSecToTicks( -getLatencyFix() );
    }
    void setLatencyFix(int latencyFix)
    {
        m_latencyFix = latencyFix;
        m_leadLagAdjust = m_tempo.mSecToTicks( -getLatencyFix());
    }
    int getLatencyFix() { return m_latencyFix; }

    void muteChannel(int channel, bool state);
    void mutePart(int channel, bool state);
    void transpose(int transpose);

    int getTranspose() {return m_transpose;}
    int getSkill() {return m_skill;}
    void setSkill(int skill)
    {
        m_skill = skill;
        if (m_skill >  5)
            m_skill = 5;
        if (m_skill < 0)
            m_skill = 0;
    }

    void pianistInput(CMidiEvent inputNote);
    void setPlayMode(playMode_t mode)
    {
        m_playMode = mode;
        if (m_playMode < 0 ) m_playMode = 0;
        if (m_playMode > 2 ) m_playMode = 2;
        if ( m_playMode == PB_PLAY_MODE_listen )
            resetWantedChord();
        activatePianistMutePart();
        outputBoostVolume();
    }

    int getBoostVolume() {return m_boostVolume;}
    void boostVolume(int boostVolume)
    {
        m_boostVolume = boostVolume;
        if (m_boostVolume < -100 ) m_boostVolume = -100;
        if (m_boostVolume > 100 ) m_boostVolume = 100;
        outputBoostVolume();
    }

    int getPianoVolume() {return m_pianoVolume;}
    void pianoVolume(int pianoVolume)
    {
        m_pianoVolume = pianoVolume;
        if (m_pianoVolume < -100 ) m_pianoVolume = -100;
        if (m_pianoVolume > 100 ) m_pianoVolume = 100;
        outputBoostVolume();
    }
    static playMode_t getPlayMode() {return m_playMode;}

    CChord getWantedChord() {return m_wantedChord;}
    void setActiveHand(whichPart_t hand);

    void setActiveChannel(int channel);
    int getActiveChannel(){return m_activeChannel;}
    void setPianistChannels(int goodChan, int badChan){
        m_pianistGoodChan = goodChan;
        m_pianistBadChan = badChan;
    }
    bool hasPianistKeyboardChannel(int chan)   { return (m_pianistGoodChan == chan || m_pianistBadChan == chan ) ? true : false;}

    CRating* getRating(){return &m_rating;}

    // You MUST clear the time sig to 0 first before setting an new start time Sig
    void setTimeSig(int top, int bottom) { m_bar.setTimeSig(top, bottom);}

    void getTimeSig(int *top, int *bottom) {m_bar.getTimeSig(top, bottom);}
    void testWrongNoteSound(bool enable);

    // -1 means no sound -2 means ignore this paramater
    void setPianoSoundPatches(int rightSound, int wrongSound, bool update = false){
        m_cfg_rightNoteSound = rightSound;
        if ( wrongSound != -2)
            m_cfg_wrongNoteSound = wrongSound;
        if (update)
            updatePianoSounds();
    }

    void setEventBits(eventBits_t bits) { m_realTimeEventBits |= bits; } // don't change the other bits
    // set to true to force the score to be redrawn
    void forceScoreRedraw(){ setEventBits( EVENT_BITS_forceFullRedraw); }
    int getBarNumber(){ return m_bar.getBarNumber();}

    double getCurrentBarPos(){ return m_bar.getCurrentBarPos();}

    void setPlayFromBar(double bar){ m_bar.setPlayFromBar(bar);}
    void setPlayUptoBar(double bar){ m_bar.setPlayUptoBar(bar);}
    double getPlayUptoBar(){ return m_bar.getPlayUptoBar();}
    void setLoopingBars(double bars){ m_bar.setLoopingBars(bars);}
    double getLoopingBars(){ return m_bar.getLoopingBars();}

    void mutePianistPart(bool state);

    bool cfg_timingMarkersFlag;
    stopPointMode_t cfg_stopPointMode;


protected:
    CScore* m_scoreWin;

    CQueue<CMidiEvent>* m_songEventQueue;
    CQueue<CChord>* m_wantedChordQueue;

    eventBits_t m_realTimeEventBits; //used to signal real time events to the caller of task()

    void outputSavedNotes();
    void activatePianistMutePart();

    void resetWantedChord();

    bool validatePianistNote( const CMidiEvent& inputNote);
    bool validatePianistChord();

    bool seekingBarNumber() { return m_bar.seekingBarNumber();}



private:
    void allSoundOff();
    void resetAllChannels();
    void outputBoostVolume();
    void outputPianoVolume();

    void channelSoundOff(int channel);
    void findSplitPoint();
    void fetchNextChord();
    void playTransposeEvent(CMidiEvent event);
    void outputSavedNotesOff();
    void findImminentNotesOff();
    void updatePianoSounds();

    void followPlaying();
    void missedNotesColour(CColour colour);

    int calcBoostVolume(int chan, int volume);

    void addDeltaTime(int ticks);


    int m_playingDeltaTime;
    int m_chordDeltaTime;
    bool m_playing;

    int m_transpose;     // the number of semitones to transpose the music
    followState_t m_followState;

    followState_t getfollowState()
    {
        if ( m_playMode == PB_PLAY_MODE_listen )
            return PB_FOLLOW_searching;
        return m_followState;
    }

    CRating m_rating;
    CQueue<CMidiEvent>* m_savedNoteQueue;
    CQueue<CMidiEvent>* m_savedNoteOffQueue;
    CMidiEvent m_nextMidiEvent;
    bool m_muteChannels[MAX_MIDI_CHANNELS];
    bool isChannelMuted(int chan)
    {
        if (chan < 0 || chan >= MAX_MIDI_CHANNELS)
            return true;
        return m_muteChannels[chan];
    }
    void setFollowSkillAdvanced(bool enable);

    CSettings* m_settings;
    CPiano* m_piano;

    CTempo m_tempo;
    CBar m_bar;
    int m_leadLagAdjust; // Synchronise the sound the the video
    int m_silenceTimeOut; // used to create silence if the student stops for toooo long
    CChord m_wantedChord;  // The chord the pianist needs to play
    CChord m_savedwantedChord; // A copy of the wanted chord complete with both left and right parts
    CChord m_goodPlayedNotes;  // The good notes the pianist plays
    int m_pianistSplitPoint;    // Defines which notes go in the base and treble clef
    bool m_followSkillAdvanced;
    int m_lastSound;
    int m_stopPoint;   // Were we stop the music if the pianist is late
    int m_cfg_rightNoteSound;
    int m_cfg_wrongNoteSound;
    int m_pianistGoodChan;
    int m_pianistBadChan;
    int m_cfg_earlyNotesPoint; // don't press the note too early
    int m_cfg_stopPointAdvanced;   // Were we stop the music if the pianist is late
    int m_cfg_stopPointBeginner;   // Were we stop the music if the pianist is late
    int m_cfg_imminentNotesOffPoint;
    int m_cfg_playZoneEarly; // when playing along
    int m_cfg_playZoneLate;

    int m_pianistTiming;  //measure whether the pianist is playing early or late
    bool m_followPlayingTimeOut;  // O dear, the student is too slow

    bool m_testWrongNoteSound;
    int m_boostVolume;
    int m_pianoVolume;
    int m_activeChannel; // The current part that is being displayed (used for boost and activatePianistMutePart)
    int m_savedMainVolume[MAX_MIDI_CHANNELS];
    static playMode_t m_playMode;
    int m_skill;
    bool m_mutePianistPart;
    int m_latencyFix;     // Try to fix the latency (put the time in msec, 0 disables it)

};

#endif //__CONDUCTOR_H__
