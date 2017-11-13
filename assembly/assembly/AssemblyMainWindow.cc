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

#include <AssemblyMainWindow.h>
#include <ApplicationConfig.h>
#include <nqlogger.h>
#include <Util.h>

#include <string>

#include <QApplication>
#include <QString>

#include <opencv2/opencv.hpp>

AssemblyMainWindow::AssemblyMainWindow(const unsigned int camera_ID, QWidget* parent) :
  QMainWindow(parent),

  toolBar_(0),
  tabWidget_(0),

//  finderView_(0),
//  edgeView_(0),
//  rawView_(0),
  autoFocusView_(0),
  thresholdTunerView_(0),
  assembleView_(0),

  checkbox1(0),
  checkbox2(0),
  checkbox3(0),
  checkbox4(0),

  motion_model_(0),
  motion_manager_(0),
  motion_manager_view_(0),
  motion_thread_(0),
  motionSettings_(0),
  motionSettingsWidget_(0),

  camera_model_(0),
  camera_thread_(0),
//  camera_widget_(0),
  camera_ID_(camera_ID),
  camera_(0),

  zfocus_finder_(0),

  marker_finder_(0),
  marker_finder_thread_(0),

  conradModel_(0),
  conradManager_(0),

  module_assembler_(0),

  image_ctr_(0),

  testTimerCount_(0.),
  liveTimer_(0)
{
    ApplicationConfig* config = ApplicationConfig::instance();
    if(!config)
    {
      NQLog("AssemblyMainWindow", NQLog::Fatal) << "ApplicationConfig::instance() not initialized, null pointer";
      exit(1);
    }

    // motion
    motion_model_   = new LStepExpressModel(config->getValue<std::string>("LStepExpressDevice").c_str(), 1000, 1000);
    motion_manager_ = new LStepExpressMotionManager(motion_model_);

    motion_thread_  = new LStepExpressMotionThread(this);
    motion_manager_->myMoveToThread(motion_thread_);
    motion_thread_->start();

    // camera
    camera_model_ = new AssemblyUEyeModel_t(10);
    camera_model_->updateInformation();

    camera_thread_ = new AssemblyUEyeCameraThread(camera_model_, this);
    camera_thread_->start();

    camera_ = camera_model_->getCameraByID(camera_ID_);
    if(!camera_)
    {
      NQLog("AssemblyMainWindow", NQLog::Fatal)
         << "null pointer to AssemblyVUEyeCamera object"
         << " (camera_ID="+std::to_string(camera_ID_)+")";

      exit(1);
    }

    // marker finder
    marker_finder_ = new MarkerFinderPatRec(Util::QtCacheDirectory()+"/MarkerFinderPatRec", "rotations");

    marker_finder_thread_ = new MarkerFinderPatRecThread(marker_finder_, this);
    marker_finder_thread_->start();

    // zfocus finder
    zfocus_finder_ = new ZFocusFinder(camera_, motion_model_);

    /* TAB WIDGET ---------------------------------------------- */
    tabWidget_ = new QTabWidget(this);
    tabWidget_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

//    finderView_ = new AssemblyUEyeSnapShooter(tabWidget_);
//    tabWidget_->addTab(finderView_, "finder");

//    edgeView_ = new AssemblyUEyeSnapShooter(tabWidget_);
//    tabWidget_->addTab(edgeView_, "edges");

//    rawView_ = new AssemblyUEyeSnapShooter(tabWidget_);
//    tabWidget_->addTab(rawView_, "raw");

    /* IMAGE-THRESHOLDING VIEW --------------------------------- */
    const QString tabname_ImageThresholding("Image Thresholding");

    thresholdTunerView_ = new AssemblyThresholdTuner(tabWidget_);
    tabWidget_->addTab(thresholdTunerView_, tabname_ImageThresholding);

    marker_finder_->set_threshold(thresholdTunerView_->get_threshold());

    connect(marker_finder_     , SIGNAL(threshold_request()) , thresholdTunerView_, SLOT(  read_threshold()));
    connect(thresholdTunerView_, SIGNAL(threshold_value(int)), marker_finder_     , SLOT(update_threshold(int)));
    connect(marker_finder_     , SIGNAL(threshold_updated()) , marker_finder_     , SLOT(update_binary_image()));

    NQLog("AssemblyMainWindow", NQLog::Message) << "added view " << tabname_ImageThresholding;
    /* --------------------------------------------------------- */

    /* AUTO-FOCUS VIEW ----------------------------------------- */
    const QString tabname_AutoFocus("Auto Focus");

    autoFocusView_ = new AssemblyAutoFocus(tabWidget_);
    tabWidget_->addTab(autoFocusView_, tabname_AutoFocus);

    connect(autoFocusView_, SIGNAL(scan_values(double, int)), zfocus_finder_, SLOT(update_focus_inputs(double, int)));

    connect(zfocus_finder_, SIGNAL(show_zscan(QString))  , autoFocusView_, SLOT(read_graph(QString)));
    connect(zfocus_finder_, SIGNAL(update_text(double))  , autoFocusView_, SLOT(updateText(double)));

    autoFocusView_->configure_scan();

    NQLog("AssemblyMainWindow", NQLog::Message) << "added view " << tabname_AutoFocus;
    /* --------------------------------------------------------- */

    /* AUTOMATED-ASSEMBLY VIEW ------------------------------------ */
    const QString tabname_AutoAssembly("Auto Assembly");

    assembleView_ = new AssemblyModuleAssembler(motion_manager_, marker_finder_, tabWidget_);
    tabWidget_->addTab(assembleView_, tabname_AutoAssembly);

    NQLog("AssemblyMainWindow", NQLog::Message) << "added view " << tabname_AutoAssembly;

    // VACUUM connections
    conradModel_   = new ConradModel(assembleView_);
    conradManager_ = new ConradManager(conradModel_);

    module_assembler_ = new AssemblyAssembler(motion_model_);

    connect(assembleView_->VacuumToggler(), SIGNAL(toggleVacuum(int))                  , conradManager_                , SLOT(toggleVacuum(int)));
    connect(conradManager_                , SIGNAL(updateVacuumChannelState(int, bool)), assembleView_->VacuumToggler(), SLOT(updateVacuumChannelState(int, bool)));

    connect(this                          , SIGNAL(updateVacuumChannelsStatus())       , conradManager_                , SLOT(updateVacuumChannelsStatus()));

    NQLog("AssemblyMainWindow", NQLog::Debug) << "emitting signal \"updateVacuumChannelsStatus\"";

    emit updateVacuumChannelsStatus();
    // ---

    /* --------------------------------------------------------- */

//    /* U-EYE VIEW ---------------------------------------------- */
//    const QString tabname_uEye("uEye");
//
//    camera_widget_ = new AssemblyUEyeWidget(camera_model_, this);
//    tabWidget_->addTab(camera_widget_, tabname_uEye);
//
//    NQLog("AssemblyMainWindow", NQLog::Message) << "added view " << tabname_uEye;
//    /* --------------------------------------------------------- */

    /* MOTION-SETTINGS VIEW ------------------------------------ */
    const QString tabname_MotionSettings("Motion Settings");

    motionSettings_ = new LStepExpressSettings(motion_model_, tabWidget_);

    motionSettingsWidget_ = new LStepExpressSettingsWidget(motionSettings_, tabWidget_);
    tabWidget_->addTab(motionSettingsWidget_, tabname_MotionSettings);

    NQLog("AssemblyMainWindow", NQLog::Message) << "added view " << tabname_MotionSettings;
    /* --------------------------------------------------------- */

    /* MOTION-MANAGER VIEW ------------------------------------- */
    const QString tabname_MotionManager("Motion Manager");

    motion_manager_view_ = new LStepExpressMotionView(motion_model_, motion_manager_, tabWidget_);
    tabWidget_->addTab(motion_manager_view_, tabname_MotionManager);

    NQLog("AssemblyMainWindow", NQLog::Message) << "added view " << tabname_MotionManager;
    /* --------------------------------------------------------- */

    /* Upper Toolbar ------------------------------------------- */
    toolBar_ = addToolBar("Tools");
    toolBar_ ->addAction("Camera ON" , this, SLOT( enable_images()));
    toolBar_ ->addAction("Camera OFF", this, SLOT(disable_images()));
    toolBar_ ->addAction("Snapshot"  , this, SLOT(    get_image ()));

    checkbox1 = new QCheckBox("Auto-Focusing", this);
    toolBar_->addWidget(checkbox1);

    checkbox2 = new QCheckBox("Precision", this);
    toolBar_->addWidget(checkbox2);

    checkbox3 = new QCheckBox("Assembly", this);
    toolBar_->addWidget(checkbox3);

    checkbox4 = new QCheckBox("Alignment", this);
    toolBar_->addWidget(checkbox4);

    connect(checkbox1, SIGNAL(stateChanged(int)), this, SLOT(changeState_AutoFocus          (int)));
    connect(checkbox2, SIGNAL(stateChanged(int)), this, SLOT(changeState_PrecisionEstimation(int)));
    connect(checkbox3, SIGNAL(stateChanged(int)), this, SLOT(changeState_SandwichAssembly   (int)));
    connect(checkbox4, SIGNAL(stateChanged(int)), this, SLOT(changeState_Alignment          (int)));

    this->setCentralWidget(tabWidget_);

    this->updateGeometry();
    /* --------------------------------------------------------- */

    liveTimer_ = new QTimer(this);

    connect(liveTimer_, SIGNAL(timeout()), this, SLOT(liveUpdate()));

    connect(QApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(quit()));

    NQLog("AssemblyMainWindow", NQLog::Message) << "///////////////////////////////////////////////////////";
    NQLog("AssemblyMainWindow", NQLog::Message) << "//                                                   //";
    NQLog("AssemblyMainWindow", NQLog::Message) << "//                     DESY-CMS                      //";
    NQLog("AssemblyMainWindow", NQLog::Message) << "//                                                   //";
    NQLog("AssemblyMainWindow", NQLog::Message) << "//             Automated Module Assembly             //";
    NQLog("AssemblyMainWindow", NQLog::Message) << "//                                                   //";
    NQLog("AssemblyMainWindow", NQLog::Message) << "//  - AssemblyMainWindow initialized successfully -  //";
    NQLog("AssemblyMainWindow", NQLog::Message) << "//                                                   //";
    NQLog("AssemblyMainWindow", NQLog::Message) << "///////////////////////////////////////////////////////";
}

