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

#include "view.hpp"
#include "ledchaincomm.hpp" // for brightnessToPwm and pwmToBrightness

using namespace p44;

// MARK: ===== View

View::View() :
  geometryChanging(0),
  changedGeometry(false),
  sizeToContent(false)
{
  setFrame(zeroRect);
  // default to normal orientation
  contentOrientation = right;
  // default to no content wrap
  contentWrapMode = noWrap;
  // default content size is same as view's
  setContent(zeroRect);
  backgroundColor = { .r=0, .g=0, .b=0, .a=0 }; // transparent background,
  foregroundColor = { .r=255, .g=255, .b=255, .a=255 }; // fully white foreground...
  alpha = 255; // but content pixels passed trough 1:1
  contentIsMask = false; // content color will be used
  targetAlpha = -1; // not fading
  localTimingPriority = true;
  maskChildDirtyUntil = Never;
}


View::~View()
{
  clear();
}


bool View::isInContentSize(PixelCoord aPt)
{
  return aPt.x>=0 && aPt.y>=0 && aPt.x<content.dx && aPt.y<content.dy;
}


void View::geometryChange(bool aStart)
{
  if (aStart){
    if (geometryChanging<=0) {
      // start tracking changes
      changedGeometry = false;
      previousFrame = frame;
      previousContent = content;
    }
    geometryChanging++;
  }
  else {
    if (geometryChanging>0) {
      geometryChanging--;
      if (geometryChanging==0) {
        if (changedGeometry) {
          makeDirty();
          if (parentView) {
            parentView->childGeometryChanged(this, previousFrame, previousContent);
          }
        }
      }
    }
  }
}



void View::setFrame(PixelRect aFrame)
{
  geometryChange(true);
  changedGeometry = true;
  frame = aFrame;
  makeDirty();
  geometryChange(false);
}


void View::setParent(ViewPtr aParentView)
{
  parentView = aParentView;
}


void View::setContent(PixelRect aContent)
{
  geometryChange(true);
  changedGeometry = true;
  content = aContent;
  if (sizeToContent) sizeFrameToContent();
  makeDirty();
  geometryChange(false);
};


void View::setContentSize(PixelCoord aSize)
{
  geometryChange(true);
  changedGeometry = true;
  content.dx = aSize.x;
  content.dy = aSize.y;
  if (sizeToContent) sizeFrameToContent();
  makeDirty();
  geometryChange(false);
};


void View::setFullFrameContent()
{
  PixelCoord sz = getContentSize();
  setOrientation(View::right);
  orientPoint(sz);
  setContent({ 0, 0, frame.dx, frame.dy });
}


void View::sizeFrameToContent()
{
  geometryChange(true);
  changedGeometry = true;
  PixelCoord sz = getContentSize();
  orientPoint(sz);
  frame.dx = sz.x;
  frame.dy = sz.y;
  makeDirty();
  geometryChange(false);
}


void View::clear()
{
  setContentSize({0, 0});
}


bool View::reportDirtyChilds()
{
  if (maskChildDirtyUntil) {
    if (MainLoop::now()<maskChildDirtyUntil) {
      return false;
    }
    maskChildDirtyUntil = 0;
  }
  return true;
}


void View::updateNextCall(MLMicroSeconds &aNextCall, MLMicroSeconds aCallCandidate, MLMicroSeconds aCandidatePriorityUntil)
{
  if (localTimingPriority && aCandidatePriorityUntil>0 && aCallCandidate>=0 && aCallCandidate<aCandidatePriorityUntil) {
    // children must not cause "dirty" before candidate time is over
    MLMicroSeconds now = MainLoop::now();
    maskChildDirtyUntil = (aCallCandidate-now)*2+now; // duplicate to make sure candidate execution has some time to happen BEFORE dirty is unblocked
  }
  if (aNextCall<=0 || (aCallCandidate>0 && aCallCandidate<aNextCall)) {
    // candidate wins
    aNextCall = aCallCandidate;
  }
}


