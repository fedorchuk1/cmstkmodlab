/////////////////////////////////////////////////////////////////////////////////
//                                                                             //
//               Copyright (C) 2011-2017 - The DESY CMS Group                  //
//                           All rights reserved                               //
//                                                                             //
//      The CMStkModLab source code is licensed under the GNU GPL v3.0.        //
//      You have the right to modify and/or redistribute this source code      //
//      under the terms specified in the license, which may be found online    //
//      at http://www.gnu.org/licenses or at License.txt.                      //
//                                                                             //
/////////////////////////////////////////////////////////////////////////////////

#include <nqlogger.h>

#include "ApplicationConfig.h"

#include "LStepExpressJoystickWidget.h"

LStepExpressJoystickWidget::LStepExpressJoystickWidget(LStepExpressModel* model,
                                                       QWidget *parent)
    : QWidget(parent),
      model_(model)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    setLayout(layout);

    axisControlWidget_= new QWidget(this);
    layout->addWidget(axisControlWidget_);

    QGridLayout* axisLayout = new QGridLayout(axisControlWidget_);
    axisControlWidget_->setLayout(axisLayout);

    // Add all the axes displays
    for (unsigned int i=0;i<4;++i) {
        axisLayout->addWidget(new LStepExpressJoystickAxisWidget(model_, i, this),
                              0, i);
    }

    connect(model_, SIGNAL(deviceStateChanged(State)),
            this, SLOT(lstepStateChanged(State)));

    connect(model_, SIGNAL(controlStateChanged(bool)),
            this, SLOT(controlStateChanged(bool)));

    connect(model_, SIGNAL(motionStarted()),
            this, SLOT(motionStarted()));

    connect(model_, SIGNAL(motionFinished()),
            this, SLOT(motionFinished()));

    lstepStateChanged(model_->getDeviceState());
}

LStepExpressJoystickWidget::~LStepExpressJoystickWidget()
{
}

void LStepExpressJoystickWidget::lstepStateChanged(State newState)
{
  //    NQLog("LStepExpressJoystickWidget", NQLog::Spam)<<" lStepStateChanged(State newState) " << newState    ;

  axisControlWidget_->setEnabled(newState == READY);
}

/// Updates the GUI when the controler is enabled/disabled.
void LStepExpressJoystickWidget::controlStateChanged(bool enabled)
{
  //    NQLog("LStepExpressJoystickWidget", NQLog::Spam)<<" controlStateChanged(bool enabled) " << enabled    ;

    if (enabled) {
        lstepStateChanged(model_->getDeviceState());
    } else {
        axisControlWidget_->setEnabled(false);
    }
}

void LStepExpressJoystickWidget::motionStarted()
{
  //  NQLog("LStepExpressJoystickWidget", NQLog::Spam) << " motionStarted()"    ;
}

void LStepExpressJoystickWidget::motionFinished()
{
  //  NQLog("LStepExpressJoystickWidget", NQLog::Spam)<<" motionFinished()"    ;
}

LStepExpressJoystickAxisWidget::LStepExpressJoystickAxisWidget(LStepExpressModel* model,
                                                               unsigned int axis,
                                                               QWidget *parent)
    : QWidget(parent),
      model_(model),
      axis_(axis)
{
    layout_ = new QVBoxLayout(this);
    setLayout(layout_);

    stepBox_ = new QDoubleSpinBox(this);
    stepBox_->setSuffix(" usteps");
    stepBox_->setMinimum(0.0000);
    stepBox_->setMaximum(10.0000);
    stepBox_->setValue(1.0);
    layout_->addWidget(stepBox_);

    upButton_ = new QPushButton(QIcon(QString(Config::CMSTkModLabBasePath.c_str())+"/share/icon_UP.png"),
                                "", this);
    layout_->addWidget(upButton_);
    connect(upButton_, SIGNAL(clicked()),
            this, SLOT(upButtonClicked()));

    downButton_ = new QPushButton(QIcon(QString(Config::CMSTkModLabBasePath.c_str())+"/share/icon_DOWN.png"),
                                  "", this);
    layout_->addWidget(downButton_);
    connect(downButton_, SIGNAL(clicked()),
            this, SLOT(downButtonClicked()));

    connect(model_, SIGNAL(deviceStateChanged(State)),
            this, SLOT(lStepStateChanged(State)));

    connect(model_, SIGNAL(controlStateChanged(bool)),
            this, SLOT(controlStateChanged(bool)));

    connect(model_, SIGNAL(informationChanged()),
            this, SLOT(updateWidgets()));

    connect(model_, SIGNAL(motionInformationChanged()),
            this, SLOT(updateMotionWidgets()));

    connect(model_, SIGNAL(motionStarted()),
            this, SLOT(motionStarted()));

    connect(model_, SIGNAL(motionFinished()),
            this, SLOT(motionFinished()));

}