void AssemblyMainWindow::liveUpdate()
{
    NQLog("AssemblyMainWindow", NQLog::Debug) << "liveUpdate: emitting signal \"image\"";

    emit image();
}

void AssemblyMainWindow::enable_images()
{
    if(!image_ctr_)
    {
      image_ctr_ = new ImageController(camera_, zfocus_finder_);

      connect(this    , SIGNAL(images_ON())      , image_ctr_, SLOT(enable()));
      connect(this    , SIGNAL(images_OFF())     , image_ctr_, SLOT(disable()));
    }

    connect(image_ctr_, SIGNAL(camera_enabled()) , this      , SLOT(connect_images()));
    connect(image_ctr_, SIGNAL(camera_disabled()), this      , SLOT(disconnect_images()));

    connect(this      , SIGNAL(image())          , image_ctr_, SLOT(acquire_image()));
    connect(this      , SIGNAL(AutoFocus_ON ())  , image_ctr_, SLOT( enable_AutoFocus()));
    connect(this      , SIGNAL(AutoFocus_OFF())  , image_ctr_, SLOT(disable_AutoFocus()));

    NQLog("AssemblyMainWindow", NQLog::Message) << "enable_images"
       << ": ImageController connected";

    NQLog("AssemblyMainWindow", NQLog::Debug) << "enable_images"
       << ": emitting image \"images_ON\"";

    emit images_ON();
}