MLMicroSeconds View::step(MLMicroSeconds aPriorityUntil)
{
  // check fading
  if (targetAlpha>=0) {
    MLMicroSeconds now = MainLoop::now();
    double timeDone = (double)(now-startTime)/fadeTime;
    if (timeDone<1) {
      // continue fading
      int currentAlpha = targetAlpha-(1-timeDone)*fadeDist;
      setAlpha(currentAlpha);
      // return recommended call-again-time for smooth fading
      return now+fadeTime/fadeDist; // single alpha steps
    }
    else {
      // target alpha reached
      setAlpha(targetAlpha);
      targetAlpha = -1;
      // call back
      if (fadeCompleteCB) {
        SimpleCB cb = fadeCompleteCB;
        fadeCompleteCB = NULL;
        cb();
      }
    }
  }
  return Infinite; // completed
}


void View::setAlpha(int aAlpha)
{
  if (alpha!=aAlpha) {
    alpha = aAlpha;
    makeDirty();
  }
}


void View::stopFading()
{
  targetAlpha = -1;
  fadeCompleteCB = NULL; // did not run to end
}


void View::fadeTo(int aAlpha, MLMicroSeconds aWithIn, SimpleCB aCompletedCB)
{
  fadeDist = aAlpha-alpha;
  startTime = MainLoop::now();
  fadeTime = aWithIn;
  if (fadeTime<=0 || fadeDist==0) {
    // immediate
    setAlpha(aAlpha);
    targetAlpha = -1;
    if (aCompletedCB) aCompletedCB();
  }
  else {
    // start fading
    targetAlpha = aAlpha;
    fadeCompleteCB = aCompletedCB;
  }
}


#define SHOW_ORIGIN 0


void View::orientPoint(PixelCoord &aCoord)
{
  // translate between content and frame coordinates
  if (contentOrientation & xy_swap) {
    swap(aCoord.x, aCoord.y);
  }
  if (contentOrientation & x_flip) {
    aCoord.x = content.dx-aCoord.x-1;
  }
  if (contentOrientation & y_flip) {
    aCoord.y = content.dy-aCoord.y-1;
  }
}



PixelColor View::colorAt(PixelCoord aPt)
{
  // default is background color
  PixelColor pc = backgroundColor;
  if (alpha==0) {
    pc.a = 0; // entire view is invisible
  }
  else {
    // calculate coordinate relative to the frame's origin
    aPt.x -= frame.x;
    aPt.y -= frame.y;
    // translate into content coordinates
    orientPoint(aPt);
    // optionally clip content
    if (contentWrapMode&clipXY && (
      ((contentWrapMode&clipXmin) && aPt.x<0) ||
      ((contentWrapMode&clipXmax) && aPt.x>=content.dx) ||
      ((contentWrapMode&clipYmin) && aPt.y<0) ||
      ((contentWrapMode&clipYmax) && aPt.y>=content.dy)
    )) {
      // clip
      pc.a = 0; // invisible
    }
    else {
      // not clipped
      // optionally wrap content
      if (content.dx>0) {
        while ((contentWrapMode&wrapXmin) && aPt.x<0) aPt.x+=content.dx;
        while ((contentWrapMode&wrapXmax) && aPt.x>=content.dx) aPt.x-=content.dx;
      }
      if (content.dy>0) {
        while ((contentWrapMode&wrapYmin) && aPt.y<0) aPt.y+=content.dy;
        while ((contentWrapMode&wrapYmax) && aPt.y>=content.dy) aPt.y-=content.dy;
      }
      // now get content pixel (possibly shifted by content origin)
      pc = contentColorAt({aPt.x+content.x, aPt.y+content.y});
      if (contentIsMask) {
        // use only alpha of content, color comes from foregroundColor
        pc.r = foregroundColor.r;
        pc.g = foregroundColor.g;
        pc.b = foregroundColor.b;
      }
      #if SHOW_ORIGIN
      if (aPt.x==0 && aPt.y==0) {
        return { .r=255, .g=0, .b=0, .a=255 };
      }
      else if (aPt.x==1 && aPt.y==0) {
        return { .r=0, .g=255, .b=0, .a=255 };
      }
      else if (aPt.x==0 && aPt.y==1) {
        return { .r=0, .g=0, .b=255, .a=255 };
      }
      #endif
      if (pc.a==0) {
        // background is where content is fully transparent
        pc = backgroundColor;
        // Note: view background does NOT shine through semi-transparent content pixels!
        //   But non-transparent content pixels directly are view pixels!
      }
      // factor in layer alpha
      if (alpha!=255) {
        pc.a = dimVal(pc.a, alpha);
      }
    }
  }
  return pc;
}