LStepExpressJoystickAxisWidget::~LStepExpressJoystickAxisWidget()
{
    if(layout_){delete layout_; layout_ = nullptr;}
    if(upButton_){delete upButton_; upButton_ = nullptr;}
    if(downButton_){delete downButton_; downButton_ = nullptr;}
    if(stepBox_){delete stepBox_; stepBox_ = nullptr;}
}

void LStepExpressJoystickAxisWidget::updateWidgets()
{
    bool axis = model_->getAxisEnabled(axis_);

    //NQLog("LStepExpressJoystickAxisWidget", NQLog::Spam)<<" updateWidgets() " << axis    ;

    stepBox_->setSuffix(QString(" ") + model_->getAxisDimensionShortName(axis_));

    if (model_->getAxisEnabled(axis_)) {
        stepBox_->setEnabled(true);
        upButton_->setEnabled(true);
        downButton_->setEnabled(true);
    } else {
        stepBox_->setEnabled(false);
        upButton_->setEnabled(false);
        downButton_->setEnabled(false);
    }
}

void LStepExpressJoystickAxisWidget::updateMotionWidgets()
{
    // NQLog("LStepExpressJoystickAxisWidget updateMotionWidgets()"    ;
}

void LStepExpressJoystickAxisWidget::lStepStateChanged(State newState)
{
  //NQLog("LStepExpressJoystickAxisWidget", NQLog::Spam)<<" lStepStateChanged(State newState) " << newState    ;
  // NQLog("LStepExpressJoystickAxisWidget", NQLog::Spam)<<"                              axis " << model_->getAxisEnabled(axis_)    ;

    if (newState == READY || newState == INITIALIZING) {
        if (model_->getAxisEnabled(axis_)) {
            stepBox_->setEnabled(true);
            upButton_->setEnabled(true);
            downButton_->setEnabled(true);
        } else {
            stepBox_->setEnabled(false);
            upButton_->setEnabled(false);
            downButton_->setEnabled(false);
        }
    } else {
        stepBox_->setEnabled(false);
        upButton_->setEnabled(false);
        downButton_->setEnabled(false);
    }
}

void LStepExpressJoystickAxisWidget::controlStateChanged(bool enabled)
{
  //    NQLog("LStepExpressJoystickAxisWidget", NQLog::Spam)<<" controlStateChanged(bool enabled) " << enabled    ;

    if (enabled) {
        lStepStateChanged(model_->getDeviceState());
    } else {
        upButton_->setEnabled(false);
        downButton_->setEnabled(false);
    }
}

void LStepExpressJoystickAxisWidget::upButtonClicked()
{
  //    NQLog("LStepExpressJoystickAxisWidget", NQLog::Spam)<<" upButtonClicked()"    ;

    double value = stepBox_->value();
    model_->moveRelative(axis_, value);
}

void LStepExpressJoystickAxisWidget::downButtonClicked()
{
  //    NQLog("LStepExpressJoystickAxisWidget", NQLog::Spam)<<" downButtonClicked()"    ;

    double value = -stepBox_->value();
    model_->moveRelative(axis_, value);
}

void LStepExpressJoystickAxisWidget::motionStarted()
{
  //    NQLog("LStepExpressJoystickAxisWidget", NQLog::Spam)<<" motionStarted()"    ;
}

void LStepExpressJoystickAxisWidget::motionFinished()
{
  //    NQLog("LStepExpressJoystickAxisWidget", NQLog::Spam)<<" motionFinished()"    ;
}