void AssemblyMainWindow::disable_images()
{
    if(image_ctr_)
    {
      disconnect(image_ctr_, SIGNAL(camera_enabled()) , this      , SLOT(connect_images()));
      disconnect(image_ctr_, SIGNAL(camera_disabled()), this      , SLOT(disconnect_images()));

      disconnect(this      , SIGNAL(image())          , image_ctr_, SLOT(acquire_image()));
      disconnect(this      , SIGNAL(AutoFocus_ON())   , image_ctr_, SLOT( enable_AutoFocus()));
      disconnect(this      , SIGNAL(AutoFocus_OFF())  , image_ctr_, SLOT(disable_AutoFocus()));

      NQLog("AssemblyMainWindow", NQLog::Message) << "disable_images"
         << ": ImageController disconnected";
    }

    NQLog("AssemblyMainWindow", NQLog::Debug) << "enable_images"
       << ": emitting image \"images_OFF\"";

    emit images_OFF();
}

void AssemblyMainWindow::changeState_AutoFocus(int state)
{
    if(!image_ctr_)
    {
      NQLog("AssemblyMainWindow", NQLog::Warning) << "changeState_AutoFocus"
         << ": ImageController not initialized, no action taken (hint: click \"Camera ON\")";

      return;
    }

    if(!motion_model_)
    {
      NQLog("AssemblyMainWindow", NQLog::Warning) << "changeState_AutoFocus"
         << ": LStepExpressModel not initialized, no action taken (hint: plug-in motion stage cable)";

      return;
    }

    if(state == 2)
    {
      NQLog("AssemblyMainWindow", NQLog::Message) << "changeState_AutoFocus"
         << ": emitting signal \"AutoFocus_ON\"";

      emit AutoFocus_ON();
    }
    else if(state == 0)
    {
      NQLog("AssemblyMainWindow", NQLog::Message) << "changeState_AutoFocus"
         << ": emitting signal \"AutoFocus_OFF\"";

      emit AutoFocus_OFF();
    }

    return;
}