// MARK: ===== Utilities

bool p44::rectContainsRect(const PixelRect &aParentRect, const PixelRect &aChildRect)
{
  return
    aChildRect.x>=aParentRect.x &&
    aChildRect.x+aChildRect.dx<=aParentRect.x+aParentRect.dx &&
    aChildRect.y>=aParentRect.y &&
    aChildRect.y+aChildRect.dy<=aParentRect.y+aParentRect.dy;
}

bool p44::rectIntersectsRect(const PixelRect &aRect1, const PixelRect &aRect2)
{
  return
    aRect1.x+aRect1.dx>aRect2.x &&
    aRect1.x<aRect2.x+aRect2.dx &&
    aRect1.y+aRect1.dy>aRect2.y &&
    aRect1.y<aRect2.y+aRect2.dy;
}



uint8_t p44::dimVal(uint8_t aVal, uint16_t aDim)
{
  uint32_t d = (aDim+1)*aVal;
  if (d>0xFFFF) return 0xFF;
  return d>>8;
}


void p44::dimPixel(PixelColor &aPix, uint16_t aDim)
{
  aPix.r = dimVal(aPix.r, aDim);
  aPix.g = dimVal(aPix.g, aDim);
  aPix.b = dimVal(aPix.b, aDim);
}


PixelColor p44::dimmedPixel(const PixelColor aPix, uint16_t aDim)
{
  PixelColor pix = aPix;
  dimPixel(pix, aDim);
  return pix;
}


void p44::alpahDimPixel(PixelColor &aPix)
{
  if (aPix.a!=255) {
    dimPixel(aPix, aPix.a);
  }
}


void p44::reduce(uint8_t &aByte, uint8_t aAmount, uint8_t aMin)
{
  int r = aByte-aAmount;
  if (r<aMin)
    aByte = aMin;
  else
    aByte = (uint8_t)r;
}


void p44::increase(uint8_t &aByte, uint8_t aAmount, uint8_t aMax)
{
  int r = aByte+aAmount;
  if (r>aMax)
    aByte = aMax;
  else
    aByte = (uint8_t)r;
}


void p44::overlayPixel(PixelColor &aPixel, PixelColor aOverlay)
{
  if (aOverlay.a==255) {
    aPixel = aOverlay;
  }
  else {
    // mix in
    // - reduce original by alpha of overlay
    aPixel = dimmedPixel(aPixel, 255-aOverlay.a);
    // - reduce overlay by its own alpha
    aOverlay = dimmedPixel(aOverlay, aOverlay.a);
    // - add in
    addToPixel(aPixel, aOverlay);
  }
  aPixel.a = 255; // result is never transparent
}


