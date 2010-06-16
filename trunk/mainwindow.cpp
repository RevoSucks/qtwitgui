/************************************************************************************
*
*   - QtWitGui -				2010 giantpune
*
*   the multilingual, multiplatform, multiformat gui for messing with
*   Wii game images.
*
*   This software comes to you with a GPLv3 license.
*   http://www.gnu.org/licenses/gpl-3.0.html
*
*   Basically you are free to modify this code, distribute it, use it in
*   other projects, or anything along those lines.  Just make sure that any
*   derivative work gets the same license.  And don't remove this notice from
*   the derivative work.
*
*   And please, don't be a douche.  If you borrow code from here, don't claim
*   you wrote it.  Share your source code with others.  Even if you are
*   charging a fee for the binaries, let others read the code as somebody has
*   done for you.
*
*************************************************************************************/

#include <QUrl>
#include <QTreeWidget>
#include <QFileInfo>
#include <QMessageBox>
#include <QDesktopServices>
#include <QMessageBox>
#include <QFontMetrics>
#include <QStringList>
#include <qtextstream.h>
#include <qfiledialog.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "svnrev.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "filefolderdialog.h"


#define SAVEFILENAME "QtWitGui.ini"

//this syntax apparently works for all platforms
#define WIT "./wit"
#define MINIMUM_WIT_VERSION 1214

#define PROGRAM_NAME "QtWitGui"
#define PROGRAM_VERSION "0.0.3"
#define WEBSITE_STRING "http://code.google.com/p/qtwitgui/"



#define SAFEDELETE( x ) if( x )delete( x )
#define MAX( x, y ) ( ( x ) > ( y ) ? ( x ) : ( y ) )
#define MIN( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )



MainWindow::MainWindow(QWidget *parent) : QMainWindow( parent ), ui( new Ui::MainWindow )
{
    ui->setupUi( this );
    setAcceptDrops(true);

    undoLastTextOperation = false;
    //gameIsLoadedOk = false;
    //alreadyGotTitle = false;

    ui->plainTextEdit->clear();

    //create the pointer to the process used to run wit
    witProcess = new QProcess( this );


#ifdef Q_WS_WIN
    // Add an environment variable to shut up the cygwin warning in windows
    QStringList env = QProcess::systemEnvironment();
    env << "CYGWIN=nodosfilewarning";
    witProcess->setEnvironment(env);
#endif

    //connect output and input signals between the process and the main window so we can get information from it
    //and also send a "kill" message if the main window is closed while the process is running
    connect( witProcess, SIGNAL( readyReadStandardOutput() ), this, SLOT( ReadyReadStdOutSlot() ) );
    connect( witProcess, SIGNAL( readyReadStandardError() ), this, SLOT( ReadyReadStdErrSlot() ) );
    connect( witProcess, SIGNAL( finished( int, QProcess::ExitStatus ) ), this, SLOT( ProcessFinishedSlot(  int, QProcess::ExitStatus ) ) );
    connect( this, SIGNAL( KillProcess() ), witProcess, SLOT( kill() ) );
    wiithread = new WiiTreeThread;

    //get the version of wit and append it to the titlebar
    witVersionString = "wit: " + tr( "Unknown" );

    QStringList arg;
    arg << "version";
    arg << "--sections";
    SendWitCommand( arg, witGetVersion );

    //create and add the tree window
	//icons
    groupIcon.addPixmap( style()->standardPixmap( QStyle::SP_DirClosedIcon ), QIcon::Normal, QIcon::Off );
    groupIcon.addPixmap( style()->standardPixmap( QStyle::SP_DirOpenIcon ), QIcon::Normal, QIcon::On );
    keyIcon.addPixmap( style()->standardPixmap( QStyle::SP_FileIcon ) );

	//make sure the name column is wide enough
    ui->treeWidget->header()->resizeSection( 0, 300 );

	//connect the wiitreethread to this main window so we can read the output
    connect( wiithread , SIGNAL( SendProgress( int ) ), this, SLOT( UpdateProgressFromThread( int ) ) );
    connect( wiithread , SIGNAL( SendDone( QTreeWidgetItem * ) ), this, SLOT( ThreadIsDoneRunning( QTreeWidgetItem * ) ) );

	//create the actions and stuff for the context menu
    extractAct = new QAction( tr( "Extract" ), this );
    replaceAct = new QAction( tr( "Replace" ), this );
    ui->treeWidget->addAction( extractAct );
    ui->treeWidget->addAction( replaceAct );
    ui->treeWidget->setContextMenuPolicy( Qt::ActionsContextMenu );
    connect( extractAct, SIGNAL( triggered() ), this, SLOT( ExtractSlot() ) );
    connect( replaceAct, SIGNAL( triggered() ), this, SLOT( ReplaceSlot() ) );


    //default the search to the user's home directory ( will be overwritten by loading settings if they exist )
    ui->lineEdit_default_path->setText( QDesktopServices::storageLocation( QDesktopServices::HomeLocation ) );

    //load settings
    LoadSettings();
    this->UpdateOptions();

    //make sure buttons are wide enough for text
    ResizeGuiToLanguage();

    ui->plainTextEdit->clear();
}