void AssemblyMainWindow::changeState_PrecisionEstimation(int /* state */)
{
/*
    if(state == 2){

      NQLog("AssemblyMainWindow::changeState_PrecisionEstimation") << "precision estimation ON";

      connect(assembleView_, SIGNAL(launchPrecisionEstimation(double, double, double, double, double, double, int)), cmdr_zscan,
                             SLOT  (run_precisionestimation  (double, double, double, double, double, double, int)));

      connect   (cmdr_zscan        , SIGNAL(moveAbsolute(double, double, double, double)), motion_manager_, SLOT(moveAbsolute(double, double,double, double)));
      connect   (motion_model_     , SIGNAL(motionFinished())                            , cmdr_zscan    , SLOT(process_step()));
      connect   (cmdr_zscan        , SIGNAL(toggleVacuum(int))                           , conradManager_, SLOT(toggleVacuum(int)));
      connect   (conradManager_    , SIGNAL(updateVacuumChannelState(int, bool))         , cmdr_zscan    , SIGNAL(nextStep()));

//      connect(cmdr_zscan, SIGNAL(toggleVacuum()), conradManager_, SLOT(changeVacuum()));
//      for testing with random numbers
//        connect(cmdr_zscan, SIGNAL(makeDummies(int, double,double,double)), cmdr_zscan, SLOT(fill_positionvectors(int, double,double,double)));

//      for real lab tests with camera
      connect   (cmdr_zscan, SIGNAL(acquireImage())          , camera_      , SLOT(acquireImage()));
      connect   (cmdr_zscan, SIGNAL(changeVacuumState())     , cmdr_zscan   , SLOT(changeVacuumState()));
      connect   (cmdr_zscan, SIGNAL(showHistos(int, QString)), assembleView_, SLOT(updateImage(int, QString)));

      connect   (camera_   , SIGNAL(imageAcquired(cv::Mat))                           , finder_   , SLOT(runObjectDetection_labmode(cv::Mat)));
      connect   (finder_   , SIGNAL(reportObjectLocation(int, double, double, double)), cmdr_zscan, SLOT(fill_positionvectors(int, double, double, double)));
      connect   (cmdr_zscan, SIGNAL(nextStep())                                       , cmdr_zscan, SLOT(process_step()));

      NQLog("AssemblyMainWindow::changeState_PrecisionEstimation") << "signal-slot connections enabled";
    }
    else if(state == 0 ){

      NQLog("AssemblyMainWindow::changeState_PrecisionEstimation") << "precision estimation OFF";

      disconnect(cmdr_zscan        , SIGNAL(moveAbsolute(double, double,double, double)), motion_manager_, SLOT(moveAbsolute(double, double,double, double)));
      disconnect(motion_model_     , SIGNAL(motionFinished())                           , camera_       , SLOT(acquireImage()));
      disconnect(cmdr_zscan        , SIGNAL(toggleVacuum(int))                          , conradManager_, SLOT(toggleVacuum(int)));
      disconnect(conradManager_    , SIGNAL(updateVacuumChannelState(int, bool))        , cmdr_zscan    , SIGNAL(nextStep()));
      disconnect(camera_           , SIGNAL(imageAcquired(cv::Mat))                     , marker_finder_, SLOT(findMarker_templateMatching(int, cv::Mat)));
      disconnect(marker_finder_    , SIGNAL(getImageBlur(cv::Mat, cv::Rect))            , cmdr_zscan    , SLOT(write_image(cv::Mat, cv::Rect)) );
      disconnect(cmdr_zscan        , SIGNAL(read_graph(vector<double>,vector<double>))  , autoFocusView_, SLOT(read_graph(vector<double>,vector<double>)));
      disconnect(cmdr_zscan        , SIGNAL(updateText(double))                         , autoFocusView_, SLOT(updateText(double)));

      NQLog("AssemblyMainWindow::changeState_PrecisionEstimation") << "signal-slot connections disabled";
    }
*/
    return;
}

