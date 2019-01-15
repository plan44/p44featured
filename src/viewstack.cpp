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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 7


#include "viewstack.hpp"

#if ENABLE_VIEWCONFIG
  #include "viewfactory.hpp"
#endif


using namespace p44;


// MARK: ===== ViewStack

ViewStack::ViewStack()
{
  positioningMode = noWrap;
}


ViewStack::~ViewStack()
{
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    (*pos)->setParent(NULL);
  }
  viewStack.clear();
}


void ViewStack::pushView(ViewPtr aView, int aSpacing)
{
  geometryChange(true);
  // clip bits set means we don't want the content rect to get recalculated
  bool adjust = (positioningMode&clipXY)==0;
  // auto-positioning?
  if (positioningMode & wrapXY) {
    // wrap bits determine in which direction to position the view relative to those already present
    // - wrapXmin = to the left, wrapXmax = to the right etc.
    PixelRect r;
    getEnclosingContentRect(r);
    // X auto positioning
    if (positioningMode&wrapXmax) {
      aView->frame.x = r.x+r.dx+aSpacing;
    }
    else if (positioningMode&wrapXmin) {
      aView->frame.x = r.x-aView->frame.dx-aSpacing;
    }
    // Y auto positioning
    if (positioningMode&wrapYmax) {
      aView->frame.y = r.y+r.dy+aSpacing;
    }
    else if (positioningMode&wrapYmin) {
      aView->frame.y = r.y-aView->frame.dy-aSpacing;
    }
    if ((aView->getWrapMode()&clipXY)==0) {
      LOG(LOG_WARNING, "ViewStack '%s', pushed view '%s' is not clipped, probably will obscure neigbours!", label.c_str(), aView->label.c_str());
    }
    if (!sizeToContent) {
      LOG(LOG_WARNING, "ViewStack '%s' does not size to content, autopositioned view '%s' might be outside frame and thus invisible!", label.c_str(), aView->label.c_str());
    }
  }
  viewStack.push_back(aView);
  FOCUSLOG("+++ ViewStack '%s' pushes subview #%lu with frame=(%d,%d,%d,%d) - frame coords are relative to content origin",
    label.c_str(),
    viewStack.size(),
    aView->frame.x, aView->frame.y, aView->frame.dx, aView->frame.dy
  );
  if (adjust) {
    recalculateContentArea();
  }
  aView->setParent(this);
  makeDirty();
  geometryChange(false);
}


void ViewStack::purgeViews(int aKeepDx, int aKeepDy, bool aCompletely)
{
  if ((positioningMode&wrapXY)==0) return; // no positioning -> NOP
  // calculate content bounds where to keep views
  PixelRect r;
  getEnclosingContentRect(r);
  if (positioningMode&wrapXmax) {
    r.x = r.x+r.dx-aKeepDx;
    r.dx = aKeepDx;
  }
  else if (positioningMode&wrapXmin) {
    r.dx = aKeepDx;
  }
  if (positioningMode&wrapYmax) {
    r.y = r.y+r.dy-aKeepDy;
    r.dy = aKeepDy;
  }
  else if (positioningMode&wrapYmin) {
    r.dy = aKeepDy;
  }
  // now purge views
  FOCUSLOG("ViewStack '%s' purges with keep rectangle=(%d,%d,%d,%d)",
    label.c_str(),
    r.x, r.y, r.dx, r.dy
  );
  ViewsList::iterator pos = viewStack.begin();
  geometryChange(true);
  while (pos!=viewStack.end()) {
    ViewPtr v = *pos;
    if (
      !rectIntersectsRect(r, v->frame) ||
      (aCompletely && !rectContainsRect(r, v->frame))
    ) {
      if (viewStack.size()<=1) break; // do not purge last view in stack
      // remove this view
      FOCUSLOG("--- purges subview #%lu with frame=(%d,%d,%d,%d)",
        viewStack.size(),
        v->frame.x, v->frame.y, v->frame.dx, v->frame.dy
      );
      pos = viewStack.erase(pos);
      changedGeometry = true;
    }
    else {
      // test next
      ++pos;
    }
  }
  if (changedGeometry && (positioningMode&clipXY)==0) {
    recalculateContentArea();
  }
  geometryChange(false);
}


void ViewStack::recalculateContentArea()
{
  // where is the actual content relative to the content origin?
  PixelRect r;
  getEnclosingContentRect(r);
  // move actual content back to content origin
  offsetSubviews({-r.x, -r.y});
  // to avoid content moving seen from the outside, now modify content rectangle
  setContent(r);
}


void ViewStack::offsetSubviews(PixelCoord aOffset)
{
  geometryChange(true);
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    ViewPtr v = *pos;
    PixelRect f = v->frame;
    f.x += aOffset.x;
    f.y += aOffset.y;
    v->setFrame(f);
  }
  geometryChange(false);
}