//destructor
MainWindow::~MainWindow()
{
    SaveSettings();
    delete ui;
    if( witJob != witNoJob )
    {
	emit KillProcess();
//	qsleep( 1 );
    }
    SAFEDELETE( extractAct  );
    SAFEDELETE( replaceAct );
    SAFEDELETE( witProcess );
}

/**************************************************
*
*   buttons for the wit tab
*
***************************************************/
//copy text
void MainWindow::on_pushButton_3_clicked()
{
    ui->plainTextEdit->selectAll();
    ui->plainTextEdit->copy();
}

//"clear" button clicked
void MainWindow::on_pushButton_2_clicked()
{
    ui->plainTextEdit->clear();
}

//append text to the message box
void MainWindow::AddText( const char in[] )
{
    ui->plainTextEdit->insertPlainText( in );
}

/**************************************************
*
*   get messages and data from threads & processes
*
***************************************************/
//get message from the workthread
void MainWindow::ShowMessage( const QString &s )
{
    ui->plainTextEdit->insertPlainText( s );
}

//get messages from the procces running wit and convert the messages to stuff to use in the GUI
void MainWindow::ReadyReadStdOutSlot()
{
    //read text from wit
    QString read = witProcess->readAllStandardOutput();

    //qDebug() << "gotmessage" << read;
    //get rid of stupid windows new lines
    read.replace( "\r\n", "\n" );

    //delete the last text appended to the console if the last message was flagged to be deleted
    if( undoLastTextOperation )
    {
	ui->plainTextEdit->undo();
	undoLastTextOperation = false;
    }

    if( read.contains( "\r" ) )
    {
	//add the current text 1 line at a time so it can be undone if needed
	QString insertText;
	QString readCopy = read;
	while( readCopy.contains( "\n") )
	{
	    while( !readCopy.startsWith( "\n" ) && !readCopy.isEmpty() )
	    {
		   insertText += readCopy.at( 0 );
		   readCopy.remove( 0, 1 );
	    }
	    insertText += "\n";
	    readCopy.remove( 0, 1 );
	    ui->plainTextEdit->insertPlainText( insertText );
	}
	ui->plainTextEdit->insertPlainText( readCopy );

	//does the current message need to be flagged to be deleted next time this function is called?
	undoLastTextOperation = true;
    }

    else
	ui->plainTextEdit->insertPlainText( read );

    switch ( witJob )
    {
	case witCopy:
	    //turn the % message into a int and pass it to the progress bar
	    if( read.contains( "%" ) )
	    {
		 QString str = read.simplified(); //remove extra whitespace
		 QString numText;
		 int perChar = str.indexOf( "%" );
		 int num = 0;
		 if( perChar < 4 && perChar > 0)
		 {
		     while( str.at( 0 ) != '%' )
		     {
			 numText += str.at( 0 );
			 str.remove( 0, 1 );
		     }
			 //for( int i = 0; i < perChar; i++ ) //copy the number to another string
			  //  numText += str.at( i );
			    //numText += str.toLatin1().data()[ i ];

			 num = numText.toInt();//convert to int
			 if( num < 101 )
			     ui->progressBar->setValue( num );
			 //qDebug( "numText: %s\nnum: %d", numText.toLatin1().data(), num );
		 }
	    }
	    break;

	case witIlist:
	case witDump:
	case witGetVersion:
	    //get all the text output from wit and run it into 1 string and process it all at once
	    filepaths += read;
	    break;

	default:
	    break;

    }

}

//read stderr from wit's process
void MainWindow::ReadyReadStdErrSlot()
{
    QString read = witProcess->readAllStandardError();
    witErrorStr += read;
    ui->plainTextEdit->insertPlainText( read );
}

