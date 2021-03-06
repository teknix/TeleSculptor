/*ckwg +29
 * Copyright 2019 by Kitware, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name Kitware, Inc. nor the names of any contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "RunAllTool.h"
#include "GuiCommon.h"

#include <arrows/core/depth_utils.h>
#include <arrows/mvg/sfm_utils.h>
#include <arrows/mvg/transform.h>

#include "BundleAdjustTool.h"
#include "ComputeAllDepthTool.h"
#include "FuseDepthTool.h"
#include "InitCamerasLandmarksTool.h"
#include "TrackFeaturesTool.h"
#include "MainWindow.h"

#include <QMessageBox>
#include <qtStlUtil.h>

#include <algorithm>

//-----------------------------------------------------------------------------
class RunAllToolPrivate
{
public:
  RunAllToolPrivate()
    : output(0), failed(false)
  {
  }

  AbstractTool::Outputs output;
  bool failed;

  std::unique_ptr<TrackFeaturesTool> tracker;
  std::unique_ptr<InitCamerasLandmarksTool> initializer;
  std::unique_ptr<ComputeAllDepthTool> depther;
  std::unique_ptr<FuseDepthTool> fuser;
  QWidget* window;
  MainWindow* mainwindow = nullptr;
};

QTE_IMPLEMENT_D_FUNC(RunAllTool)

//-----------------------------------------------------------------------------
RunAllTool::RunAllTool(QObject* parent)
  : AbstractTool(parent), d_ptr(new RunAllToolPrivate)
{
  this->data()->logger =
    kwiver::vital::get_logger("telesculptor.tools.run_all");

  this->setText("&Run End-to-End");
  this->setToolTip("Runs the end-to-end reconstruction pipeline starting with "
                   "a video and ending with a surface mesh.");

  // Find the main window
  QObject* p = parent;
  d_ptr->mainwindow = qobject_cast<MainWindow*>(p);
  while (p && !d_ptr->mainwindow)
  {
    p = p->parent();
    d_ptr->mainwindow = qobject_cast<MainWindow*>(p);
  }
}

//-----------------------------------------------------------------------------
RunAllTool::~RunAllTool()
{
}

//-----------------------------------------------------------------------------
AbstractTool::Outputs RunAllTool::outputs() const
{
  QTE_D();
  return d->output;
}

//-----------------------------------------------------------------------------
bool RunAllTool::execute(QWidget* window)
{
  QTE_D();

  if (!this->hasVideoSource())
  {
    QMessageBox::information(
      window, "Insufficient data", "This operation requires a video source.");
    return false;
  }

  d->tracker = std::unique_ptr<TrackFeaturesTool>(new TrackFeaturesTool);
  d->initializer = std::unique_ptr<InitCamerasLandmarksTool>(new InitCamerasLandmarksTool);
  d->depther = std::unique_ptr<ComputeAllDepthTool>(new ComputeAllDepthTool);
  d->fuser = std::unique_ptr<FuseDepthTool>(new FuseDepthTool);

  d->window = window;
  d->failed = false;

  return AbstractTool::execute(window);
}

//-----------------------------------------------------------------------------
void RunAllTool::cancel()
{
  QTE_D();
  d->tracker->cancel();
  d->initializer->cancel();
  d->depther->cancel();
  d->fuser->cancel();
  AbstractTool::cancel();
}

//-----------------------------------------------------------------------------
void RunAllTool::forwardInterimResults(std::shared_ptr<ToolData> data)
{
  emit AbstractTool::updated(data);
}

//-----------------------------------------------------------------------------
void RunAllTool::reportToolError(QString const& msg)
{
  QTE_D();
  d->failed = true;
  emit AbstractTool::failed(msg);
}

//-----------------------------------------------------------------------------
bool RunAllTool::runTool(AbstractTool* tool, bool save_final)
{
  QTE_D();
  QObject::connect(tool, &AbstractTool::updated,
                   this, &RunAllTool::forwardInterimResults);
  QObject::connect(tool, &AbstractTool::failed,
                   this, &RunAllTool::reportToolError);
  // A direct connection to the main window with a blocking connection is
  // needed for "saved".  Using a slot on the RunAllTool to forward the message
  // as done for "updated" causes deadlock.
  QObject::connect(tool, &AbstractTool::saved,
                   d->mainwindow, &MainWindow::acceptToolSaveResults,
                   Qt::BlockingQueuedConnection);

  tool->setToolData(this->data());
  d->output = tool->outputs();
  tool->execute(d->window);
  tool->wait();
  if (d->failed || isCanceled())
    return false;
  if (save_final)
  {
    saveResults(tool);
  }
  tool->disconnect();

  return true;
}

//-----------------------------------------------------------------------------
void RunAllTool::saveResults(AbstractTool* tool)
{
  //Copy data to allow visualization of last finished tool
  auto savedata = std::make_shared<ToolData>();
  AbstractTool::Outputs out = tool->outputs();
  if (out.testFlag(AbstractTool::Cameras))
  {
    savedata->copyCameras(this->cameras());
  }
  if (out.testFlag(AbstractTool::Landmarks))
  {
    savedata->copyLandmarks(this->landmarks());
  }
  if (out.testFlag(AbstractTool::Tracks))
  {
    savedata->copyTracks(this->tracks());
  }
  if (out.testFlag(AbstractTool::TrackChanges))
  {
    savedata->copyTrackChanges(this->track_changes());
  }
  if (out.testFlag(AbstractTool::Depth))
  {
    savedata->copyDepth(this->depth());
  }
  if (out.testFlag(AbstractTool::ActiveFrame))
  {
    savedata->activeFrame = this->activeFrame();
  }
  if (out.testFlag(AbstractTool::Fusion))
  {
    savedata->copyFusion(this->volume());
  }
  emit saved(savedata);
}

//-----------------------------------------------------------------------------
void RunAllTool::run()
{
  QTE_D();
  if (!runTool(d->tracker.get()))
  {
    return;
  }

  if (!runTool(d->initializer.get()))
  {
    return;
  }

  // shift the coordinates
  auto constraints = this->sfmConstraints();
  auto lgcs = constraints->get_local_geo_cs();
  if (!lgcs.origin().is_empty())
  {
    auto landmarks = this->landmarks();
    auto cameras = this->cameras();
    kwiver::vital::vector_3d offset =
      kwiver::arrows::mvg::landmarks_ground_center(*landmarks);

    kwiver::vital::vector_3d new_origin = lgcs.origin().location() + offset;
    lgcs.set_origin(kwiver::vital::geo_point(new_origin, lgcs.origin().crs()));
    constraints->set_local_geo_cs(lgcs);
    this->setSfmConstraints(constraints);

    kwiver::arrows::mvg::translate_inplace(*landmarks, -offset);
    kwiver::arrows::mvg::translate_inplace(*cameras, -offset);
  }

  //Compute an ROI from landmarks
  auto const& lm = this->landmarks()->landmarks();
  std::vector<kwiver::vital::landmark_sptr> landmarks_out;
  foreach (auto const& l, lm)
  {
    landmarks_out.push_back(l.second);
  }
  double bounds[6];
  kwiver::arrows::core::compute_robust_ROI(landmarks_out, bounds);
  vtkNew<vtkBox> roi;
  roi->AddBounds(bounds);
  setROI(roi);

  if (!runTool(d->depther.get(), false))
  {
    return;
  }

  runTool(d->fuser.get(), false);
}