void p44::mixinPixel(PixelColor &aMainPixel, PixelColor aOutsidePixel, uint8_t aAmountOutside)
{
  if (aAmountOutside>0) {
    if (aMainPixel.a!=255 || aOutsidePixel.a!=255) {
      // mixed transparency
      uint8_t alpha = dimVal(aMainPixel.a, pwmToBrightness(255-aAmountOutside)) + dimVal(aOutsidePixel.a, pwmToBrightness(aAmountOutside));
      if (alpha>0) {
        // calculation only needed for non-transparent result
        // - alpha boost compensates for energy
        uint16_t ab = 65025/alpha;
        // Note: aAmountOutside is on the energy scale, not brightness, so need to add in PWM scale!
        uint16_t r_e = ( (((uint16_t)brightnessToPwm(dimVal(aMainPixel.r, aMainPixel.a))+1)*(255-aAmountOutside)) + (((uint16_t)brightnessToPwm(dimVal(aOutsidePixel.r, aOutsidePixel.a))+1)*(aAmountOutside)) )>>8;
        uint16_t g_e = ( (((uint16_t)brightnessToPwm(dimVal(aMainPixel.g, aMainPixel.a))+1)*(255-aAmountOutside)) + (((uint16_t)brightnessToPwm(dimVal(aOutsidePixel.g, aOutsidePixel.a))+1)*(aAmountOutside)) )>>8;
        uint16_t b_e = ( (((uint16_t)brightnessToPwm(dimVal(aMainPixel.b, aMainPixel.a))+1)*(255-aAmountOutside)) + (((uint16_t)brightnessToPwm(dimVal(aOutsidePixel.b, aOutsidePixel.a))+1)*(aAmountOutside)) )>>8;
        // - back to brightness, add alpha boost
        uint16_t r = (((uint16_t)pwmToBrightness(r_e)+1)*ab)>>8;
        uint16_t g = (((uint16_t)pwmToBrightness(g_e)+1)*ab)>>8;
        uint16_t b = (((uint16_t)pwmToBrightness(b_e)+1)*ab)>>8;
        // - check max brightness
        uint16_t m = r; if (g>m) m = g; if (b>m) m = b;
        if (m>255) {
          // more brightness requested than we have
          // - scale down to make max=255
          uint16_t cr = 65025/m;
          r = (r*cr)>>8;
          g = (g*cr)>>8;
          b = (b*cr)>>8;
          // - increase alpha by reduction of components
          alpha = (((uint16_t)alpha+1)*m)>>8;
          aMainPixel.r = r>255 ? 255 : r;
          aMainPixel.g = g>255 ? 255 : g;
          aMainPixel.b = b>255 ? 255 : b;
          aMainPixel.a = alpha>255 ? 255 : alpha;
        }
        else {
          // brightness below max, just convert back
          aMainPixel.r = r;
          aMainPixel.g = g;
          aMainPixel.b = b;
          aMainPixel.a = alpha;
        }
      }
      else {
        // resulting alpha is 0, fully transparent pixel
        aMainPixel = transparent;
      }
    }
    else {
      // no transparency on either side, simplified case
      uint16_t r_e = ( (((uint16_t)brightnessToPwm(aMainPixel.r)+1)*(255-aAmountOutside)) + (((uint16_t)brightnessToPwm(aOutsidePixel.r)+1)*(aAmountOutside)) )>>8;
      uint16_t g_e = ( (((uint16_t)brightnessToPwm(aMainPixel.g)+1)*(255-aAmountOutside)) + (((uint16_t)brightnessToPwm(aOutsidePixel.g)+1)*(aAmountOutside)) )>>8;
      uint16_t b_e = ( (((uint16_t)brightnessToPwm(aMainPixel.b)+1)*(255-aAmountOutside)) + (((uint16_t)brightnessToPwm(aOutsidePixel.b)+1)*(aAmountOutside)) )>>8;
      aMainPixel.r = r_e>255 ? 255 : pwmToBrightness(r_e);
      aMainPixel.g = g_e>255 ? 255 : pwmToBrightness(g_e);
      aMainPixel.b = b_e>255 ? 255 : pwmToBrightness(b_e);
      aMainPixel.a = 255;
    }
  }
}


void p44::addToPixel(PixelColor &aPixel, PixelColor aPixelToAdd)
{
  increase(aPixel.r, aPixelToAdd.r);
  increase(aPixel.g, aPixelToAdd.g);
  increase(aPixel.b, aPixelToAdd.b);
}


PixelColor p44::webColorToPixel(const string aWebColor)
{
  PixelColor res = transparent;
  size_t i = 0;
  size_t n = aWebColor.size();
  if (n>0 && aWebColor[0]=='#') { i++; n--; } // skip optional #
  uint32_t h;
  if (sscanf(aWebColor.c_str()+i, "%x", &h)==1) {
    if (n<=4) {
      // short form RGB or ARGB
      res.a = 255;
      if (n==4) { res.a = (h>>12)&0xF; res.a |= res.a<<4; }
      res.r = (h>>8)&0xF; res.r |= res.r<<4;
      res.g = (h>>4)&0xF; res.g |= res.g<<4;
      res.b = (h>>0)&0xF; res.b |= res.b<<4;
    }
    else {
      // long form RRGGBB or AARRGGBB
      res.a = 255;
      if (n==8) { res.a = (h>>24)&0xFF; }
      res.r = (h>>16)&0xFF;
      res.g = (h>>8)&0xFF;
      res.b = (h>>0)&0xFF;
    }
  }
  return res;
}