//triggered after the wit process is done
void MainWindow::ProcessFinishedSlot( int i, QProcess::ExitStatus s )
{
//    qDebug( "process is done running\nExitCode: %d\nExitStatus: %d", i, s );
    if( !i && !s )
    {
	ui->progressBar->setValue( 100 );
	if( witJob != witGetVersion )
	    ui->plainTextEdit->insertPlainText( tr( "Done!" ) );
    }
    else
    {
	QString st;
	QTextStream( &st ) <<"Done, but with error [ ExitCode: " << i << "  ErrorStatus: " << s << "]";
	ui->plainTextEdit->insertPlainText( st );

	if( !witErrorStr.isEmpty() )
	    ErrorMessage( witErrorStr );

	//clear old errors
	witErrorStr.clear();

	witJob = witNoJob;
	return;
    }

    //clear old errors
    witErrorStr.clear();

    QStringList list;
    QString idStr;
    QString nameStr;
    int regionInt = -1;

    //qDebug() << "witJob: " << witJob;
    switch ( witJob )
    {
	case witIlist:
	    if( filepaths.trimmed().isEmpty() )
	    {
		//reset the flag for the current job
		witJob = witNoJob;

		//loading this game was successful, so change the path for use when extracting/saving
		lastPathLoadedCorrectly = isoPath;

		//tell the user that we are ready to do something else
		ui->statusBar->showMessage( tr( "Didn't get any files to display" ) );
		break;
	    }

	    ui->statusBar->showMessage( tr( "Got FST list from wit, parsing it into a pretty file tree..." ) );
	    wiithread->DoCommand( filepaths, ui->checkBox_hiddenFiles->isChecked(), keyIcon, groupIcon );
	    break;

	case witDump:
	    if( filepaths.trimmed().isEmpty() )
		break;

	    //split the output from wit at "\n" and remove spaces and shit
	    list = filepaths.split("\n", QString::SkipEmptyParts );
	    foreach( QString str, list )
	    {
		str = str.trimmed();
		if( str.contains( "ID & type" ) )
		{
		    str.remove( 0, 10 );
		    str = str.trimmed();
		    str.resize( 6 );
		    idStr = str;
		}
		else if( str.contains( "Disc name:" ) )
		{
		    str.remove( 0, 10 );
		    str = str.trimmed();
		    nameStr = str;
		}
		else if( str.contains( "Region setting:" ) )
		{
		    str.remove( 0, 15 );
		    str = str.trimmed();
		    str.resize( 1 );
		    //qDebug() << "region: " << str;
		    bool ok;
		    int v = str.toInt( &ok, 10 );
		    if( ok && v < 4 )
		    {
			regionInt = v + 1;
		    }
		}

	    }
	    if( !idStr.isEmpty()
		&& !nameStr.isEmpty()
		&& regionInt >= 0 )
	    {
		ui->lineEdit_3->setText( idStr );
		ui->lineEdit_4->setText( nameStr );
		ui->comboBox_region->setCurrentIndex( regionInt );
		gameRegion = regionInt -1;
	    }
	    else
	    {
		AbortLoadingGame( tr( "Error parsing data from wit.  The game cannot be loaded.") );
		break;
	    }
	    DoIlist();
	    break;


	case witGetVersion:
	    if( filepaths.trimmed().isEmpty() )
	    {
		ErrorMessage( tr( "The version of wit cannot be determined." ) );
		ui->statusBar->showMessage( tr( "The version of wit cannot be determined." ) );
		witJob = witNoJob;
		break;
	    }

	    //split the output from wit at "\n" and remove spaces
	    list = filepaths.split("\n", QString::SkipEmptyParts );
	    foreach( QString str, list )
	    {
		if( str.contains( "name" ) )
		{
		    str.remove( 0, 6 );
		    str.resize( str.size() - 1 );
		    witVersionString = str + " ";
		}
		else if( str.contains( "version" ) )
		{
		    str.remove( 0, 8 );
		    witVersionString += str + " ";
		}
		else if( str.contains( "revision" ) )
		{
		    str.remove( 0, 9 );
		    witVersionString += "r" + str + " ";
		    bool ok = false;
		    int witSVNr = str.toInt( &ok );
		    if( !ok )
			ErrorMessage( tr( "The version of wit cannot be determined." ) );
		    else if( witSVNr < MINIMUM_WIT_VERSION )
			ErrorMessage( tr( "The version of wit is too low.  Upgrade it!" ) );
		}
		else if( str.contains( "system" ) )
		{
		    str.remove( 0, 7 );
		    witVersionString += str ;
		    QString title = witVersionString + " | Gui: r" + SVN_REV_STR;

		    if( title.endsWith( "m", Qt::CaseInsensitive ) )
			title.resize( title.size() - 1 );

		    setWindowTitle( title );
		}
	    }

	    ui->statusBar->showMessage( tr( "Ready" ) );
	    witJob = witNoJob;
	    break;

	default:
	    witJob = witNoJob;
	    break;
    }
}