void AssemblyMainWindow::changeState_SandwichAssembly(int /* state */)
{
/*
    if(state == 2){

      NQLog("AssemblyMainWindow::changeState_SandwichAssembly") << ": state  " << state;

      connect(assembleView_, SIGNAL(launchSandwitchAssembly(double, double, double, double, double, double, double, double, double)), module_assembler_,
                             SLOT  (run_sandwitchassembly  (double, double, double, double, double, double, double, double, double)));

      connect(module_assembler_ , SIGNAL(moveAbsolute(double, double, double, double)), motion_manager_   , SLOT(moveAbsolute(double, double,double, double)));
      connect(motion_model_, SIGNAL(motionFinished())                            , module_assembler_, SLOT(process_step()));
      connect(module_assembler_ , SIGNAL(toggleVacuum(int))                           , conradManager_   , SLOT(toggleVacuum(int)));
      connect(conradManager_    , SIGNAL(updateVacuumChannelState(int, bool))         , module_assembler_, SIGNAL(nextStep()));

      //for testing with random numbers
      // connect(cmdr_zscan, SIGNAL(makeDummies(int, double,double,double)), cmdr_zscan, SLOT(fill_positionvectors(int, double,double,double)));

      // for real lab tests with camera
      connect(module_assembler_, SIGNAL(acquireImage())          , camera_      , SLOT(acquireImage()));
      connect(module_assembler_, SIGNAL(showHistos(int, QString)), assembleView_, SLOT(updateImage(int, QString)));
      connect(camera_          , SIGNAL(imageAcquired(cv::Mat))  , finder_      , SLOT(runObjectDetection_labmode(cv::Mat)) );

      connect(marker_finder_   , SIGNAL(reportObjectLocation(int,double,double,double)), module_assembler_, SLOT(centre_marker(int, double,double,double)));
      connect(module_assembler_, SIGNAL(nextStep())                                    , module_assembler_, SLOT(process_step()));
    }
    else if(state == 0){

      NQLog("AssemblyMainWindow::changeState_SandwichAssembly") << ": state  " << state;

      disconnect(assembleView_, SIGNAL(launchSandwitchAssembly(double, double, double, double, double, double, double, double, double)), module_assembler_,
                                SLOT  (run_sandwitchassembly  (double, double, double, double, double, double, double, double, double)));

      disconnect(module_assembler_ , SIGNAL(moveAbsolute(double, double, double, double)), motion_manager_   , SLOT(moveAbsolute(double, double,double, double)));
      disconnect(motion_model_, SIGNAL(motionFinished())                            , module_assembler_, SLOT(process_step()));
      disconnect(module_assembler_ , SIGNAL(toggleVacuum(int))                           , conradManager_   , SLOT(toggleVacuum(int)));
      disconnect(conradManager_    , SIGNAL(updateVacuumChannelState(int, bool))         , module_assembler_, SIGNAL(nextStep()));

      // for testing with random numbers
//      disconnect(cmdr_zscan, SIGNAL(makeDummies(int, double,double,double)), cmdr_zscan, SLOT(fill_positionvectors(int, double,double,double)));

      // for real lab tests with camera
      disconnect(module_assembler_, SIGNAL(acquireImage())                                , camera_          , SLOT(acquireImage()));
      disconnect(module_assembler_, SIGNAL(showHistos(int, QString))                      , assembleView_    , SLOT(updateImage(int, QString)));
      disconnect(camera_          , SIGNAL(imageAcquired(cv::Mat))                        , marker_finder_   , SLOT(runObjectDetection_labmode(cv::Mat)) );
      disconnect(marker_finder_   , SIGNAL(reportObjectLocation(int,double,double,double)), module_assembler_, SLOT(fill_positionvectors(int, double,double,double)));
      disconnect(module_assembler_, SIGNAL(nextStep())                                    , module_assembler_, SLOT(process_step()));
    }
*/
    return;
}

