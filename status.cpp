/*
 * Copyright (c) 1996-2002 Nicolas HADACEK (hadacek@kde.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "status.h"
#include "status.moc"

#include <qpainter.h>
#include <qpixmap.h>
#include <qwhatsthis.h>
#include <qlayout.h>

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kmessagebox.h>
#include <kaction.h>

#include "ghighscores.h"
#include "solver/solver.h"
#include "dialogs.h"


Status::Status(QWidget *parent)
    : QWidget(parent, "status")
{
    _solver = new Solver(this);
    connect(_solver, SIGNAL(solvingDone(bool)), SLOT(solvingDone(bool)));

// top layout
	QGridLayout *top = new QGridLayout(this, 2, 5, 10, 10);
    top->setResizeMode(QLayout::Fixed);
    top->setColStretch(1, 1);
    top->setColStretch(3, 1);

// status bar
	// mines left LCD
	left = new LCD(5, this);
    left->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    left->setDefaultColors(white, black);
	QWhatsThis::add(left, i18n("<qt>Mines left.<br/>"
                               "It turns <font color=\"red\">red</font> "
                               "when you have flagged more cases than "
                               "present mines.</qt>"));
    top->addWidget(left, 0, 0);

	// smiley
	smiley = new Smiley(this);
	connect(smiley, SIGNAL(clicked()), SLOT(smileyClicked()));
	smiley->setFocusPolicy(QWidget::NoFocus);
	QWhatsThis::add(smiley, i18n("Press to start a new game"));
    top->addWidget(smiley, 0, 2);

	// digital clock LCD
	dg = new DigitalClock(this);
	QWhatsThis::add(dg, i18n("<qt>Time elapsed.<br/>"
                             "It turns <font color=\"blue\">blue</font> "
                             "if it is a highscore "
                             "and <font color=\"red\">red</font> "
                             "if it is the best time.</qt>"));
    top->addWidget(dg, 0, 4);

// mines field
    _fieldContainer = new QWidget(this);
    QGridLayout *g = new QGridLayout(_fieldContainer, 1, 1);
    field = new Field(_fieldContainer);
    g->addWidget(field, 0, 0, AlignCenter);
	connect( field, SIGNAL(updateStatus(bool)), SLOT(update(bool)) );
	connect(field, SIGNAL(gameStateChanged(GameState, bool)),
			SLOT(gameStateChanged(GameState, bool)) );
    connect(field, SIGNAL(setMood(Smiley::Mood)), smiley,
            SLOT(setMood(Smiley::Mood)));
    connect(field, SIGNAL(setCheating()), dg, SLOT(setCheating()));
    connect(field, SIGNAL(incActions()), dg, SLOT(incActions()));
	QWhatsThis::add(field, i18n("Mines field."));

// resume button
    _resumeContainer = new QWidget(this);
    g = new QGridLayout(_resumeContainer, 1, 1);
    QFont f = font();
    f.setBold(true);
    QPushButton *pb
        = new QPushButton(i18n("Press to Resume"), _resumeContainer);
    pb->setFont(f);
    connect(pb, SIGNAL(clicked()), SIGNAL(pause()));
    g->addWidget(pb, 0, 0, AlignCenter);

    _stack = new QWidgetStack(this);
    _stack->addWidget(_fieldContainer);
    _stack->addWidget(_resumeContainer);
    _stack->raiseWidget(_fieldContainer);
    top->addMultiCellWidget(_stack, 1, 1, 0, 4);
}

void Status::smileyClicked()
{
    if ( field->isPaused() ) emit pause();
    else restartGame();
}

void Status::newGame(int t)
{
    Level::Type type = (Level::Type)t;
    if ( type!=Level::Custom ) {
        KExtHighscores::setGameType(type);
        field->setLevel(Level(type));
    } else field->setLevel(CustomSettings::readLevel());
}

void Status::settingsChanged()
{
    field->readSettings();

    Level current = field->level();
    if ( current.type()!=Level::Custom ) return;
    Level l = CustomSettings::readLevel();
    if ( l.width()==current.width() && l.height()==current.height()
         && l.nbMines()==current.nbMines() ) return;
    field->setLevel(l);
}

void Status::update(bool mine)
{
	int r = field->nbMines() - field->nbMarked();
    QColor color = (r<0 && !field->isSolved() ? red : white);
    left->setColor(color);
	left->display(r);

	if ( field->isSolved() && !mine )
        gameStateChanged(GameOver, true); // ends only for wins
}

void Status::gameStateChanged(GameState state, bool won)
{
    switch (state) {
    case Playing:
        smiley->setMood(Smiley::Normal);
        dg->start();
        break;
    case GameOver:
        field->showAllMines();
        field->gameOver();
        smiley->setMood(won ? Smiley::Happy : Smiley::Sad);
        dg->stop();
        if ( field->level().type()!=Level::Custom && won && !dg->cheating() )
            KExtHighscores::submitScore(dg->score(), this);
        break;
    case Paused:
        smiley->setMood(Smiley::Sleeping);
        dg->stop();
        break;
    case Stopped:
        smiley->setMood(Smiley::Normal);
        update(false);
        KExtHighscores::Score first(KExtHighscores::Won);
        KExtHighscores::Score last(KExtHighscores::Won);
        if ( field->level().type()!=Level::Custom ) {
            first = KExtHighscores::firstScore();
            last = KExtHighscores::lastScore();
        }
        dg->reset(first, last);
        break;
    }

    if ( state==Paused ) _stack->raiseWidget(_resumeContainer);
    else _stack->raiseWidget(_fieldContainer);
    emit gameStateChangedSignal(state);
}

void Status::advise()
{
    int res = KMessageBox::warningContinueCancel(this,
               i18n("When the solver gives "
               "you an advice, you lose the ability to enter the highscores."),
                QString::null, QString::null, "advice_warning");
    if ( res==KMessageBox::Cancel ) return;
    dg->setCheating();
    float probability;
    Grid2D::Coord c = _solver->advise(*field, probability);
    field->setAdvised(c, probability);
}

void Status::solve()
{
    dg->setCheating();
    _solver->solve(*field, false);
}

void Status::solvingDone(bool success)
{
    if ( !success ) gameStateChanged(GameOver, false);
}

void Status::solveRate()
{
    SolvingRateDialog sd(*field, this);
    sd.exec();
}
