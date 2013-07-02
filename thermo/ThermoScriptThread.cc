#include <QDebug>

#include "ThermoScriptThread.h"
#include "ThermoScriptableGlobals.h"
#include "ScriptableJulabo.h"
#include "ScriptableKeithley.h"
#include "ScriptableHameg.h"
#include "ScriptablePfeiffer.h"

ThermoScriptThread::ThermoScriptThread(ThermoScriptModel* scriptModel,
                                       JulaboModel* julaboModel,
                                       KeithleyModel* keithleyModel,
                                       HamegModel* hamegModel,
                                       PfeifferModel* pfeifferModel,
                                       QObject *parent) :
    QThread(parent),
    scriptModel_(scriptModel),
    julaboModel_(julaboModel),
    keithleyModel_(keithleyModel),
    hamegModel_(hamegModel),
    pfeifferModel_(pfeifferModel)
{

}

void ThermoScriptThread::executeScript(const QString & script)
{
  script_ = script;

  engine_ = new QScriptEngine();
  engine_->setProcessEventsInterval(1000);

  ThermoScriptableGlobals *globalsObj = new ThermoScriptableGlobals(scriptModel_, this);
  QScriptValue globalsValue = engine_->newQObject(globalsObj);
  engine_->globalObject().setProperty("thermo", globalsValue);

  ScriptableJulabo *julaboObj = new ScriptableJulabo(julaboModel_, this);
  QScriptValue julaboValue = engine_->newQObject(julaboObj);
  engine_->globalObject().setProperty("julabo", julaboValue);

  ScriptableKeithley *keithleyObj = new ScriptableKeithley(keithleyModel_, this);
  QScriptValue keithleyValue = engine_->newQObject(keithleyObj);
  engine_->globalObject().setProperty("keithley", keithleyValue);

  ScriptableHameg *hamegObj = new ScriptableHameg(hamegModel_, this);
  QScriptValue hamegValue = engine_->newQObject(hamegObj);
  engine_->globalObject().setProperty("hameg", hamegValue);

  ScriptablePfeiffer *pfeifferObj = new ScriptablePfeiffer(pfeifferModel_, this);
  QScriptValue pfeifferValue = engine_->newQObject(pfeifferObj);
  engine_->globalObject().setProperty("pfeiffer", pfeifferValue);

  start();
}

void ThermoScriptThread::abortScript() {
  std::cout << "abort" << std::endl;
  if (engine_) {
    std::cout << "abort " << (int)engine_->isEvaluating() << std::endl;
    engine_->abortEvaluation();
    //delete engine_;
    //engine_ = 0;
    //terminate();
  }
}

void ThermoScriptThread::run() {

  if (engine_->canEvaluate(script_)) {
    QScriptValue fun = engine_->evaluate(script_);
    //QScriptContext * context = engine_->pushContext();
    //QScriptValue v = context->activationObject();
    //v.setProperty("fun", fun);
    //engine_->evaluate("fun()");
    //engine_->popContext();
    delete engine_;
    engine_ = 0;
  } else {
    qDebug() << script_;
  }
}