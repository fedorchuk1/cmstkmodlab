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
#include <ApplicationConfig.h>

#include <AssemblyAssembly.h>
#include <AssemblyUtilities.h>

#include <string>

#include <QStringList>

AssemblyAssembly::AssemblyAssembly(const LStepExpressMotionManager* const motion, const ConradManager* const vacuum, const AssemblySmartMotionManager* const smart_motion, QObject* parent)
 : QObject(parent)

 , motion_(motion)
 , vacuum_(vacuum)
 , smart_motion_(smart_motion)

 , vacuum_pickup_(0)
 , vacuum_spacer_(0)
 , vacuum_basepl_(0)

 , pickup1_dZ_(0.)
 , pickup2_dZ_(0.)

 , use_smartMove_(false)
{
  // validate pointers to controllers
  this->motion();
  this->vacuum();

  // indices of vacuum lines
  ApplicationConfig* config = ApplicationConfig::instance();
  if(config == nullptr)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal)
       << "ApplicationConfig::instance() not initialized (null pointer), stopped constructor";

    assembly::kill_application(tr("[AssemblyAssembly]"), tr("ApplicationConfig::instance() not initialized (null pointer), closing application"));
  }

  vacuum_pickup_ = config->getValue<int>("Vacuum_PickupTool");
  vacuum_spacer_ = config->getValue<int>("Vacuum_Spacers");
  vacuum_basepl_ = config->getValue<int>("Vacuum_Baseplate");

  // positive double(s) for vertical upward movement(s) for pickup
  pickup1_dZ_ = config->getValue<double>("AssemblyAssembly_pickup1_dZ");

  if(pickup1_dZ_ <= 0.)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal)
       << "invalid (non-positive) value for vertical upward movement for pickup #1 (dZ=" << pickup1_dZ_ << "), closing application";

    assembly::kill_application(tr("[AssemblyAssembly]"), "Invalid (non-positive) value for vertical upward movement for pickup #1 (dZ="+QString::number(pickup1_dZ_)+"), closing application");
  }

  pickup2_dZ_ = config->getValue<double>("AssemblyAssembly_pickup2_dZ");

  if(pickup2_dZ_ <= 0.)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal)
       << "invalid (non-positive) value for vertical upward movement for pickup #2 (dZ=" << pickup2_dZ_ << "), closing application";

    assembly::kill_application(tr("[AssemblyAssembly]"), "Invalid (non-positive) value for vertical upward movement for pickup #2 (dZ="+QString::number(pickup2_dZ_)+"), closing application");
  }
}

const LStepExpressMotionManager* AssemblyAssembly::motion() const
{
  if(motion_ == nullptr)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "motion"
       << ": pointer to LStepExpressMotionManager is NULL, exiting constructor";

    assembly::kill_application(tr("[AssemblyAssembly]"), tr("pointer to LStepExpressMotionManager is NULL, aborting"));
  }

  return motion_;
}

const ConradManager* AssemblyAssembly::vacuum() const
{
  if(motion_ == nullptr)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "vacuum"
       << ": pointer to ConradManager is NULL, exiting constructor";

    assembly::kill_application(tr("[AssemblyAssembly]"), tr("pointer to ConradManager is NULL, aborting"));
  }

  return vacuum_;
}

AssemblyParameters* AssemblyAssembly::parameters() const
{
  AssemblyParameters* const params = AssemblyParameters::instance();

  if(params == nullptr)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "parameters"
       << ": pointer to AssemblyParameters is NULL, exiting constructor";

    assembly::kill_application(tr("[AssemblyAssembly]"), tr("pointer to AssemblyParameters is NULL, aborting"));
  }

  return params;
}

