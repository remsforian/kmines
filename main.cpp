#include "main.h"
#include "main.moc"

#include <qwhatsthis.h>

#include <kapp.h>
#include <klocale.h>
#include <kglobal.h>
#include <kconfig.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kmenubar.h>
#include <khelpmenu.h>
#include <kstdaction.h>
#include <kkeydialog.h>
#include <kstatusbar.h>
#include <kstdgameaction.h>

#include "version.h"
#include "status.h"

MainWidget::MainWidget()
: KMainWindow(0), levelAction(NbLevels+1)
{
	installEventFilter(this);

	status = new Status(this);
	status->installEventFilter(this);
	connect(status, SIGNAL(keyboardEnabled(bool)),
			SLOT(setKeyboardEnabled(bool)));
	connect(status, SIGNAL(gameStateChanged(GameState)),
			SLOT(gameStateChanged(GameState)));
	connect(status, SIGNAL(message(const QString &)),
			statusBar(), SLOT(message(const QString &)));

	// Game & Popup
	KStdGameAction::gameNew(status, SLOT(restartGame()), actionCollection());
    KStdGameAction::pause(status, SLOT(pauseGame()), actionCollection());
    KListAction *la = new KListAction(i18n("Show Highscores"), 0, 0, 0,
                                      actionCollection(), "highscores");
    QStringList list;
    list.append("Easy");
    list.append("Normal");
    list.append("Expert");
    la->setItems(list);
    connect(la, SIGNAL(activated(int)), status, SLOT(showHighscores(int)));

	KStdGameAction::print(status, SLOT(print()), actionCollection());
	KStdGameAction::quit(qApp, SLOT(quit()), actionCollection());

	// keyboard
	QArray<KAction *> keyAction(7);
	keyAction[0] = new KAction(i18n("Move up"), Key_Up,
							   status, SLOT(moveUp()),
							   actionCollection(), "keyboard_moveup");
	keyAction[1] = new KAction(i18n("Move down"), Key_Down,
							   status, SLOT(moveDown()),
							   actionCollection(), "keyboard_movedown");
	keyAction[2] = new KAction(i18n("Move left"), Key_Left,
							   status, SLOT(moveLeft()),
							   actionCollection(), "keyboard_moveleft");
	keyAction[3] = new KAction(i18n("Move right"), Key_Right,
							   status, SLOT(moveRight()),
							   actionCollection(), "keyboard_moveright");
	keyAction[4] = new KAction(i18n("Reveal mine"), Key_Shift,
							   status, SLOT(reveal()),
							   actionCollection(), "keyboard_revealmine");
	keyAction[5] = new KAction(i18n("Mark mine"), Key_Space,
							   status, SLOT(mark()),
							   actionCollection(), "keyboard_markmine");
	keyAction[6] = new KAction(i18n("Automatic reveal"), Key_Control,
							   status, SLOT(autoReveal()),
							   actionCollection(), "keyboard_autoreveal");
	KAccel *kacc = new KAccel(this);
	for (uint i=0; i<keyAction.size(); i++) {
		keyAction[i]->setGroup("keyboard_group");
		keyAction[i]->plugAccel(kacc);
	}

	// Settings
	KStdAction::showMenubar(this, SLOT(toggleMenubar()), actionCollection());
	KStdAction::preferences(status, SLOT(preferences()), actionCollection());
	KStdAction::keyBindings(this, SLOT(configureKeys()), actionCollection());

	// Levels
	levelAction[Easy]
		= new KRadioAction(i18n("Easy"), 0, this, SLOT(easyLevel()),
						   actionCollection(), "level_easy");
	levelAction[Normal]
		= new KRadioAction(i18n("Normal"), 0, this, SLOT(normalLevel()),
						   actionCollection(), "level_normal");
	levelAction[Expert]
		= new KRadioAction(i18n("Expert"), 0, this, SLOT(expertLevel()),
						   actionCollection(), "level_expert");
	levelAction[Custom]
		= new KRadioAction(i18n("Custom mine field ..."), 0,
                           this, SLOT(customLevel()),
						   actionCollection(), "level_custom");
	for (uint i=0; i<levelAction.size(); i++)
		levelAction[i]->setExclusiveGroup("level");

	// status bar
	QWhatsThis::add(statusBar(), i18n("Game status"));

	createGUI();
	readSettings();
	setCentralWidget(status);
}