void AssemblyMainWindow::changeState_Alignment(int state)
{
    if(state == 2)
    {
      connect   (marker_finder_    , SIGNAL(image_updated())                                  , marker_finder_   , SLOT(update_binary_image()));
      connect   (marker_finder_    , SIGNAL(binary_image_updated())                           , marker_finder_   , SLOT(run_PatRec(1, 0)));

      connect   (assembleView_     , SIGNAL(launchAlignment     (int, double, double, double)), module_assembler_, SLOT(run_alignment(int, double, double, double)));
      connect   (module_assembler_ , SIGNAL(acquireImage())                                   , image_ctr_       , SLOT(acquire_image()));
      connect   (motion_model_     , SIGNAL(motionFinished())                                 , module_assembler_, SLOT(launch_next_alignment_step()));
      connect   (module_assembler_ , SIGNAL(moveRelative(double, double, double, double))     , motion_manager_  , SLOT(moveRelative(double, double, double, double)));
      connect   (marker_finder_    , SIGNAL(reportObjectLocation(int, double, double, double)), module_assembler_, SLOT(run_alignment(int, double, double, double)));
      connect   (module_assembler_ , SIGNAL(nextAlignmentStep   (int, double, double, double)), module_assembler_, SLOT(run_alignment(int, double, double, double)));

      NQLog("AssemblyMainWindow", NQLog::Message) << "changeState_Alignment: alignment enabled";
    }
    else if(state == 0)
    {
      disconnect(marker_finder_    , SIGNAL(image_updated())                                  , marker_finder_   , SLOT(update_binary_image()));
      disconnect(marker_finder_    , SIGNAL(binary_image_updated())                           , marker_finder_   , SLOT(run_PatRec(1, 0)));

      disconnect(assembleView_     , SIGNAL(launchAlignment     (int, double, double, double)), module_assembler_, SLOT(run_alignment(int, double, double, double)));
      disconnect(module_assembler_ , SIGNAL(acquireImage())                                   , image_ctr_       , SLOT(acquire_image()));
      disconnect(motion_model_     , SIGNAL(motionFinished())                                 , module_assembler_, SLOT(launch_next_alignment_step()));
      disconnect(module_assembler_ , SIGNAL(moveRelative(double, double, double, double))     , motion_manager_  , SLOT(moveRelative(double, double, double, double)));
      disconnect(marker_finder_    , SIGNAL(reportObjectLocation(int, double, double, double)), module_assembler_, SLOT(run_alignment(int, double, double, double)));
      disconnect(module_assembler_ , SIGNAL(nextAlignmentStep   (int, double, double, double)), module_assembler_, SLOT(run_alignment(int, double, double, double)));

      NQLog("AssemblyMainWindow", NQLog::Message) << "changeState_Alignment: alignment disabled";
    }

    return;
}

