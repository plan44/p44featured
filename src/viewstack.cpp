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

#include "viewstack.hpp"

#if ENABLE_VIEWCONFIG
  #include "viewfactory.hpp"
#endif


using namespace p44;


// MARK: ===== ViewStack

ViewStack::ViewStack()
{
}


ViewStack::~ViewStack()
{
}


void ViewStack::pushView(ViewPtr aView, WrapMode aPositioning, int aSpacing, int aNeededDx, int aNeededDy)
{
  // auto-positioning?
  if (aPositioning && !viewStack.empty()) {
    ViewPtr refView = viewStack.back();
    // X auto positioning
    if (aPositioning&wrapXmax) {
      aView->frame.x = refView->frame.x+refView->frame.dx+aSpacing;
    }
    else if (aPositioning&wrapXmin) {
      aView->frame.x = refView->frame.x-aView->frame.dx-aSpacing;
    }
    // Y auto positioning
    if (aPositioning&wrapYmax) {
      aView->frame.y = refView->frame.y+refView->frame.dy+aSpacing;
    }
    else if (aPositioning&wrapYmin) {
      aView->frame.y = refView->frame.y-aView->frame.dy-aSpacing;
    }
  }
  viewStack.push_back(aView);
  // recalculate my own content size
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
  content.dx = maxX-minX+aSpacing; if (content.dx<0) content.dx = 0;
  content.dy = maxY-minY+aSpacing; if (content.dy<0) content.dy = 0;
  makeDirty();
}


void ViewStack::popView()
{
  viewStack.pop_back();
  makeDirty();
}


void ViewStack::removeView(ViewPtr aView)
{
  for (ViewsList::iterator pos = viewStack.begin(); pos!=viewStack.end(); ++pos) {
    if ((*pos)==aView) {
      viewStack.erase(pos);
      break;
    }
  }
}


void ViewStack::clear()
{
  viewStack.clear();
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


PixelColor ViewStack::contentColorAt(int aX, int aY)
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
      lc = layer->colorAt(aX, aY);
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
    if (aViewConfig->get("layers", o)) {
      for (int i=0; i<o->arrayLength(); ++i) {
        JsonObjectPtr l = o->arrayGet(i);
        JsonObjectPtr o2;
        ViewPtr layerView;
        if (l->get("view", o2)) {
          err = p44::createViewFromConfig(o2, layerView);
          if (Error::isOK(err)) {
            WrapMode pos = noWrap;
            int spacing = 0;
            if (l->get("positioning", o2)) {
              pos = (WrapMode)o2->int32Value();
              if (l->get("spacing", o2)) {
                spacing = o2->int32Value();
              }
            }
            pushView(layerView, pos, spacing);
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