//build the ILIST-L command and start the process with it
void MainWindow::DoIlist()
{
    //qDebug() << "DoIlist()";
    if( isoPath.isEmpty() )
    {
	qDebug() << "isoPath.isEmpty()";
	return;
    }

    //clear the last loaded ISO
    filepaths.clear();
    while( ui->treeWidget->takeTopLevelItem( 0 ) );

    QStringList args;
    args << "ILIST-L";
    args << isoPath;

    //partition select
    if( ui->comboBox_partition->currentIndex() )
    {
	args << "--psel=" + ui->comboBox_partition->currentText();
	args << "--pmode=name";
    }

    //args << "--sort=offset";
    args << "--sort=none";

    SendWitCommand( args, witIlist );
}

//update the progressbar based on output from the tree-creating thread
void MainWindow::UpdateProgressFromThread( int i )
{
    ui->progressBar->setValue( i );
}

//triggered when the tree-creating thread is done.  it passes a root item filled with the directory structure for the game
void MainWindow::ThreadIsDoneRunning( QTreeWidgetItem *i )
{
    //move the file tree
    ui->treeWidget->addTopLevelItems( i->takeChildren() );

    //delete the pointer of the root item used by the thread
    SAFEDELETE( i );

    //reset the flag for the current job
    witJob = witNoJob;

    //loading this game was successful, so change the path for use when extracting/saving
    lastPathLoadedCorrectly = isoPath;

    //enable "save" now that there has been a game successfully loaded
    ui->actionSave_As->setEnabled( true );

    //tell the user that we are ready to do something else
    ui->statusBar->showMessage( tr( "Ready" ) );
}

/*******************************************
*
*   rght-click actions
*
********************************************/
//this is triggered on right-click -> extract
void MainWindow::ExtractSlot()
{
    extractPaths.clear();
    //qDebug() << "extractSlotTriggered ";
    QList<QTreeWidgetItem *> selectedItems = ui->treeWidget->selectedItems();
    for( int i = 0; i < selectedItems.size(); i++ )
    {
	extractPaths << ItemToFullPath( selectedItems[ i ] );
    }

    /*for( int i = 0; i < extractPaths.size(); i++ )
    {
	qDebug() << extractPaths[ i ] ;
    }*/
}

//this is triggered on right-click -> replace
void MainWindow::ReplaceSlot()
{
    qDebug() << "replaceSlotTriggered";
}

/*******************************************
*
*   settings functions
*
********************************************/
//save settings to disc
bool MainWindow::SaveSettings()
{
    QFile file( SAVEFILENAME );
    if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) )
	 return false;

    QTextStream out( &file );
    out << "test:"	    << ui->checkBox->checkState()
	<< "\noverwrite:"   << ui->overwrite_checkbox->checkState()
	<< "\ntext:"	    << ui->verbose_combobox->currentIndex()
	<< "\nlogging:"	    << ui->logging_combobox->currentIndex()
	<< "\nios:"	    << ui->default_ios_spinbox->value()
	<< "\npath:"	    << ui->lineEdit_default_path->text()
	<< "\nregion:"	    << ui->comboBox_region->currentIndex()
	<< "\nstarttab:"    << ui->startupTab_combobox->currentIndex()
	<< "\nupdatetitle:" << ui->checkBox_6->checkState()
	<< "\nupdateid:"    << ui->checkBox_7->checkState()
	<< "\ndischdr:"	    << ui->checkBox_2->checkState()
	<< "\ntmdticket:"   << ui->checkBox_4->checkState()
	<< "\nparthdr:"	    << ui->checkBox_3->checkState()
	<< "\nignoreidden:" << ui->checkBox_hiddenFiles->checkState()
	<< "\ndefaultiosch:"<< ui->checkBox_defaultIos->checkState()
	<< "\ndefaultios:"  << ui->spinBox_gameIOS->value()
	<< "\ndefaultregch:"<< ui->checkBox_defaultRegion->checkState()
	<< "\npselect:"	    << ui->comboBox_partition->currentIndex()
	<< "\nwheight:"	    << this->height()
	<< "\nwwidth:"	    << this->width()
	<< "\nwposx:"	    << this->x()
	<< "\nwposy:"	    << this->y()

	;

    return true;

}

