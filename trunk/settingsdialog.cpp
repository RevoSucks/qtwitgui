
#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "includes.h"
#include "tools.h"
#include "wiitdb.h"

extern QString rootAskStr;
extern QString rootWrongStr;
extern QString rootFailStr;
/*
 page order

 paths
 root previleges
 wit/wwt
 partition windows
 game browsers
 coverflow
 wiitdb

 */

SettingsDialog::SettingsDialog( QWidget *parent ) : QDialog( parent ), ui( new Ui::SettingsDialog )
{
    ui->setupUi( this );
    setWindowTitle( tr( "Settings" ) );
#ifdef Q_WS_WIN//remove root settings for windows
    QWidget *p = ui->page_2;
    ui->stackedWidget->removeWidget( p );
    delete( p );

    QListWidgetItem *q = ui->listWidget->takeItem( 1 );
    delete( q );
#endif
    ResizeButtons();

    QSettings s( settingsPath, QSettings::IniFormat );
    s.beginGroup( "paths" );
    ui->lineEdit_wit->setText( s.value( "wit" ).toString() );
    ui->lineEdit_wwt->setText( s.value( "wwt" ).toString() );
    ui->lineEdit_coverPath->setText( s.value( "covers" ).toString() );
    ui->lineEdit_wiitdbPath->setText( s.value( "wiitdb" ).toString() );
#ifndef Q_WS_WIN
    ui->lineEdit_MntPath->setText( s.value( "mountfile", "/etc/mtab" ).toString() );
#endif
    s.endGroup();
#ifndef Q_WS_WIN
    s.beginGroup( "root" );
    ui->checkBox_runasRoot->setChecked( s.value( "enabled" ).toBool() );
    ui->groupBox_rootMessages->setEnabled( ui->checkBox_runasRoot->isChecked() );
    ui->lineEdit_rootReqStr->setText( s.value( "requestString", "[sudo] password for" ).toString() );
    ui->lineEdit_rootWrongStr->setText( s.value( "wrongPwString", "Sorry, try again." ).toString() );
    ui->lineEdit_rootFail->setText( s.value( "failString", "sudo: 3 incorrect password attempts" ).toString() );
    s.endGroup();
#endif
    s.beginGroup( "wit_wwt" );
    //ui->checkBox_testMode->setChecked( s.value( "testMode" ).toBool() );
    //ui->checkBox_overWrite->setChecked( s.value( "overWriteFiles" ).toBool() );
    //ui->checkBox_ignoreSvn->setChecked( s.value( "ignoreSvn" ).toBool() );
    //ui->checkBox_Sneek->setChecked( s.value( "sneek" ).toBool() );
    ui->comboBox_key->setCurrentIndex( s.value( "encKey" ).toInt() );
    //ui->spinBox_defaultIos->setValue( s.value( "defaultIos" ).toInt() );
    //ui->comboBox_defaultRegion->setCurrentIndex( s.value( "defaultRegion" ).toInt() );

    //int sel = ui->comboBox_partitionSelect->findText( s.value( "partitionSelect" ).toString() );
    //if( sel >= 0 )
	//ui->comboBox_partitionSelect->setCurrentIndex( sel );
    s.endGroup();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::on_pushButton_ok_clicked()
{
    //save settings
    QSettings s( settingsPath, QSettings::IniFormat );
    s.beginGroup( "paths" );
    QString thisProgram = QCoreApplication::instance()->arguments().at( 0 );//avoid an endless loop if this program is used as the path for a process
    if( ui->lineEdit_wit->text() != thisProgram )
	s.setValue( "wit", ui->lineEdit_wit->text() );
    if( ui->lineEdit_wwt->text() != thisProgram )
	s.setValue( "wwt", ui->lineEdit_wwt->text() );
    s.setValue( "covers", ui->lineEdit_coverPath->text() );
    s.setValue( "wiitdb", ui->lineEdit_wiitdbPath->text() );
#ifndef Q_WS_WIN
    s.setValue( "mountfile", ui->lineEdit_MntPath->text() );
#endif
    s.endGroup();
#ifndef Q_WS_WIN
    s.beginGroup( "root" );
    s.setValue( "enabled", ui->checkBox_runasRoot->isChecked() );
    s.setValue( "requestString", ui->lineEdit_rootReqStr->text() );
    s.setValue( "wrongPwString", ui->lineEdit_rootWrongStr->text() );
    s.setValue( "failString", ui->lineEdit_rootFail->text() );
    rootAskStr = ui->lineEdit_rootReqStr->text();
    rootWrongStr = ui->lineEdit_rootWrongStr->text();
    rootFailStr = ui->lineEdit_rootFail->text();
    s.endGroup();
#endif
    s.beginGroup( "wit_wwt" );
    //s.setValue( "testMode", ui->checkBox_testMode->isChecked() );
    //s.setValue( "overWriteFiles", ui->checkBox_overWrite->isChecked() );
    //s.setValue( "ignoreSvn", ui->checkBox_ignoreSvn->isChecked() );
    //s.setValue( "sneek", ui->checkBox_Sneek->isChecked() );
    s.setValue( "encKey", ui->comboBox_key->currentIndex() );
    //s.setValue( "defaultIos", ui->spinBox_defaultIos->value() );
    //s.setValue( "defaultRegion", ui->comboBox_defaultRegion->currentIndex() );
    //s.setValue( "partitionSelect", ui->comboBox_partitionSelect->currentText() );
    s.endGroup();

    s.sync();

    QDialog::accept();
}

void SettingsDialog::on_pushButton_wit_clicked()
{
    QString wit = QFileDialog::getOpenFileName( this, tr( "Where is wit?" ) );
    if( wit.isEmpty() )
	return;
    ui->lineEdit_wit->setText( wit );
}

void SettingsDialog::on_pushButton_wwt_clicked()
{
    QString wwt = QFileDialog::getOpenFileName( this, tr( "Where is wwt?" ) );
    if( wwt.isEmpty() )
	return;
    ui->lineEdit_wwt->setText( wwt );
}
#ifndef Q_WS_WIN
void SettingsDialog::on_pushButton_mountFilePath_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select Mounts File"), "/" );
    if( !fileName.isEmpty() )
	ui->lineEdit_MntPath->setText( fileName );
}
#endif
void SettingsDialog::on_pushButton_wiitdbPath_clicked()
{
    QString p = QFileDialog::getOpenFileName( this, tr( "Where is wiitdb.zip / .xml?" ) );
    if( p.isEmpty() )
    {
	wiiTDB->LoadFile( ":/wiitdb.zip" );
	return;
    }
    ui->lineEdit_wiitdbPath->setText( p );

    if( !wiiTDB->LoadFile( p ) )//try to load theirs.  if not, fall back to the one we have compiled in the program
    {
	ui->lineEdit_wiitdbPath->clear();
	wiiTDB->LoadFile( ":/wiitdb.zip" );
    }
}

//get the width of teh widest text in the "paths" buttons and resize all buttons to match that one
void SettingsDialog::ResizeButtons()
{
    QFontMetrics fm( fontMetrics() );
    int max = fm.width( ui->pushButton_coverPath->text() );
    max = MAX( max, fm.width( ui->pushButton_mountFilePath->text() ) );
    max = MAX( max, fm.width( ui->pushButton_titlesPath->text() ) );
    max = MAX( max, fm.width( ui->pushButton_wiitdbPath->text() ) );
    max = MAX( max, fm.width( ui->pushButton_wit->text() ) );
    max = MAX( max, fm.width( ui->pushButton_wwt->text() ) );

    max += 30;
    ui->pushButton_coverPath->setMinimumWidth( max );
    ui->pushButton_mountFilePath->setMinimumWidth( max );
    ui->pushButton_titlesPath->setMinimumWidth( max );
    ui->pushButton_wiitdbPath->setMinimumWidth( max );
    ui->pushButton_wit->setMinimumWidth( max );
    ui->pushButton_wwt->setMinimumWidth( max );

}