#define MENUBAR_ACTION \
    ((KToggleAction *)action(KStdAction::stdName(KStdAction::ShowMenubar)))

#define PAUSE_ACTION ((KToggleAction *)action("game_pause"))

void MainWidget::readSettings()
{
	LevelData l = OptionDialog::readLevel();
	if ( l.level!=Custom ) levelAction[l.level]->setChecked(true);
	status->newGame(l);

	bool visible = OptionDialog::readMenuVisible();
	MENUBAR_ACTION->setChecked(visible);
	toggleMenubar();

	setKeyboardEnabled( OptionDialog::readKeyboard() );
}

void MainWidget::changeLevel(Level level)
{
	if ( !levelAction[level]->isChecked() ) return;

	LevelData l;
	if ( level==Custom ) {
		levelAction[Custom]->setChecked(false);
		l = status->currentLevel();
		level = l.level;
		CustomDialog cu(l, this);
		if ( !cu.exec() ) { // level unchanged
			if ( level!=Custom ) levelAction[level]->setChecked(true);
			return;
		}
	} else l = LEVELS[level];

	status->newGame(l);
	OptionDialog::writeLevel(l);
}

bool MainWidget::eventFilter(QObject *, QEvent *e)
{
	QPopupMenu *popup;
	switch (e->type()) {
	case QEvent::MouseButtonPress :
		if ( ((QMouseEvent *)e)->button()!=RightButton ) return false;
		popup = (QPopupMenu*)factory()->container("popup", this);
		if ( popup ) popup->popup(QCursor::pos());
		return true;
	case QEvent::LayoutHint:
		setFixedSize(minimumSize()); // because QMainWindow and KMainWindow
		                             // do not manage fixed central widget and
		                             // hidden menubar ...
		return false;
	default : return false;
	}
}

void MainWidget::toggleMenubar()
{
	bool b = MENUBAR_ACTION->isChecked();
	if (b) menuBar()->show();
	else menuBar()->hide();
	OptionDialog::writeMenuVisible(b);
}

void MainWidget::setKeyboardEnabled(bool enable)
{
	QValueList<KAction *> list = actionCollection()->actions("keyboard_group");
	QValueList<KAction *>::Iterator it;
	for (it = list.begin(); it!=list.end(); ++it)
		(*it)->setEnabled(enable);
}

void MainWidget::configureKeys()
{
	KKeyDialog::configureKeys(actionCollection(), xmlFile(), true, this);
}

void MainWidget::gameStateChanged(GameState s)
{
	switch (s) {
	case Stopped:
		PAUSE_ACTION->setEnabled(false);
        break;
	case Paused:
		PAUSE_ACTION->setText(i18n("&Resume"));
		break;
	case Playing:
		PAUSE_ACTION->setText(i18n("&Pause"));
		PAUSE_ACTION->setEnabled(true);
		break;
	}
}

//----------------------------------------------------------------------------
static const char *DESCRIPTION
    = I18N_NOOP("KMines is a classical mine sweeper game.");

int main(int argc, char **argv)
{
    KAboutData aboutData("kmines", I18N_NOOP("KMines"), LONG_VERSION,
						 DESCRIPTION, KAboutData::License_GPL,
						 COPYLEFT, 0, HOMEPAGE);
    aboutData.addAuthor("Nicolas Hadacek", 0, EMAIL);
	aboutData.addCredit("Andreas Zehender", I18N_NOOP("Smiley pixmaps"));
    KCmdLineArgs::init(argc, argv, &aboutData);

    KApplication a;
    if ( a.isRestored() ) RESTORE(MainWidget)
    else {
        MainWidget *mw = new MainWidget;
        mw->show();
    }
    return a.exec();
}