//read settings file and set values from it
bool MainWindow::LoadSettings()
{
    QFile file( SAVEFILENAME );
    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
	 return false;

    int newHeight = -1,
	newWidth = -1,
	newX = -1,
	newY = -1;

    while ( !file.atEnd() )
    {

	 QString setting;
	 QString value = file.readLine().trimmed();		//get a line from the file and remove whitespace from the start and end
	 bool ok = false;

	 if( value.startsWith( "#" ) || value.isEmpty() )	//allow for # used as comments and empty lines in the ini file just in case
	     continue;

	 //split the string into setting and value
	 while( !value.startsWith( ":" ) )
	 {
		setting += value.at( 0 );
		value.remove( 0, 1 );
	 }
	 value.remove( 0, 1 );

	//match the setting string and then set a value in the gui
	if( setting == "test" )
	{
	    int v = value.toInt( &ok, 10 );			//for checkboxes, 0 is not checked / 2 is checked
	    ui->checkBox->setChecked( ok && v );		//turn "ok" signals that the string was successfully turned into a int ( base 10 )
	}							//use ok & int as a bool value
	else if( setting == "overwrite" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->overwrite_checkbox->setChecked( ok && v );
	}
	else if( setting == "text" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		ui->verbose_combobox->setCurrentIndex( v );
	}
	else if( setting == "logging" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		ui->logging_combobox->setCurrentIndex( v );
	}
	else if( setting == "ios" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		ui->default_ios_spinbox->setValue( v );
	}
	else if( setting == "region" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		ui->comboBox_region->setCurrentIndex( v );
	}
	else if( setting == "path" )
	{
	    ui->lineEdit_default_path->setText( value );
	}
	else if( setting == "starttab" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
	    {
		ui->tabWidget->setCurrentIndex( v );
		ui->startupTab_combobox->setCurrentIndex( v );
	    }
	}
	else if( setting == "updatetitle" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->checkBox_6->setChecked( ok && v );
	}
	else if( setting == "updateid" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->checkBox_7->setChecked( ok && v );
	}
	else if( setting == "dischdr" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->checkBox_2->setChecked( ok && v );
	}
	else if( setting == "tmdticket" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->checkBox_4->setChecked( ok && v );
	}
	else if( setting == "parthdr" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->checkBox_3->setChecked( ok && v );
	}
	else if( setting == "ignoreidden" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->checkBox_hiddenFiles->setChecked( ok && v );
	}
	else if( setting == "defaultiosch" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->checkBox_defaultIos->setChecked( ok && v );
	}
	else if( setting == "defaultregch" )
	{
	    int v = value.toInt( &ok, 10 );
	    ui->checkBox_defaultRegion->setChecked( ok && v );
	}
	else if( setting == "defaultios" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
	    {
		ui->spinBox_gameIOS->setValue( v );
	    }
	}
	else if( setting == "defaultreg" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
	    {
		ui->comboBox_defaultRegion->setCurrentIndex( v );
	    }
	}
	else if( setting == "pselect" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		ui->comboBox_partition->setCurrentIndex( v );
	}
	else if( setting == "wheight" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		newHeight = v;
	}
	else if( setting == "wwidth" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		newWidth = v;
	}
	else if( setting == "wposx" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		newX = v;
	}
	else if( setting == "wposy" )
	{
	    int v = value.toInt( &ok, 10 );
	    if( ok )
		newY = v;
	}


    }

    if( newHeight > 0 && newWidth > 0 && newX > 0 && newY > 0 )
    {
	this->resize( newWidth, newHeight );
	this->move( newX, newY );
    }
    else qDebug() << "using default size & pos";
    return true;
}

//save settings button clicked
void MainWindow::on_save_pushButton_clicked()
{
    SaveSettings();
    ui->statusBar->showMessage( tr( "Settings Saved" ), 5000 );
    //qDebug() << "settings saved";
}

//get the size of text strings used in different parts of the GUI and resize gui elements based on that size
void MainWindow::ResizeGuiToLanguage()
{
    int pad = 40;
    int size = 0;
    QFontMetrics fm( fontMetrics() );
    ui->pushButton_2->setMinimumWidth( MAX( ui->pushButton_2->minimumWidth(), fm.width( ui->pushButton_2->text() ) + pad ) );
    ui->pushButton_3->setMinimumWidth( MAX( ui->pushButton_3->minimumWidth(), fm.width( ui->pushButton_3->text() ) + pad ) );
    ui->save_pushButton->setMinimumWidth( MAX( ui->save_pushButton->minimumWidth(), fm.width( ui->save_pushButton->text() ) + pad ) );
    ui->checkBox_defaultIos->setMinimumWidth( MAX( ui->checkBox_defaultIos->minimumWidth(), fm.width( ui->checkBox_defaultIos->text() ) + pad ) );
    ui->checkBox_defaultRegion->setMinimumWidth( MAX( ui->checkBox_defaultRegion->minimumWidth(), fm.width( ui->checkBox_defaultRegion->text() ) + pad ) );
    ui->label->setMinimumWidth( MAX( ui->label->minimumWidth(), fm.width( ui->label->text() ) + pad ) );
    ui->label_2->setMinimumWidth( MAX( ui->label_2->minimumWidth(), fm.width( ui->label_2->text() ) + pad ) );
    ui->label_3->setMinimumWidth( MAX( ui->label_3->minimumWidth(), fm.width( ui->label_3->text() ) + pad ) );
    ui->label_7->setMinimumWidth( MAX( ui->label_7->minimumWidth(), fm.width( ui->label_7->text() ) + pad ) );
    ui->label_partition->setMinimumWidth( MAX( ui->label_partition->minimumWidth(), fm.width( ui->label_partition->text() ) + pad ) );


    for( int i = 0; i < ui->verbose_combobox->count(); i++ )
    {
	size = MAX( size, fm.width( ui->verbose_combobox->itemText( i ) ) + pad );
    }
    ui->verbose_combobox->setMinimumWidth( size );

    for( int i = 0; i < ui->startupTab_combobox->count(); i++ )
    {
	size = MAX( size, fm.width( ui->startupTab_combobox->itemText( i ) ) + pad );
    }
    ui->startupTab_combobox->setMinimumWidth( size );




}