void AssemblyAssembly::use_smartMove(const int state)
{
  if(state == 2)
  {
    if(smart_motion_ == nullptr)
    {
      NQLog("AssemblyAssembly", NQLog::Warning) << "use_smartMove(" << state << ")"
         << ": attemptin to enable smartMove option, but pointer to AssemblySmartMotionManager is invalid (NULL), smartMove option will stay OFF";

      use_smartMove_ = false;
    }
    else
    {
      NQLog("AssemblyAssembly", NQLog::Message) << "use_smartMove(" << state << ")"
         << ": smartMove option switched ON";

      use_smartMove_ = true;
    }
  }
  else if(state == 0)
  {
    NQLog("AssemblyAssembly", NQLog::Message) << "use_smartMove(" << state << ")"
       << ": smartMove option switched OFF";

    use_smartMove_ = false;
  }
  else
  {
    NQLog("AssemblyAssembly", NQLog::Warning) << "use_smartMove(" << state << ")"
       << ": invalid state value, no action taken (smartMove=" << use_smartMove_ << ")";
  }

  return;
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// GoToSensorMarkerPreAlignment -----------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::GoToSensorMarkerPreAlignment_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "GoToSensorMarkerPreAlignment_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_absolute_request(double, double, double, double)), motion_, SLOT(moveAbsolute(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(GoToSensorMarkerPreAlignment_finish()));

  const double x0 = this->parameters()->get("RefPointSensor_X");
  const double y0 = this->parameters()->get("RefPointSensor_Y");
  const double z0 = this->parameters()->get("RefPointSensor_Z");
  const double a0 = this->parameters()->get("RefPointSensor_A");

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoToSensorMarkerPreAlignment_start"
     << ": emitting signal \"move_absolute_request(" << x0 << ", " << y0 << ", " << z0 << ", " << a0 << ")\"";

  emit move_absolute_request(x0, y0, z0, a0);
}