void ViewStack::getEnclosingContentRect(PixelRect &aBounds)
{
  // get enclosing rect of all layers
  if (viewStack.empty()) {
    // no content, just a point at the origin
    aBounds = zeroRect;
    return;
  }
  int minX = INT_MAX;
  int maxX = INT_MIN;
  int minY = INT_MAX;
  int maxY = INT_MIN;
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    ViewPtr v = *pos;
    if (v->frame.x<minX) minX = v->frame.x;
    if (v->frame.y<minY) minY = v->frame.y;
    if (v->frame.x+v->frame.dx>maxX) maxX = v->frame.x+v->frame.dx;
    if (v->frame.y+v->frame.dy>maxY) maxY = v->frame.y+v->frame.dy;
  }
  aBounds.x = minX;
  aBounds.y = minY;
  aBounds.dx = maxX-minX; if (aBounds.dx<0) aBounds.dx = 0;
  aBounds.dy = maxY-minY; if (aBounds.dy<0) aBounds.dy = 0;
  FOCUSLOG("ViewStack '%s': enclosingContentRect=(%d,%d,%d,%d)", label.c_str(), aBounds.x, aBounds.y, aBounds.dx, aBounds.dy);
}


void ViewStack::popView()
{
  geometryChange(true);
  if (!viewStack.empty()) {
    viewStack.back()->setParent(NULL);
    viewStack.pop_back();
    changedGeometry = true;
  }
  geometryChange(false);
}


void ViewStack::removeView(ViewPtr aView)
{
  geometryChange(true);
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    if ((*pos)==aView) {
      (*pos)->setParent(NULL);
      viewStack.erase(pos);
      changedGeometry = true;
      break;
    }
  }
  geometryChange(false);
}


void ViewStack::clear()
{
  geometryChange(true);
  viewStack.clear();
  changedGeometry = true;
  inherited::clear();
}


MLMicroSeconds ViewStack::step(MLMicroSeconds aPriorityUntil)
{
  MLMicroSeconds nextCall = inherited::step(aPriorityUntil);
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    updateNextCall(nextCall, (*pos)->step(aPriorityUntil)); // no local view priorisation
  }
  return nextCall;
}


bool ViewStack::isDirty()
{
  if (inherited::isDirty()) return true; // dirty anyway
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    if ((*pos)->isDirty())
      return true; // subview is dirty -> stack is dirty
  }
  return false;
}


void ViewStack::updated()
{
  inherited::updated();
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    (*pos)->updated();
  }
}



void ViewStack::childGeometryChanged(ViewPtr aChildView, PixelRect aOldFrame, PixelRect aOldContent)
{
  if (geometryChanging==0) {
    // only if not already in process of changing
    //if ((positioningMode&clipXY)==0) {
    if (sizeToContent) {
      // current content bounds should not clip -> auto-adjust
      recalculateContentArea();
      moveFrameToContent(true);
    }
  }
}



PixelColor ViewStack::contentColorAt(PixelCoord aPt)
{
  // default is the viewstack's background color
  if (alpha==0) {
    return transparent; // entire viewstack is invisible
  }
  else {
    // consult views in stack
    PixelColor pc = black;
    PixelColor lc;
    uint8_t seethrough = 255; // first layer is directly visible, not yet obscured
    for (ViewsList::reverse_iterator pos = viewStack.rbegin(); pos!=viewStack.rend(); ++pos) {
      ViewPtr layer = *pos;
      if (layer->alpha==0) continue; // shortcut: skip fully transparent layers
      lc = layer->colorAt(aPt);
      if (lc.a==0) continue; // skip layer with fully transparent pixel
      // not-fully-transparent pixel
      // - scale down to current budget left
      lc.a = dimVal(lc.a, seethrough);
      lc = dimmedPixel(lc, lc.a);
      addToPixel(pc, lc);
      seethrough -= lc.a;
      if (seethrough<=0) break; // nothing more to see though
    } // collect from all layers
    if (seethrough>0) {
      // rest is background
      lc.a = dimVal(backgroundColor.a, seethrough);
      lc = dimmedPixel(backgroundColor, lc.a);
      addToPixel(pc, lc);
    }
    // factor in alpha of entire viewstack
    if (alpha!=255) {
      pc.a = dimVal(pc.a, alpha);
    }
    return pc;
  }
}


#if ENABLE_VIEWCONFIG

// MARK: ===== view configuration

ErrorPtr ViewStack::configureView(JsonObjectPtr aViewConfig)
{
  ErrorPtr err = inherited::configureView(aViewConfig);
  if (Error::isOK(err)) {
    JsonObjectPtr o;
    if (aViewConfig->get("positioningmode", o)) {
      setPositioningMode((WrapMode)o->int32Value());
    }
    if (aViewConfig->get("layers", o)) {
      for (int i=0; i<o->arrayLength(); ++i) {
        JsonObjectPtr l = o->arrayGet(i);
        JsonObjectPtr o2;
        ViewPtr layerView;
        if (l->get("view", o2)) {
          err = p44::createViewFromConfig(o2, layerView, this);
          if (Error::isOK(err)) {
            int spacing = 0;
            if (l->get("positioning", o2)) {
              LOG(LOG_WARNING, "Warning: legacy 'positioning' in stack subview -> use stack-global 'positioningmode' instead");
              setPositioningMode((WrapMode)o2->int32Value());
            }
            if (l->get("spacing", o2)) {
              spacing = o2->int32Value();
            }
            pushView(layerView, spacing);
          }
        }
      }
    }
  }
  return err;
}


ViewPtr ViewStack::getView(const string aLabel)
{
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    if (*pos) {
      ViewPtr view = (*pos)->getView(aLabel);
      if (view) return view;
    }
  }
  return inherited::getView(aLabel);
}



#endif // ENABLE_VIEWCONFIG