//browse for a "default search path" folder
void MainWindow::on_pushButton_settings_searchPath_clicked()
{
    QString dir = QFileDialog::getExistingDirectory( this, tr("Open Directory"), ui->lineEdit_default_path->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );
    if( !dir.isEmpty() )
	ui->lineEdit_default_path->setText( dir );
}

/*******************************************
*
*  file/help menu actions & functions
*
********************************************/

//file->open / ctrl+O
void MainWindow::on_actionOpen_triggered()
{
    qDebug() << "open triggered";
    FileFolderDialog dialog(this);
    dialog.setNameFilter( "*.iso *.wbfs *.ciso *.wdf" );
#ifdef Q_WS_MAC
    dialog.setOption( QFileDialog::DontUseNativeDialog );
#endif
    dialog.setDirectory( ui->lineEdit_default_path->text() );

    if ( !dialog.exec() )
	return;

    isoPath = dialog.selectedFiles()[ 0 ];

    if( isoPath.isEmpty() )
	return;

    OpenGame();
}

void MainWindow::OpenGame()
{
    QStringList args;
    args << "DUMP";
    args << isoPath;

    ui->plainTextEdit->clear();

    SendWitCommand( args, witDump );
}

//file->save as / ctrl+A
void MainWindow::on_actionSave_As_triggered()
{
    qDebug() << "save as";
    if( !ui->actionSave_As->isEnabled() //should never happen
	|| lastPathLoadedCorrectly.isEmpty() )
	return;

    if( witJob != witNoJob )
    {
	QMessageBox::warning(this, tr( "Slow your roll!" ),tr( "Wit is still running.\nWait for the current job to finish." ), tr( "Ok" ) );
	return;
    }

    //open a dialog and browse for an output file
    QString outputPath;
    QFileDialog dialog( this );
#ifdef Q_WS_MAC						    //the OS-x default dialog box doesn't let me select files & folders
    dialog.setOption( QFileDialog::DontUseNativeDialog );
#endif
    dialog.setDirectory( ui->lineEdit_default_path->text() );
    dialog.setNameFilter( "*.iso *.wbfs *.ciso *.wdf" );

    if ( !dialog.exec() || dialog.selectedFiles()[ 0 ].isEmpty() )
	return;


    ui->progressBar->setValue( 0 );

    outputPath = dialog.selectedFiles()[ 0 ];

    QStringList args;
    args << "CP";				// copy command
    args << lastPathLoadedCorrectly;		//source path
    args << outputPath;				//dest path

    //add a --fst arg if needed, since wit will asume we want a .wdf file usually
    QFileInfo fi( outputPath );
    if( fi.isDir() ||
	(  !outputPath.endsWith( ".wbfs", Qt::CaseInsensitive)
	&& !outputPath.endsWith( ".wdf", Qt::CaseInsensitive)
	&& !outputPath.endsWith( ".iso", Qt::CaseInsensitive)
	&& !outputPath.endsWith( ".ciso", Qt::CaseInsensitive) ) )
    {
	qDebug() << "treating " << outputPath << "as fst";
	args << "--fst";
    }

    //region
    int regInt = GetRegion();
    qDebug() << "got reg: " << regInt;
    if( regInt >= 0 )
    {
	QString regString;
	QTextStream( &regString ) << "--region=" << regInt;
	args << regString;
    }

    //ios
    int iosInt = GetIOS();
    if( iosInt >= 0 )
    {
	QString ios;
	QTextStream( &ios ) << "--ios=" << iosInt;
	args << ios;
    }

    //id
    if( ui->checkBox_7->isChecked() )
    {
	args << "--id=" + ui->lineEdit_3->text();
    }

    //title
    if( ui->checkBox_6->isChecked() )
    {
	args << "\'--name=" + ui->lineEdit_4->text() + "\'";
    }

    //modify
    if( ( ui->checkBox_2->isChecked() ||
	ui->checkBox_3->isChecked() ||
	ui->checkBox_4->isChecked() )
	&& ( ui->checkBox_6->isChecked() ||
	ui->checkBox_7->isChecked() ) )
    {
	QString mod = "--modify=";
	u8 checked = 0;

	if( ui->checkBox_2->isChecked() )
	{
	    checked++;
	    mod += "DISC";
	}
	if( ui->checkBox_3->isChecked() )
	{
	    if( checked )mod += ",";
	    mod += "BOOT";
	    checked++;
	}
	if( ui->checkBox_4->isChecked() )
	{
	    if( checked )mod += ",";
	    mod += "TMD,TICKET";
	}

	args << mod;
    }

    //partition select
    if( ui->comboBox_partition->currentIndex() )
    {
	args << "--psel=" + ui->comboBox_partition->currentText();
	args << "--pmode=name";
    }

    //verbose
    if( ui->verbose_combobox->currentIndex() )
    {
	if( ui->verbose_combobox->currentIndex() == 1)
	    args << "--quiet";
	else
	{
	    for( int i = 1; i < ui->verbose_combobox->currentIndex(); i++ )
	    args << "-v";
	}
    }

    //logging
    for( int i = 0; i < ui->logging_combobox->currentIndex(); i++ )
	args << "-L";

    //overwrite existing files
    if( ui->overwrite_checkbox->isChecked() )
	args << "--overwrite";

    //test mode
    if( ui->checkBox->isChecked() )
	args << "--test";

    //make sure we get the progress output
    args << "--progress";

    //clear the current text window
    ui->plainTextEdit->clear();

    SendWitCommand( args, witCopy );
}