string p44::pixelToWebColor(const PixelColor aPixelColor)
{
  string w;
  if (aPixelColor.a!=255) string_format_append(w, "%02X", aPixelColor.a);
  string_format_append(w, "%02X%02X%02X", aPixelColor.r, aPixelColor.g, aPixelColor.b);
  return w;
}



PixelColor p44::hsbToPixel(int aHue, uint8_t aSaturation, uint8_t aBrightness)
{
  PixelColor p;
  Row3 RGB, HSV = { (double)aHue, (double)aSaturation/255, (double)aBrightness/255 };
  HSVtoRGB(HSV, RGB);
  p.r = RGB[0]*255;
  p.g = RGB[1]*255;
  p.b = RGB[2]*255;
  p.a = 255;
  return p;
}




#if ENABLE_VIEWCONFIG

// MARK: ===== view configuration

ErrorPtr View::configureView(JsonObjectPtr aViewConfig)
{
  JsonObjectPtr o;
  geometryChange(true);
  if (aViewConfig->get("label", o)) {
    label = o->stringValue();
  }
  if (aViewConfig->get("clear", o)) {
    if(o->boolValue()) clear();
  }
  if (aViewConfig->get("x", o)) {
    frame.x = o->int32Value(); makeDirty();
    changedGeometry = true;
  }
  if (aViewConfig->get("y", o)) {
    frame.y = o->int32Value(); makeDirty();
    changedGeometry = true;
  }
  if (aViewConfig->get("dx", o)) {
    frame.dx = o->int32Value(); makeDirty();
    changedGeometry = true;
  }
  if (aViewConfig->get("dy", o)) {
    frame.dy = o->int32Value(); makeDirty();
    changedGeometry = true;
  }
  if (aViewConfig->get("bgcolor", o)) {
    backgroundColor = webColorToPixel(o->stringValue()); makeDirty();
  }
  if (aViewConfig->get("color", o)) {
    foregroundColor = webColorToPixel(o->stringValue()); makeDirty();
  }
  if (aViewConfig->get("alpha", o)) {
    setAlpha(o->int32Value());
  }
  if (aViewConfig->get("wrapmode", o)) {
    setWrapMode(o->int32Value());
  }
  if (aViewConfig->get("mask", o)) {
    contentIsMask = o->boolValue();
  }
  if (aViewConfig->get("content_x", o)) {
    content.x = o->int32Value(); makeDirty();
    changedGeometry = true;
  }
  if (aViewConfig->get("content_y", o)) {
    content.y = o->int32Value(); makeDirty();
    changedGeometry = true;
  }
  if (aViewConfig->get("content_dx", o)) {
    content.dx = o->int32Value(); makeDirty();
    changedGeometry = true;
  }
  if (aViewConfig->get("content_dy", o)) {
    content.dy = o->int32Value(); makeDirty();
    changedGeometry = true;
  }
  if (aViewConfig->get("orientation", o)) {
    setOrientation(o->int32Value());
  }
  if (aViewConfig->get("fullframe", o)) {
    if(o->boolValue()) setFullFrameContent();
  }
  if (aViewConfig->get("sizetocontent", o)) {
    sizeToContent = o->boolValue();
  }
  if (aViewConfig->get("timingpriority", o)) {
    localTimingPriority = o->boolValue();
  }
  if (changedGeometry && sizeToContent) {
    sizeFrameToContent();
  }
  geometryChange(false);
  return ErrorPtr();
}


ViewPtr View::getView(const string aLabel)
{
  if (aLabel==label) {
    return ViewPtr(this); // that's me
  }
  return ViewPtr(); // not found
}



#endif // ENABLE_VIEWCONFIG



