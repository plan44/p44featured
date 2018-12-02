//
//  Copyright (c) 2016-2018 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of pixelboardd.
//
//  pixelboardd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  pixelboardd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with pixelboardd. If not, see <http://www.gnu.org/licenses/>.
//

#include "viewanimator.hpp"

#if ENABLE_VIEWCONFIG
  #include "viewfactory.hpp"
#endif

using namespace p44;


// MARK: ===== ViewAnimator

ViewAnimator::ViewAnimator() :
  repeating(false),
  currentStep(-1),
  animationState(as_begin)
{
}


ViewAnimator::~ViewAnimator()
{
  for (SequenceVector::iterator pos = sequence.begin(); pos!=sequence.end(); ++pos) {
    ViewPtr v = pos->view;
    if (v) v->setParent(NULL);
  }
  sequence.clear();
}


void ViewAnimator::clear()
{
  stopAnimation();
  sequence.clear();
  inherited::clear();
}


void ViewAnimator::pushStep(ViewPtr aView, MLMicroSeconds aShowTime, MLMicroSeconds aFadeInTime, MLMicroSeconds aFadeOutTime)
{
  AnimationStep s;
  s.view = aView;
  s.showTime = aShowTime;
  s.fadeInTime = aFadeInTime;
  s.fadeOutTime = aFadeOutTime;
  sequence.push_back(s);
  makeDirty();
}


MLMicroSeconds ViewAnimator::step(MLMicroSeconds aPriorityUntil)
{
  MLMicroSeconds nextCall = inherited::step(aPriorityUntil);
  updateNextCall(nextCall, stepAnimation(), aPriorityUntil); // animation has priority
  if (currentStep<sequence.size()) {
    updateNextCall(nextCall, sequence[currentStep].view->step(aPriorityUntil));
  }
  return nextCall;
}


void ViewAnimator::stopAnimation()
{
  if (currentView) currentView->stopFading();
  animationState = as_begin;
  currentStep = -1;
}


MLMicroSeconds ViewAnimator::stepAnimation()
{
  if (currentStep<sequence.size()) {
    MLMicroSeconds now = MainLoop::now();
    MLMicroSeconds sinceLast = now-lastStateChange;
    AnimationStep as = sequence[currentStep];
    switch (animationState) {
      case as_begin:
        // initiate animation
        // - set current view
        currentView = as.view;
        makeDirty();
        if (as.fadeInTime>0) {
          currentView->setAlpha(0);
          currentView->fadeTo(255, as.fadeInTime);
        }
        animationState = as_show;
        lastStateChange = now;
        // next change we must handle is end of show time
        return now+as.fadeInTime+as.showTime;
      case as_show:
        if (sinceLast>as.fadeInTime+as.showTime) {
          // check fadeout
          if (as.fadeOutTime>0) {
            as.view->fadeTo(255, as.fadeOutTime);
            animationState = as_fadeout;
          }
          else {
            goto ended;
          }
          lastStateChange = now;
          // next change we must handle is end of fade out time
          return now + as.fadeOutTime;
        }
        else {
          // still waiting for end of show time
          return lastStateChange+as.fadeInTime+as.showTime;
        }
      case as_fadeout:
        if (sinceLast<as.fadeOutTime) {
          // next change is end of fade out
          return lastStateChange+as.fadeOutTime;
        }
      ended:
        // end of this step
        animationState = as_begin; // begins from start
        currentStep++;
        if (currentStep>=sequence.size()) {
          // all steps done
          // - call back
          if (completedCB) {
            SimpleCB cb=completedCB;
            if (!repeating) completedCB = NULL;
            cb();
          }
          // - possibly restart
          if (repeating) {
            currentStep = 0;
          }
          else {
            stopAnimation();
            return Infinite; // no need to call again for this animation
          }
        }
        return now; // call again immediately to initiate next step
    }
  }
  return Infinite;
}


void ViewAnimator::startAnimation(bool aRepeat, SimpleCB aCompletedCB)
{
  repeating = aRepeat;
  completedCB = aCompletedCB;
  currentStep = 0;
  animationState = as_begin; // begins from start
  stepAnimation();
}



bool ViewAnimator::isDirty()
{
  if (inherited::isDirty()) return true; // dirty anyway
  return currentView && reportDirtyChilds() ? currentView->isDirty() : false;
}


void ViewAnimator::updated()
{
  inherited::updated();
  if (currentView) currentView->updated();
}


PixelColor ViewAnimator::contentColorAt(PixelCoord aPt)
{
  // default is the animator's background color
  if (alpha==0 || !currentView) {
    return transparent; // entire viewstack is invisible
  }
  else {
    // consult current step's view
    return currentView->colorAt(aPt);
  }
}


#if ENABLE_VIEWCONFIG

// MARK: ===== view configuration

ErrorPtr ViewAnimator::configureView(JsonObjectPtr aViewConfig)
{
  ErrorPtr err = inherited::configureView(aViewConfig);
  if (Error::isOK(err)) {
    JsonObjectPtr o;
    if (aViewConfig->get("steps", o)) {
      for (int i=0; i<o->arrayLength(); ++i) {
        JsonObjectPtr s = o->arrayGet(i);
        JsonObjectPtr o2;
        ViewPtr stepView;
        MLMicroSeconds showTime = 500*MilliSecond;
        MLMicroSeconds fadeInTime = 0;
        MLMicroSeconds fadeOutTime = 0;
        if (s->get("view", o2)) {
          err = p44::createViewFromConfig(o2, stepView, this);
          if (Error::isOK(err)) {
            if (s->get("showtime", o2)) {
              showTime = o2->int32Value()*MilliSecond;
            }
            if (s->get("fadeintime", o2)) {
              fadeInTime = o2->int32Value()*MilliSecond;
            }
            if (s->get("fadeouttime", o2)) {
              fadeOutTime = o2->int32Value()*MilliSecond;
            }
            pushStep(stepView, showTime, fadeInTime, fadeOutTime);
          }
        }
      }
    }
    bool doStart = false;
    bool doRepeat = false;
    if (aViewConfig->get("start", o)) {
      doStart = o->boolValue();
    }
    if (aViewConfig->get("repeat", o)) {
      doRepeat = o->boolValue();
    }
    if (doStart) startAnimation(doRepeat);
  }
  return err;
}


ViewPtr ViewAnimator::getView(const string aLabel)
{
  for (SequenceVector::iterator pos = sequence.begin(); pos!=sequence.end(); ++pos) {
    ViewPtr v = pos->view;
    if (v) {
      ViewPtr view = v->getView(aLabel);
      if (view) return view;
    }
  }
  return inherited::getView(aLabel);
}


#endif // ENABLE_VIEWCONFIG


