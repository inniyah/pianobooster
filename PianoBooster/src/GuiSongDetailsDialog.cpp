/*!
    @file           GuiSongDetailsDialog.cpp

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

#include <QtGui>

#include "GuiSongDetailsDialog.h"
#include "GlView.h"

GuiSongDetailsDialog::GuiSongDetailsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    m_song = 0;
    m_settings = 0;
    m_glView = 0;
    m_trackList = 0;
    setWindowTitle("Song Details");
}


void GuiSongDetailsDialog::init(CSong* song, CSettings* settings, CGLView * glView)
{
    m_song = song;
    m_settings = settings;
    m_glView = glView;
    m_trackList = m_song->getTrackList();
    leftHandChannelCombo->addItem(tr("No channel assigned"));
    leftHandChannelCombo->addItems(m_trackList->getAllChannelProgramNames(true));
    rightHandChannelCombo->addItem(tr("No channel assigned"));
    rightHandChannelCombo->addItems(m_trackList->getAllChannelProgramNames(true));

    leftHandChannelCombo->setCurrentIndex(m_trackList->getHandTrackIndex(PB_PART_left) + 1);
    rightHandChannelCombo->setCurrentIndex(m_trackList->getHandTrackIndex(PB_PART_right) +1);
    updateSongInfoText();

}


void GuiSongDetailsDialog::updateSongInfoText()
{
    QString str;
    songInfoText->clear();
    bool activateOkButton = false;

    if (leftHandChannelCombo->currentIndex() != 0 && leftHandChannelCombo->currentIndex() == rightHandChannelCombo->currentIndex())
        songInfoText->append("<span style=\"color:red\">The left and rignt hand channels must be different</span>");
    else if ((leftHandChannelCombo->currentIndex() == 0 && rightHandChannelCombo->currentIndex() != 0 ) ||
             (rightHandChannelCombo->currentIndex() == 0 && leftHandChannelCombo->currentIndex() != 0 ) )
        songInfoText->append("<span style=\"color:red\">Both left and rignt hand channels must be none to disable this feature</span>");
    else
    {
        songInfoText->append("<span style=\"color:gray\">Set the MIDI Channels to be used for left and right hand piano parts:</span>");
        activateOkButton = true;
    }

    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(activateOkButton);
}


void GuiSongDetailsDialog::on_leftHandChannelCombo_activated (int index)
{
    updateSongInfoText();
}

void GuiSongDetailsDialog::on_rightHandChannelCombo_activated (int index)
{
    updateSongInfoText();
}

void GuiSongDetailsDialog::accept()
{
    m_trackList->setActiveHandsIndex(leftHandChannelCombo->currentIndex() -1, rightHandChannelCombo->currentIndex() -1);
    this->QDialog::accept();
}