void AssemblyMainWindow::get_image()
{
    if(image_ctr_ == 0)
    {
      NQLog("AssemblyMainWindow", NQLog::Warning) << "get_image"
         << ": ImageController not initialized, no action taken (hint: click \"Camera ON\")";

      return;
    }

    if(image_ctr_->is_enabled() == false)
    {
      NQLog("AssemblyMainWindow", NQLog::Warning) << "get_image"
         << ": ImageController not enabled, no action taken (hint: click \"Camera ON\")";

      return;
    }

    NQLog("AssemblyMainWindow", NQLog::Debug) << "get_image"
       << ": emitting signal \"image\"";

    emit image();
}

void AssemblyMainWindow::testTimer()
{
    NQLog("AssemblyMainWindow", NQLog::Debug) << "testTimer"
       << ": timeOut=" << testTimerCount_;

    testTimerCount_ += 0.1;

    return;
}

void AssemblyMainWindow::connect_images()
{
//    finderView_        ->connectImageProducer(finder_, SIGNAL(markerFound  (const cv::Mat&)));
//    edgeView_          ->connectImageProducer(finder_, SIGNAL(edgesDetected(const cv::Mat&)));
//    rawView_           ->connectImageProducer(camera_, SIGNAL(imageAcquired(const cv::Mat&)));

//    const bool test = connect(camera_, SIGNAL(imageAcquired(cv::Mat)), finder_, SLOT(write_image(cv::Mat)));
//    connect(camera_, SIGNAL(imageAcquired(cv::Mat)), finder_, SLOT(runObjectDetection_labmode(cv::Mat)));
//    connect(camera_, SIGNAL(imageAcquired(cv::Mat)), finder_, SLOT(locatePickup(cv::Mat)));

    connect(marker_finder_, SIGNAL(image_request())        , image_ctr_    , SLOT(acquire_image()));
    connect(image_ctr_    , SIGNAL(image_acquired(cv::Mat)), marker_finder_, SLOT( update_image(cv::Mat)));

    thresholdTunerView_->connectImageProducer_1(marker_finder_, SIGNAL(       image_updated(cv::Mat)));
    thresholdTunerView_->connectImageProducer_2(marker_finder_, SIGNAL(binary_image_updated(cv::Mat)));

    autoFocusView_     ->connectImageProducer(zfocus_finder_, SIGNAL(image_acquired(cv::Mat)));

    NQLog("AssemblyMainWindow", NQLog::Message) << "connect_images"
       << ": enabled images in application view(s)";

//    liveTimer_->start(2000);
}

void AssemblyMainWindow::disconnect_images()
{
//    finderView_        ->disconnectImageProducer(finder_, SIGNAL(markerFound  (const cv::Mat&)));
//    edgeView_          ->disconnectImageProducer(finder_, SIGNAL(edgesDetected(const cv::Mat&)));
//    rawView_           ->disconnectImageProducer(camera_, SIGNAL(imagef       (const cv::Mat&)));

    disconnect(marker_finder_, SIGNAL(image_request())        , image_ctr_    , SLOT(acquire_image()));
    disconnect(image_ctr_    , SIGNAL(image_acquired(cv::Mat)), marker_finder_, SLOT(update_image(cv::Mat)));

    thresholdTunerView_->disconnectImageProducer_1(marker_finder_, SIGNAL(       image_updated(cv::Mat)));
    thresholdTunerView_->disconnectImageProducer_2(marker_finder_, SIGNAL(binary_image_updated(cv::Mat)));

    autoFocusView_     ->disconnectImageProducer(zfocus_finder_, SIGNAL(image_acquired(cv::Mat)));

    NQLog("AssemblyMainWindow", NQLog::Message) << "disconnect_images"
       << ": disabled images in application view(s)";

    liveTimer_->stop();
}

void AssemblyMainWindow::quit()
{
    if(camera_)
    {
      NQLog("AssemblyMainWindow", NQLog::Message) << "quit"
         << ": emitting signal \"images_OFF\"";

      emit images_OFF();

      camera_ = 0;
    }

    if(camera_thread_)
    {
      NQLog("AssemblyMainWindow", NQLog::Message) << "quit"
         << ": quitting camera thread";

      if(camera_thread_->wait(2000) == false){
         camera_thread_->terminate();
      }

      camera_thread_->quit();
    }

    return;
}
