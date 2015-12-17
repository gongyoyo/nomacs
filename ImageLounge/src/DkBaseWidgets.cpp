/*******************************************************************************************************
 DkBaseWidgets.cpp
 Created on:	11.02.2015
 
 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances
 
 Copyright (C) 2011-2015 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011-2015 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011-2015 Florian Kleber <florian@nomacs.org>

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 *******************************************************************************************************/

#include "DkBaseWidgets.h"
#include "DkSettings.h"
#include "DkUtils.h"

#pragma warning(push, 0)	// no warnings from includes - begin
#include <QGraphicsEffect>
#include <QAction>
#include <QDebug>
#include <QTimer>
#include <QPainter>
#include <QStyleOption>
#include <QEvent>
#include <QScrollBar>
#pragma warning(pop)	// no warnings from includes - end

namespace nmc {

DkWidget::DkWidget(QWidget* parent, Qt::WindowFlags flags) : QWidget(parent, flags) {
	init();
}

void DkWidget::init() {

	setMouseTracking(true);

	mBgCol = (DkSettings::app.appMode == DkSettings::mode_frameless) ?
		DkSettings::display.bgColorFrameless :
	DkSettings::display.bgColorWidget;

	mShowing = false;
	mHiding = false;
	mBlocked = false;
	mDisplaySettingsBits = 0;
	mOpacityEffect = 0;

	// painter problems if the widget is a child of another that has the same graphicseffect
	// widget starts on hide
	mOpacityEffect = new QGraphicsOpacityEffect(this);
	mOpacityEffect->setOpacity(0);
	mOpacityEffect->setEnabled(false);
	setGraphicsEffect(mOpacityEffect);

	setVisible(false);
}

void DkWidget::paintEvent(QPaintEvent *event) {
	
	// fixes stylesheets which are not applied to custom widgets
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

	QWidget::paintEvent(event);
}

void DkWidget::registerAction(QAction* action) {
	connect(this, SIGNAL(visibleSignal(bool)), action, SLOT(setChecked(bool)));
}

void DkWidget::block(bool blocked) {
	mBlocked = blocked;
	setVisible(false);
}

void DkWidget::setDisplaySettings(QBitArray* displayBits) {
	mDisplaySettingsBits = displayBits;
}

bool DkWidget::getCurrentDisplaySetting() {


	if (!mDisplaySettingsBits)
		return false;

	if (DkSettings::app.currentAppMode < 0 || DkSettings::app.currentAppMode >= mDisplaySettingsBits->size()) {
		qDebug() << "[WARNING] illegal app mode: " << DkSettings::app.currentAppMode;
		return false;
	}

	return mDisplaySettingsBits->testBit(DkSettings::app.currentAppMode);
};

bool DkWidget::isHiding() const {
	return mHiding;
}

void DkWidget::show(bool saveSetting) {

	// here is a strange problem if you add a DkWidget to another DkWidget -> painters crash
	if (!mBlocked && !mShowing) {
		mHiding = false;
		mShowing = true;
		setVisible(true, saveSetting);
		animateOpacityUp();
	}
}

void DkWidget::hide(bool saveSetting) {

	if (!mHiding) {
		mHiding = true;
		mShowing = false;
		animateOpacityDown();

		// set display bit here too -> since the final call to setVisible takes a few seconds
		if (saveSetting && mDisplaySettingsBits && mDisplaySettingsBits->size() > DkSettings::app.currentAppMode) {
			mDisplaySettingsBits->setBit(DkSettings::app.currentAppMode, false);
		}
	}
}

void DkWidget::setVisible(bool visible, bool saveSetting) {

	if (mBlocked) {
		QWidget::setVisible(false);
		return;
	}

	if (visible && !isVisible() && !mShowing)
		mOpacityEffect->setOpacity(100);

	QWidget::setVisible(visible);
	emit visibleSignal(visible);	// if this gets slow -> put it into hide() or show()

	if (saveSetting && mDisplaySettingsBits && mDisplaySettingsBits->size() > DkSettings::app.currentAppMode) {
		mDisplaySettingsBits->setBit(DkSettings::app.currentAppMode, true);
	}

}

void DkWidget::animateOpacityUp() {

	if (!mShowing)
		return;

	mOpacityEffect->setEnabled(true);
	if (mOpacityEffect->opacity() >= 1.0f || !mShowing) {
		mOpacityEffect->setOpacity(1.0f);
		mShowing = false;
		mOpacityEffect->setEnabled(false);
		return;
	}

	QTimer::singleShot(20, this, SLOT(animateOpacityUp()));
	mOpacityEffect->setOpacity(mOpacityEffect->opacity()+0.05);
}

void DkWidget::animateOpacityDown() {

	if (!mHiding)
		return;

	mOpacityEffect->setEnabled(true);
	if (mOpacityEffect->opacity() <= 0.0f) {
		mOpacityEffect->setOpacity(0.0f);
		mHiding = false;
		setVisible(false, false);	// finally hide the widget
		mOpacityEffect->setEnabled(false);
		return;
	}

	QTimer::singleShot(20, this, SLOT(animateOpacityDown()));
	mOpacityEffect->setOpacity(mOpacityEffect->opacity()-0.05);
}

// DkLabel --------------------------------------------------------------------
DkLabel::DkLabel(QWidget* parent, const QString& text) : QLabel(text, parent) {

	mBgCol = (DkSettings::app.appMode == DkSettings::mode_frameless) ?
		DkSettings::display.bgColorFrameless :
	DkSettings::display.bgColorWidget;

	setMouseTracking(true);
	mParent = parent;
	mText = text;
	init();
	hide();
}

DkLabel::~DkLabel() {
}

void DkLabel::init() {

	mTime = -1;
	mFixedWidth = -1;
	mFontSize = 17;
	mTextCol = QColor(255, 255, 255);
	mBlocked = false;

	mTimer.setSingleShot(true);
	connect(&mTimer, SIGNAL(timeout()), this, SLOT(hide()));

	// default look and feel
	QFont font;
	font.setPixelSize(mFontSize);
	QLabel::setFont(font);
	QLabel::setTextInteractionFlags(Qt::TextSelectableByMouse);

	QLabel::setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	updateStyleSheet();
}

void DkLabel::hide() {
	mTime = 0;
	QLabel::hide();
}

void DkLabel::setText(const QString& msg, int time) {

	mText = msg;
	mTime = time;

	if (!mTime || msg.isEmpty()) {
		hide();
		return;
	}

	setTextToLabel();
	show();

	if (time != -1)
		mTimer.start(time);
}

void DkLabel::showTimed(int time) {

	mTime = time;

	if (!time) {
		hide();
		return;
	}

	show();

	if (time != -1)
		mTimer.start(time);

}


QString DkLabel::getText() {
	return mText;
}

void DkLabel::setFontSize(int fontSize) {

	mFontSize = fontSize;

	QFont font;
	font.setPixelSize(fontSize);
	QLabel::setFont(font);
	QLabel::adjustSize();
}

void DkLabel::stop() {
	mTimer.stop();
	hide();
}

void DkLabel::updateStyleSheet() {
	QLabel::setStyleSheet("QLabel{color: " + mTextCol.name() + "; margin: " + 
		QString::number(mMargin.y()) + "px " +
		QString::number(mMargin.x()) + "px " +
		QString::number(mMargin.y()) + "px " +
		QString::number(mMargin.x()) + "px;}");
}

void DkLabel::paintEvent(QPaintEvent *event) {

	if (mBlocked || !mTime)	// guarantee that the label is hidden after the time is up
		return;

	QPainter painter(this);
	draw(&painter);
	painter.end();

	QLabel::paintEvent(event);
}

void DkLabel::draw(QPainter* painter) {

	drawBackground(painter);
}

void DkLabel::setFixedWidth(int fixedWidth) {

	mFixedWidth = fixedWidth;
	setTextToLabel();
}

void DkLabel::setTextToLabel() {

	if (mFixedWidth == -1) {
		QLabel::setText(mText);
		QLabel::adjustSize();
	}
	else {
		setToolTip(mText);
		QLabel::setText(fontMetrics().elidedText(mText, Qt::ElideRight, mFixedWidth-2*mMargin.x()));
		QLabel::resize(mFixedWidth, height());
	}

}

DkLabelBg::DkLabelBg(QWidget* parent, const QString& text) : DkLabel(parent, text) {

	mBgCol = (DkSettings::app.appMode == DkSettings::mode_frameless) ?
		DkSettings::display.bgColorFrameless :
	DkSettings::display.bgColorWidget;

	setAttribute(Qt::WA_TransparentForMouseEvents);	// labels should forward mouse events

	setObjectName("DkLabelBg");
	updateStyleSheet();

	mMargin = QPoint(7,2);
	setMargin(mMargin);
}

void DkLabelBg::updateStyleSheet() {

	QLabel::setStyleSheet("QLabel#DkLabelBg{color: " + mTextCol.name() + "; padding: " + 
		QString::number(mMargin.y()) + "px " +
		QString::number(mMargin.x()) + "px " +
		QString::number(mMargin.y()) + "px " +
		QString::number(mMargin.x()) + "px; " +
		"background-color: " + DkUtils::colorToString(mBgCol) + ";}");	// background
}

// DkFadeLabel --------------------------------------------------------------------
DkFadeLabel::DkFadeLabel(QWidget* parent, const QString& text) : DkLabel(parent, text) {
	init();
}

void DkFadeLabel::init() {

	mBgCol = (DkSettings::app.appMode == DkSettings::mode_frameless) ?
		DkSettings::display.bgColorFrameless :
	DkSettings::display.bgColorWidget;

	showing = false;
	hiding = false;
	mBlocked = false;
	displaySettingsBits = 0;

	// widget starts on hide
	opacityEffect = new QGraphicsOpacityEffect(this);
	opacityEffect->setOpacity(0);
	opacityEffect->setEnabled(false);	// default disabled -> otherwise we get problems with children having the same effect
	setGraphicsEffect(opacityEffect);

	setVisible(false);
}

void DkFadeLabel::block(bool blocked) {
	mBlocked = blocked;
	setVisible(false);
}

void DkFadeLabel::registerAction(QAction* action) {
	connect(this, SIGNAL(visibleSignal(bool)), action, SLOT(setChecked(bool)));
}

void DkFadeLabel::setDisplaySettings(QBitArray* displayBits) {
	displaySettingsBits = displayBits;
}

bool DkFadeLabel::getCurrentDisplaySetting() {

	if (!displaySettingsBits)
		return false;

	if (DkSettings::app.currentAppMode < 0 || DkSettings::app.currentAppMode >= displaySettingsBits->size()) {
		qDebug() << "[WARNING] illegal app mode: " << DkSettings::app.currentAppMode;
		return false;
	}

	return displaySettingsBits->testBit(DkSettings::app.currentAppMode);
}

void DkFadeLabel::show(bool saveSettings) {

	if (!mBlocked && !showing) {
		hiding = false;
		showing = true;
		setVisible(true, saveSettings);
		animateOpacityUp();
	}
}

void DkFadeLabel::hide(bool saveSettings) {

	if (!hiding) {
		hiding = true;
		showing = false;
		animateOpacityDown();
	}

	if (saveSettings && displaySettingsBits && displaySettingsBits->size() > DkSettings::app.currentAppMode) {
		displaySettingsBits->setBit(DkSettings::app.currentAppMode, false);
	}
}

void DkFadeLabel::setVisible(bool visible, bool saveSettings) {

	if (mBlocked) {
		DkLabel::setVisible(false);
		return;
	}

	if (visible && !isVisible() && !showing)
		opacityEffect->setOpacity(100);

	emit visibleSignal(visible);
	DkLabel::setVisible(visible);

	if (saveSettings && displaySettingsBits && displaySettingsBits->size() > DkSettings::app.currentAppMode) {
		displaySettingsBits->setBit(DkSettings::app.currentAppMode, visible);
	}

}

void DkFadeLabel::animateOpacityUp() {

	if (!showing)
		return;

	opacityEffect->setEnabled(true);
	if (opacityEffect->opacity() >= 1.0f || !showing) {
		opacityEffect->setOpacity(1.0f);
		opacityEffect->setEnabled(false);
		showing = false;
		return;
	}

	QTimer::singleShot(20, this, SLOT(animateOpacityUp()));
	opacityEffect->setOpacity(opacityEffect->opacity()+0.05);
}

void DkFadeLabel::animateOpacityDown() {

	if (!hiding)
		return;

	opacityEffect->setEnabled(true);
	if (opacityEffect->opacity() <= 0.0f) {
		opacityEffect->setOpacity(0.0f);
		hiding = false;
		opacityEffect->setEnabled(false);
		setVisible(false, false);	// finally hide the widget
		return;
	}

	QTimer::singleShot(20, this, SLOT(animateOpacityDown()));
	opacityEffect->setOpacity(opacityEffect->opacity()-0.05);
}

// DkDockWidget --------------------------------------------------------------------
DkDockWidget::DkDockWidget(const QString& title, QWidget* parent /* = 0 */, Qt::WindowFlags flags /* = 0 */ ) : QDockWidget(title, parent, flags) {
	displaySettingsBits = 0;
	setObjectName("DkDockWidget");
}

DkDockWidget::~DkDockWidget() {
}

void DkDockWidget::registerAction(QAction* action) {
	connect(this, SIGNAL(visibleSignal(bool)), action, SLOT(setChecked(bool)));
}

void DkDockWidget::setDisplaySettings(QBitArray* displayBits) {
	displaySettingsBits = displayBits;
}

bool DkDockWidget::getCurrentDisplaySetting() const {

	if (!displaySettingsBits)
		return false;

	return testDisplaySettings(*displaySettingsBits);
}

bool DkDockWidget::testDisplaySettings(const QBitArray& displaySettingsBits) {

	if (DkSettings::app.currentAppMode < 0 || DkSettings::app.currentAppMode >= displaySettingsBits.size()) {
		qDebug() << "[WARNING] illegal app mode: " << DkSettings::app.currentAppMode;
		return false;
	}

	return displaySettingsBits.testBit(DkSettings::app.currentAppMode);
}

void DkDockWidget::setVisible(bool visible, bool saveSetting) {

	QDockWidget::setVisible(visible);
	emit visibleSignal(visible);	// if this gets slow -> put it into hide() or show()

	if (saveSetting && displaySettingsBits && displaySettingsBits->size() > DkSettings::app.currentAppMode) {
		displaySettingsBits->setBit(DkSettings::app.currentAppMode, visible);
	}
}

void DkDockWidget::closeEvent(QCloseEvent* event) {

	setVisible(false);

	QDockWidget::closeEvent(event);
}

Qt::DockWidgetArea DkDockWidget::getDockLocationSettings(const Qt::DockWidgetArea& defaultArea) const {
	
	QSettings& settings = Settings::instance().getSettings();
	Qt::DockWidgetArea location = (Qt::DockWidgetArea)settings.value(objectName(), defaultArea).toInt();

	return location;
}

// DkResizableScrollArea --------------------------------------------------------------------
DkResizableScrollArea::DkResizableScrollArea(QWidget * parent /* = 0 */) : QScrollArea(parent) {

}

bool DkResizableScrollArea::eventFilter(QObject * o, QEvent * e) {

	if(widget() && o == widget() && e->type() == QEvent::Resize) {

		updateSize();
	}

	return false;
}

void DkResizableScrollArea::updateSize() {

	if (!widget())
		return;

	updateGeometry();

	if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff) {
		
		int height = widget()->minimumSizeHint().height();

		if (horizontalScrollBar()->isVisible())
			height += horizontalScrollBar()->height();
		setMinimumHeight(height);
	}
	if (horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff) {
	
		int width = widget()->minimumSizeHint().width();

		if (verticalScrollBar()->isVisible())
			width += verticalScrollBar()->height();
		setMinimumWidth(width);
	}
}

QSize DkResizableScrollArea::sizeHint() const {

	if (!widget())
		return QScrollArea::sizeHint();

	widget()->updateGeometry();

	QSize s = QScrollArea::sizeHint();
	QSize ws = widget()->sizeHint();

	if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
		s.setHeight(ws.height());
	if (horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
		s.setWidth(ws.width());

	return s;
}

QSize DkResizableScrollArea::minimumSizeHint() const {

	if (!widget())
		return QScrollArea::minimumSizeHint();

	QSize s = QScrollArea::minimumSizeHint();
	QSize ws = widget()->minimumSizeHint();

	if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
		s.setHeight(ws.height());
	if (horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
		s.setWidth(ws.width());

	return s;
}

}