//actually start the process with the given list of args and set the jobtype flag for the functions getting output from the process
void MainWindow::SendWitCommand( QStringList args, int jobType )
{
    QString str;
    str += "\n";
    str += WIT;
    str += " ";
    ui->plainTextEdit->insertPlainText( str );

    foreach( QString arg, args)
	ui->plainTextEdit->insertPlainText( arg + " " );

    witJob = jobType;
    witProcess->start( WIT, args );
    if( !witProcess->waitForStarted() )//default timeout 30,000 msecs
    {
	qDebug() << "failed to start wit";
	ui->statusBar->showMessage( tr( "Error starting wit!" ) );
	return;
    }
    ui->statusBar->showMessage( tr( "Wit is running..." ) );
}

//about this program
void MainWindow::on_actionAbout_triggered()
{
    QString aboutText;
    QTextStream( &aboutText ) << PROGRAM_NAME << tr( " is a cross-platform GUI for wit.\n"\
						     "This software comes to you with a GPLv3 license\n\n")

						<< tr( "Version: " ) << PROGRAM_VERSION << "\n"
						<< tr( "Revision: " ) << SVN_REV_STR << "\n"
						<< tr( "Website: " ) << WEBSITE_STRING << "\n"
						<< "2010 Giantpune\n\n"
						<< witVersionString << "\n";

    QMessageBox::about( this, tr( "About " ) + PROGRAM_NAME, aboutText );
}

//about Qt
void MainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

//for aborting loading a game...  doesn't really do anything yet
void MainWindow::AbortLoadingGame( QString message )
{
    ErrorMessage( message );
}

//determine which region to use based on all the different checkboxes and options and crap
//return -1 for "auto"
int MainWindow::GetRegion()
{
    int ret = -1;
    if( ui->checkBox_defaultRegion->isChecked() )//if default region is set
    {
	ret = ui->comboBox_defaultRegion->currentIndex() - 1;
    }
    else
    {
	ret = ui->comboBox_region->currentIndex() - 1;
    }
    qDebug() << "GetRegion(): " << ret;
    return ret;
}

int MainWindow::GetIOS()
{
    int ret = -1;
    if( ui->checkBox_defaultIos->isChecked() )
	ret = ui->default_ios_spinbox->value();

    else
	ret = ui->spinBox_gameIOS->value();

    //qDebug() << ret;
    if( ret != gameIOS )
	return ret;

    return -1;
}