void AssemblyAssembly::GoToSensorMarkerPreAlignment_finish()
{
  disconnect(this, SIGNAL(move_absolute_request(double, double, double, double)), motion_, SLOT(moveAbsolute(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(GoToSensorMarkerPreAlignment_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoToSensorMarkerPreAlignment_finish"
     << ": emitting signal \"GoToSensorMarkerPreAlignment_finished\"";

  emit GoToSensorMarkerPreAlignment_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "GoToSensorMarkerPreAlignment_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// EnableVacuumPickupTool -----------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::EnableVacuumPickupTool_start()
{
  connect(this, SIGNAL(vacuum_ON_request(int)), this->vacuum(), SLOT(enableVacuum(int)));

  connect(this->vacuum(), SIGNAL(vacuum_enabled()), this, SLOT(EnableVacuumPickupTool_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_toggled()), this, SLOT(EnableVacuumPickupTool_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_error  ()), this, SLOT(EnableVacuumPickupTool_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "EnableVacuumPickupTool_start"
     << ": emitting signal \"vacuum_ON_request(" << vacuum_pickup_ << ")\"";

  emit vacuum_ON_request(vacuum_pickup_);
}

void AssemblyAssembly::EnableVacuumPickupTool_finish()
{
  disconnect(this, SIGNAL(vacuum_ON_request(int)), this->vacuum(), SLOT(enableVacuum(int)));

  disconnect(this->vacuum(), SIGNAL(vacuum_enabled()), this, SLOT(EnableVacuumPickupTool_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_toggled()), this, SLOT(EnableVacuumPickupTool_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_error  ()), this, SLOT(EnableVacuumPickupTool_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "EnableVacuumPickupTool_finish"
     << ": emitting signal \"EnableVacuumPickupTool_finished\"";

  emit EnableVacuumPickupTool_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "EnableVacuumPickupTool_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// DisableVacuumPickupTool ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::DisableVacuumPickupTool_start()
{
  connect(this, SIGNAL(vacuum_OFF_request(int)), this->vacuum(), SLOT(disableVacuum(int)));

  connect(this->vacuum(), SIGNAL(vacuum_disabled()), this, SLOT(DisableVacuumPickupTool_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_toggled ()), this, SLOT(DisableVacuumPickupTool_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_error   ()), this, SLOT(DisableVacuumPickupTool_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "DisableVacuumPickupTool_start"
     << ": emitting signal \"vacuum_OFF_request(" << vacuum_pickup_ << ")\"";

  emit vacuum_OFF_request(vacuum_pickup_);
}

void AssemblyAssembly::DisableVacuumPickupTool_finish()
{
  disconnect(this, SIGNAL(vacuum_OFF_request(int)), this->vacuum(), SLOT(disableVacuum(int)));

  disconnect(this->vacuum(), SIGNAL(vacuum_disabled()), this, SLOT(DisableVacuumPickupTool_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_toggled ()), this, SLOT(DisableVacuumPickupTool_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_error   ()), this, SLOT(DisableVacuumPickupTool_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "DisableVacuumPickupTool_finish"
     << ": emitting signal \"DisableVacuumPickupTool_finished\"";

  emit DisableVacuumPickupTool_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "DisableVacuumPickupTool_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// EnableVacuumSpacers --------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::EnableVacuumSpacers_start()
{
  connect(this, SIGNAL(vacuum_ON_request(int)), this->vacuum(), SLOT(enableVacuum(int)));

  connect(this->vacuum(), SIGNAL(vacuum_enabled()), this, SLOT(EnableVacuumSpacers_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_toggled()), this, SLOT(EnableVacuumSpacers_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_error  ()), this, SLOT(EnableVacuumSpacers_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "EnableVacuumSpacers_start"
     << ": emitting signal \"vacuum_ON_request(" << vacuum_spacer_ << ")\"";

  emit vacuum_ON_request(vacuum_spacer_);
}

void AssemblyAssembly::EnableVacuumSpacers_finish()
{
  disconnect(this, SIGNAL(vacuum_ON_request(int)), this->vacuum(), SLOT(enableVacuum(int)));

  disconnect(this->vacuum(), SIGNAL(vacuum_enabled()), this, SLOT(EnableVacuumSpacers_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_toggled()), this, SLOT(EnableVacuumSpacers_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_error  ()), this, SLOT(EnableVacuumSpacers_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "EnableVacuumSpacers_finish"
     << ": emitting signal \"EnableVacuumSpacers_finished\"";

  emit EnableVacuumSpacers_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "EnableVacuumSpacers_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// DisableVacuumSpacers -------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::DisableVacuumSpacers_start()
{
  connect(this, SIGNAL(vacuum_OFF_request(int)), this->vacuum(), SLOT(disableVacuum(int)));

  connect(this->vacuum(), SIGNAL(vacuum_disabled()), this, SLOT(DisableVacuumSpacers_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_toggled ()), this, SLOT(DisableVacuumSpacers_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_error   ()), this, SLOT(DisableVacuumSpacers_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "DisableVacuumSpacers_start"
     << ": emitting signal \"vacuum_OFF_request(" << vacuum_spacer_ << ")\"";

  emit vacuum_OFF_request(vacuum_spacer_);
}

void AssemblyAssembly::DisableVacuumSpacers_finish()
{
  disconnect(this, SIGNAL(vacuum_OFF_request(int)), this->vacuum(), SLOT(disableVacuum(int)));

  disconnect(this->vacuum(), SIGNAL(vacuum_disabled()), this, SLOT(DisableVacuumSpacers_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_toggled ()), this, SLOT(DisableVacuumSpacers_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_error   ()), this, SLOT(DisableVacuumSpacers_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "DisableVacuumSpacers_finish"
     << ": emitting signal \"DisableVacuumSpacers_finished\"";

  emit DisableVacuumSpacers_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "DisableVacuumSpacers_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// EnableVacuumBaseplate ------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::EnableVacuumBaseplate_start()
{
  connect(this, SIGNAL(vacuum_ON_request(int)), this->vacuum(), SLOT(enableVacuum(int)));

  connect(this->vacuum(), SIGNAL(vacuum_enabled()), this, SLOT(EnableVacuumBaseplate_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_toggled()), this, SLOT(EnableVacuumBaseplate_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_error  ()), this, SLOT(EnableVacuumBaseplate_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "EnableVacuumBaseplate_start"
     << ": emitting signal \"vacuum_ON_request(" << vacuum_basepl_ << ")\"";

  emit vacuum_ON_request(vacuum_basepl_);
}

void AssemblyAssembly::EnableVacuumBaseplate_finish()
{
  disconnect(this, SIGNAL(vacuum_ON_request(int)), this->vacuum(), SLOT(enableVacuum(int)));

  disconnect(this->vacuum(), SIGNAL(vacuum_enabled()), this, SLOT(EnableVacuumBaseplate_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_toggled()), this, SLOT(EnableVacuumBaseplate_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_error  ()), this, SLOT(EnableVacuumBaseplate_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "EnableVacuumBaseplate_finish"
     << ": emitting signal \"EnableVacuumBaseplate_finished\"";

  emit EnableVacuumBaseplate_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "EnableVacuumBaseplate_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// DisableVacuumBaseplate -----------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::DisableVacuumBaseplate_start()
{
  connect(this, SIGNAL(vacuum_OFF_request(int)), this->vacuum(), SLOT(disableVacuum(int)));

  connect(this->vacuum(), SIGNAL(vacuum_disabled()), this, SLOT(DisableVacuumBaseplate_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_toggled ()), this, SLOT(DisableVacuumBaseplate_finish()));
  connect(this->vacuum(), SIGNAL(vacuum_error   ()), this, SLOT(DisableVacuumBaseplate_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "DisableVacuumBaseplate_start"
     << ": emitting signal \"vacuum_OFF_request(" << vacuum_basepl_ << ")\"";

  emit vacuum_OFF_request(vacuum_basepl_);
}

void AssemblyAssembly::DisableVacuumBaseplate_finish()
{
  disconnect(this, SIGNAL(vacuum_OFF_request(int)), this->vacuum(), SLOT(disableVacuum(int)));

  disconnect(this->vacuum(), SIGNAL(vacuum_disabled()), this, SLOT(DisableVacuumBaseplate_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_toggled ()), this, SLOT(DisableVacuumBaseplate_finish()));
  disconnect(this->vacuum(), SIGNAL(vacuum_error   ()), this, SLOT(DisableVacuumBaseplate_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "DisableVacuumBaseplate_finish"
     << ": emitting signal \"DisableVacuumBaseplate_finished\"";

  emit DisableVacuumBaseplate_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "DisableVacuumBaseplate_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// GoFromSensorMarkerToPickupXY -----------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::GoFromSensorMarkerToPickupXY_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "GoFromSensorMarkerToPickupXY_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(GoFromSensorMarkerToPickupXY_finish()));

  const double dx0 = this->parameters()->get("FromSensorRefPointToSensorPickup_dX");
  const double dy0 = this->parameters()->get("FromSensorRefPointToSensorPickup_dY");
  const double dz0 = 0.0;
  const double da0 = 0.0;

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoFromSensorMarkerToPickupXY_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::GoFromSensorMarkerToPickupXY_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(GoFromSensorMarkerToPickupXY_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoFromSensorMarkerToPickupXY_finish"
     << ": emitting signal \"GoFromSensorMarkerToPickupXY_finished\"";

  emit GoFromSensorMarkerToPickupXY_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "GoFromSensorMarkerToPickupXY_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// LowerPickupToolOntoPSS -----------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::LowerPickupToolOntoPSS_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "LowerPickupToolOntoPSS_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  const double dx0 = 0.0;
  const double dy0 = 0.0;
  const double da0 = 0.0;

  const double dz0 =
    - motion_->get_position_Z()
    + this->parameters()->get("Thickness_PSS")
    + this->parameters()->get("Thickness_VacuumCups")
    + this->parameters()->get("PickupToolOnRotStage_Z")
  ;

  if(use_smartMove_)
  {
    connect(this, SIGNAL(move_relative_request(double, double, double, double)), smart_motion_, SLOT(move_relative(double, double, double, double)));
    connect(smart_motion_, SIGNAL(motion_completed()), this, SLOT(LowerPickupToolOntoPSS_finish()));
  }
  else
  {
    connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
    connect(motion_, SIGNAL(motion_finished()), this, SLOT(LowerPickupToolOntoPSS_finish()));
  }

  NQLog("AssemblyAssembly", NQLog::Spam) << "LowerPickupToolOntoPSS_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::LowerPickupToolOntoPSS_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), smart_motion_, SLOT(move_relative(double, double, double, double)));
  disconnect(smart_motion_, SIGNAL(motion_completed()), this, SLOT(LowerPickupToolOntoPSS_finish()));

  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(LowerPickupToolOntoPSS_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "LowerPickupToolOntoPSS_finish"
     << ": emitting signal \"LowerPickupToolOntoPSS_finished\"";

  emit LowerPickupToolOntoPSS_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "LowerPickupToolOntoPSS_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// PickupPSS ------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::PickupPSS_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "PickupPSS_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(PickupPSS_finish()));

  const double dx0 = 0.0;
  const double dy0 = 0.0;
  const double dz0 = pickup1_dZ_;
  const double da0 = 0.0;

  NQLog("AssemblyAssembly", NQLog::Spam) << "PickupPSS_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::PickupPSS_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(PickupPSS_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "PickupPSS_finish"
     << ": emitting signal \"PickupPSS_finished\"";

  emit PickupPSS_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "PickupPSS_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// GoToSpacerRefPoint ---------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::GoToSpacerRefPoint_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "GoToSpacerRefPoint_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_absolute_request(double, double, double, double)), motion_, SLOT(moveAbsolute(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(GoToSpacerRefPoint_finish()));

  const double x0 = this->parameters()->get("RefPointSpacer_X");
  const double y0 = this->parameters()->get("RefPointSpacer_Y");
  const double z0 = this->parameters()->get("RefPointSpacer_Z");
  const double a0 = this->parameters()->get("RefPointSpacer_A");

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoToSpacerRefPoint_start"
     << ": emitting signal \"move_absolute_request(" << x0 << ", " << y0 << ", " << z0 << ", " << a0 << ")\"";

  emit move_absolute_request(x0, y0, z0, a0);
}

void AssemblyAssembly::GoToSpacerRefPoint_finish()
{
  disconnect(this, SIGNAL(move_absolute_request(double, double, double, double)), motion_, SLOT(moveAbsolute(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(GoToSpacerRefPoint_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoToSpacerRefPoint_finish"
     << ": emitting signal \"GoToSpacerRefPoint_finished\"";

  emit GoToSpacerRefPoint_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "GoToSpacerRefPoint_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// GoFromSpacerRefPointToSpacerGluingXYPosition -------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::GoFromSpacerRefPointToSpacerGluingXYPosition_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "GoFromSpacerRefPointToSpacerGluingXYPosition_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(GoFromSpacerRefPointToSpacerGluingXYPosition_finish()));

  const double dx0 = this->parameters()->get("FromSpacerRefPointToSensorRefPoint_dX") + this->parameters()->get("FromSensorRefPointToSensorPickup_dX");
  const double dy0 = this->parameters()->get("FromSpacerRefPointToSensorRefPoint_dY") + this->parameters()->get("FromSensorRefPointToSensorPickup_dY");
  const double dz0 = 0.0;
  const double da0 = 0.0;

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoFromSpacerRefPointToSpacerGluingXYPosition_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::GoFromSpacerRefPointToSpacerGluingXYPosition_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(GoFromSpacerRefPointToSpacerGluingXYPosition_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoFromSpacerRefPointToSpacerGluingXYPosition_finish"
     << ": emitting signal \"GoFromSpacerRefPointToSpacerGluingXYPosition_finished\"";

  emit GoFromSpacerRefPointToSpacerGluingXYPosition_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "GoFromSpacerRefPointToSpacerGluingXYPosition_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// LowerPSSOntoSpacers --------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::LowerPSSOntoSpacers_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "LowerPSSOntoSpacers_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  const double dx0 = 0.0;
  const double dy0 = 0.0;
  const double da0 = 0.0;

  const double dz0 =
    - motion_->get_position_Z()
    + this->parameters()->get("Thickness_VacuumCups")
    + this->parameters()->get("Thickness_PSS")
    + this->parameters()->get("Thickness_GlueLayer")
    + this->parameters()->get("Thickness_Spacer") - this->parameters()->get("Height_SpacerSlots")
    + this->parameters()->get("PickupToolOnRotStage_Z")
  ;

  if(use_smartMove_)
  {
    connect(this, SIGNAL(move_relative_request(double, double, double, double)), smart_motion_, SLOT(move_relative(double, double, double, double)));
    connect(smart_motion_, SIGNAL(motion_completed()), this, SLOT(LowerPSSOntoSpacers_finish()));
  }
  else
  {
    connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
    connect(motion_, SIGNAL(motion_finished()), this, SLOT(LowerPSSOntoSpacers_finish()));
  }

  NQLog("AssemblyAssembly", NQLog::Spam) << "LowerPSSOntoSpacers_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::LowerPSSOntoSpacers_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), smart_motion_, SLOT(move_relative(double, double, double, double)));
  disconnect(smart_motion_, SIGNAL(motion_completed()), this, SLOT(LowerPSSOntoSpacers_finish()));

  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(LowerPSSOntoSpacers_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "LowerPSSOntoSpacers_finish"
     << ": emitting signal \"LowerPSSOntoSpacers_finished\"";

  emit LowerPSSOntoSpacers_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "LowerPSSOntoSpacers_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// ApplyPSPToPSSXYOffset ------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::ApplyPSPToPSSXYOffset_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "ApplyPSPToPSSXYOffset_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(ApplyPSPToPSSXYOffset_finish()));

  const double dx0 = this->parameters()->get("FromPSPRefPointToPSSRefPoint_dX");
  const double dy0 = this->parameters()->get("FromPSPRefPointToPSSRefPoint_dY");
  const double dz0 = 0.0;
  const double da0 = 0.0;

  NQLog("AssemblyAssembly", NQLog::Spam) << "ApplyPSPToPSSXYOffset_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::ApplyPSPToPSSXYOffset_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(ApplyPSPToPSSXYOffset_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "ApplyPSPToPSSXYOffset_finish"
     << ": emitting signal \"ApplyPSPToPSSXYOffset_finished\"";

  emit ApplyPSPToPSSXYOffset_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "ApplyPSPToPSSXYOffset_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// LowerSpacersAndPSSOntoPSP --------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::LowerSpacersAndPSSOntoPSP_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "LowerSpacersAndPSSOntoPSP_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  const double dx0 = 0.0;
  const double dy0 = 0.0;
  const double da0 = 0.0;

  const double dz0 =
    - motion_->get_position_Z()
    + this->parameters()->get("Thickness_VacuumCups")
    + this->parameters()->get("Thickness_PSS")
    + this->parameters()->get("Thickness_GlueLayer")
    + this->parameters()->get("Thickness_Spacer")
    + this->parameters()->get("Thickness_GlueLayer")
    + this->parameters()->get("Thickness_PSP")
    + this->parameters()->get("Thickness_VacuumCups")
    + this->parameters()->get("PickupToolOnRotStage_Z")
  ;

  if(use_smartMove_)
  {
    connect(this, SIGNAL(move_relative_request(double, double, double, double)), smart_motion_, SLOT(move_relative(double, double, double, double)));
    connect(smart_motion_, SIGNAL(motion_completed()), this, SLOT(LowerSpacersAndPSSOntoPSP_finish()));
  }
  else
  {
    connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
    connect(motion_, SIGNAL(motion_finished()), this, SLOT(LowerSpacersAndPSSOntoPSP_finish()));
  }

  NQLog("AssemblyAssembly", NQLog::Spam) << "LowerSpacersAndPSSOntoPSP_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::LowerSpacersAndPSSOntoPSP_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), smart_motion_, SLOT(move_relative(double, double, double, double)));
  disconnect(smart_motion_, SIGNAL(motion_completed()), this, SLOT(LowerSpacersAndPSSOntoPSP_finish()));

  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(LowerSpacersAndPSSOntoPSP_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "LowerSpacersAndPSSOntoPSP_finish"
     << ": emitting signal \"LowerSpacersAndPSSOntoPSP_finished\"";

  emit LowerSpacersAndPSSOntoPSP_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "LowerSpacersAndPSSOntoPSP_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// PickupSpacersAndPSS --------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::PickupSpacersAndPSS_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "PickupSpacersAndPSS_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(PickupSpacersAndPSS_finish()));

  const double dx0 = 0.0;
  const double dy0 = 0.0;
  const double dz0 = pickup1_dZ_;
  const double da0 = 0.0;

  NQLog("AssemblyAssembly", NQLog::Spam) << "PickupSpacersAndPSS_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::PickupSpacersAndPSS_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(PickupSpacersAndPSS_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "PickupSpacersAndPSS_finish"
     << ": emitting signal \"PickupSpacersAndPSS_finished\"";

  emit PickupSpacersAndPSS_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "PickupSpacersAndPSS_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// LiftUpPickupTool -----------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::LiftUpPickupTool_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "LiftUpPickupTool_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(LiftUpPickupTool_finish()));

  const double dx0 = 0.0;
  const double dy0 = 0.0;
  const double dz0 = pickup2_dZ_;
  const double da0 = 0.0;

  NQLog("AssemblyAssembly", NQLog::Spam) << "LiftUpPickupTool_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::LiftUpPickupTool_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(LiftUpPickupTool_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "LiftUpPickupTool_finish"
     << ": emitting signal \"LiftUpPickupTool_finished\"";

  emit LiftUpPickupTool_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "LiftUpPickupTool_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// PickupPSPAndPSS ------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::PickupPSPAndPSS_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "PickupPSPAndPSS_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(PickupPSPAndPSS_finish()));

  const double dx0 = 0.0;
  const double dy0 = 0.0;
  const double dz0 = pickup1_dZ_;
  const double da0 = 0.0;

  NQLog("AssemblyAssembly", NQLog::Spam) << "PickupPSPAndPSS_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::PickupPSPAndPSS_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(PickupPSPAndPSS_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "PickupPSPAndPSS_finish"
     << ": emitting signal \"PickupPSPAndPSS_finished\"";

  emit PickupPSPAndPSS_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "PickupPSPAndPSS_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// GoToBaseplateRefPoint ---------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::GoToBaseplateRefPoint_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "GoToBaseplateRefPoint_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_absolute_request(double, double, double, double)), motion_, SLOT(moveAbsolute(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(GoToBaseplateRefPoint_finish()));

  const double x0 = this->parameters()->get("RefPointBaseplate_X");
  const double y0 = this->parameters()->get("RefPointBaseplate_Y");
  const double z0 = this->parameters()->get("RefPointBaseplate_Z");
  const double a0 = this->parameters()->get("RefPointBaseplate_A");

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoToBaseplateRefPoint_start"
     << ": emitting signal \"move_absolute_request(" << x0 << ", " << y0 << ", " << z0 << ", " << a0 << ")\"";

  emit move_absolute_request(x0, y0, z0, a0);
}

void AssemblyAssembly::GoToBaseplateRefPoint_finish()
{
  disconnect(this, SIGNAL(move_absolute_request(double, double, double, double)), motion_, SLOT(moveAbsolute(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(GoToBaseplateRefPoint_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoToBaseplateRefPoint_finish"
     << ": emitting signal \"GoToBaseplateRefPoint_finished\"";

  emit GoToBaseplateRefPoint_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "GoToBaseplateRefPoint_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// GoFromBaseplateRefPointToBaseplateGluingXYPosition -------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::GoFromBaseplateRefPointToBaseplateGluingXYPosition_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "GoFromBaseplateRefPointToBaseplateGluingXYPosition_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  connect(motion_, SIGNAL(motion_finished()), this, SLOT(GoFromBaseplateRefPointToBaseplateGluingXYPosition_finish()));

  const double dx0 = this->parameters()->get("FromBaseplateRefPointToPSPRefPoint_dX") + this->parameters()->get("FromPSPRefPointToPSSRefPoint_dX") +this->parameters()->get("FromSensorRefPointToSensorPickup_dX");
  const double dy0 = this->parameters()->get("FromBaseplateRefPointToPSPRefPoint_dY") + this->parameters()->get("FromPSPRefPointToPSSRefPoint_dY") +this->parameters()->get("FromSensorRefPointToSensorPickup_dY");
  const double dz0 = 0.0;
  const double da0 = 0.0;

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoFromBaseplateRefPointToBaseplateGluingXYPosition_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::GoFromBaseplateRefPointToBaseplateGluingXYPosition_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(GoFromBaseplateRefPointToBaseplateGluingXYPosition_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "GoFromBaseplateRefPointToBaseplateGluingXYPosition_finish"
     << ": emitting signal \"GoFromBaseplateRefPointToBaseplateGluingXYPosition_finished\"";

  emit GoFromBaseplateRefPointToBaseplateGluingXYPosition_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "GoFromBaseplateRefPointToBaseplateGluingXYPosition_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------
// LowerSensorAssemblyOntoBaseplate -------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
void AssemblyAssembly::LowerSensorAssemblyOntoBaseplate_start()
{
  const bool valid_params = this->parameters()->update();

  if(valid_params == false)
  {
    NQLog("AssemblyAssembly", NQLog::Fatal) << "LowerSensorAssemblyOntoBaseplate_start"
       << ": failed to update content of AssemblyParameters, no action taken";

    return;
  }

  const double dx0 = 0.0;
  const double dy0 = 0.0;
  const double da0 = 0.0;

  const double dz0 =
    - motion_->get_position_Z()
    + this->parameters()->get("Thickness_VacuumCups")
    + this->parameters()->get("Thickness_PSS")
    + this->parameters()->get("Thickness_GlueLayer")
    + this->parameters()->get("Thickness_Spacer")
    + this->parameters()->get("Thickness_GlueLayer")
    + this->parameters()->get("Thickness_PSP")
    + this->parameters()->get("Thickness_GlueLayer")
    + this->parameters()->get("Thickness_Baseplate")
    + this->parameters()->get("Thickness_VacuumCups")
    + this->parameters()->get("PickupToolOnRotStage_Z")
  ;

  if(use_smartMove_)
  {
    connect(this, SIGNAL(move_relative_request(double, double, double, double)), smart_motion_, SLOT(move_relative(double, double, double, double)));
    connect(smart_motion_, SIGNAL(motion_completed()), this, SLOT(LowerSensorAssemblyOntoBaseplate_finish()));
  }
  else
  {
    connect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
    connect(motion_, SIGNAL(motion_finished()), this, SLOT(LowerSensorAssemblyOntoBaseplate_finish()));
  }

  NQLog("AssemblyAssembly", NQLog::Spam) << "LowerSensorAssemblyOntoBaseplate_start"
     << ": emitting signal \"move_relative_request(" << dx0 << ", " << dy0 << ", " << dz0 << ", " << da0 << ")\"";

  emit move_relative_request(dx0, dy0, dz0, da0);
}

void AssemblyAssembly::LowerSensorAssemblyOntoBaseplate_finish()
{
  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), smart_motion_, SLOT(move_relative(double, double, double, double)));
  disconnect(smart_motion_, SIGNAL(motion_completed()), this, SLOT(LowerSensorAssemblyOntoBaseplate_finish()));

  disconnect(this, SIGNAL(move_relative_request(double, double, double, double)), motion_, SLOT(moveRelative(double, double, double, double)));
  disconnect(motion_, SIGNAL(motion_finished()), this, SLOT(LowerSensorAssemblyOntoBaseplate_finish()));

  NQLog("AssemblyAssembly", NQLog::Spam) << "LowerSensorAssemblyOntoBaseplate_finish"
     << ": emitting signal \"LowerSensorAssemblyOntoBaseplate_finished\"";

  emit LowerSensorAssemblyOntoBaseplate_finished();

  NQLog("AssemblyAssembly", NQLog::Message) << "LowerSensorAssemblyOntoBaseplate_finish"
     << ": assembly-step completed";
}
// ----------------------------------------------------------------------------------------------------