//returns the full path of a item in the tree view
QString MainWindow::ItemToFullPath( QTreeWidgetItem * item )
{
    QString key = item->text(0);
    QTreeWidgetItem *ancestor = item->parent();
    while ( ancestor ) {
	key.prepend(ancestor->text(0) + "/");
	ancestor = ancestor->parent();
    }
    return key;
}

/*******************************************
*
*   prompts and message windows
*
********************************************/
//error prompt window
void MainWindow::ErrorMessage( QString message )
{
    QString text;
    QTextStream( &text ) << message;
    QMessageBox::critical( this, tr( "Error"), text );
}

//update the window & available settings
void MainWindow::UpdateOptions()
{
    // title & ID line edits
    ui->lineEdit_4->setEnabled( ui->checkBox_6->isChecked() );
    ui->lineEdit_3->setEnabled( ui->checkBox_7->isChecked() );

    // check boxes
    bool checked = ui->checkBox_6->isChecked() || ui->checkBox_7->isChecked();
    ui->checkBox_2->setEnabled( checked );
    ui->checkBox_3->setEnabled( checked );
    ui->checkBox_4->setEnabled( checked );

    //default IOS & region settings
    ui->default_ios_spinbox->setEnabled( ui->checkBox_defaultIos->isChecked() );
    ui->comboBox_defaultRegion->setEnabled( ui->checkBox_defaultRegion->isChecked() );

    //IOS & region [ tab 1 ]
    ui->spinBox_gameIOS->setEnabled( !ui->checkBox_defaultIos->isChecked() );
    ui->comboBox_region->setEnabled( !ui->checkBox_defaultRegion->isChecked() );

    //title & id
/*    char path[ 256 ];
    snprintf( path, sizeof( path ), "%s/sys/boot.bin", ui->lineEdit->text().toLatin1().data() );

    qDebug() << path;
    FILE *f = fopen( path, "rb" );
    if( !f )
    {

	snprintf( path, sizeof( path ), "%s/DATA/sys/boot.bin", ui->lineEdit->text().toLatin1().data() );
	qDebug() << path;
	f = fopen( path, "rb" );
    }
    if( f )
    {
	fseek( f, 0, 0 );
	fread( &id, 6, 1, f );
	ui->lineEdit_3->setText( id );

	fseek( f, 0x20, 0 );
	fread( &name, 0x40, 1, f );
	ui->lineEdit_4->setText( name );

	fclose( f );
    }

    //IOS
    snprintf( path, sizeof( path ), "%s/tmd.bin", ui->lineEdit->text().toLatin1().data() );
    qDebug() << path;
    f = fopen( path, "rb" );
    if( !f )
    {
	snprintf( path, sizeof( path ), "%s/DATA/tmd.bin", ui->lineEdit->text().toLatin1().data() );
	qDebug() << path;
	f = fopen( path, "rb" );
    }
    if( f )
    {
	fseek( f, 0x18b, 0 );
	fread( &tmdIOS, 1, 1, f );
	fclose( f );

	if( ui->default_ios_spinbox->value() < 3 )
	{
	    ui->spinBox->setValue( tmdIOS );
	}
	else
	{
	    ui->spinBox->setValue( ui->default_ios_spinbox->value() );
	}

	ui->spinBox->setDisabled( false );

	QString m = "\"" + ui->lineEdit->text() + "\" loaded";
	ui->statusBar->showMessage( m );
    }
    else
    {
	ui->spinBox->setDisabled( false );
	tmdIOS = ui->spinBox->value();
	QString m = "Unknown IOS for \"" + ui->lineEdit->text() + "\" - action not supported yet.";
	ui->statusBar->showMessage( m );
    }
*/
}

void MainWindow::on_checkBox_6_clicked()
{
    this->UpdateOptions();
}

void MainWindow::on_checkBox_7_clicked()
{
    this->UpdateOptions();
}

void MainWindow::on_checkBox_defaultIos_clicked()
{
    this->UpdateOptions();
}

void MainWindow::on_checkBox_defaultRegion_clicked()
{
    this->UpdateOptions();
}

/*******************************************
*
*   drag and drop stuff
*
********************************************/
//triggered on dropping a file in the main window
void MainWindow::dropEvent( QDropEvent *event )
{
    QString path = event->mimeData()->urls().at( 0 ).toLocalFile().trimmed();

    QFile file( path );
    if( file.exists() )
    {
	isoPath = path;
	OpenGame();
    }
    else
    {
	qDebug() << path << "doesn't exist";
	event->ignore();
    }
}

//triggered on dragging a file around the mainwindow
void MainWindow::dragEnterEvent( QDragEnterEvent *event )
{
    if( event->mimeData()->hasUrls() )
    {
	event->acceptProposedAction();
    }
    else
    {
	event->ignore();
    }
}
