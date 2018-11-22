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

#ifndef __pixelboardd_view_hpp__
#define __pixelboardd_view_hpp__

#include "p44utils_common.hpp"

#ifndef ENABLE_VIEWCONFIG
  #define ENABLE_VIEWCONFIG 1
#endif

#if ENABLE_VIEWCONFIG
  #include "jsonobject.hpp"
#endif

namespace p44 {


  typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a; // alpha
  } PixelColor;

  const PixelColor transparent = { .r=0, .g=0, .b=0, .a=0 };
  const PixelColor black = { .r=0, .g=0, .b=0, .a=255 };


  typedef struct {
    int x,y,dx,dy;
  } PixelRect;


  /// Utilities
  /// @{

  /// dim down (or light up) value
  /// @param aVal 0..255 value to dim up or down
  /// @param aDim 0..255: dim, >255: light up (255=100%)
  /// @return dimmed value, limited to max==255
  uint8_t dimVal(uint8_t aVal, uint16_t aDim);

  /// dim  r,g,b values of a pixel (alpha unaffected)
  /// @param aPix the pixel
  /// @param aDim 0..255: dim, >255: light up (255=100%)
  void dimPixel(PixelColor &aPix, uint16_t aDim);

  /// return dimmed pixel (alpha same as input)
  /// @param aPix the pixel
  /// @param aDim 0..255: dim, >255: light up (255=100%)
  /// @return dimmed pixel
  PixelColor dimmedPixel(const PixelColor aPix, uint16_t aDim);

  /// dim pixel r,g,b down by its alpha value, but alpha itself is not changed!
  /// @param aPix the pixel
  void alpahDimPixel(PixelColor &aPix);

  /// reduce a value by given amount, but not below minimum
  /// @param aByte value to be reduced
  /// @param aAmount amount to reduce
  /// @param aMin minimum value (default=0)
  void reduce(uint8_t &aByte, uint8_t aAmount, uint8_t aMin = 0);

  /// increase a value by given amount, but not above maximum
  /// @param aByte value to be increased
  /// @param aAmount amount to increase
  /// @param aMax maximum value (default=255)
  void increase(uint8_t &aByte, uint8_t aAmount, uint8_t aMax = 255);

  /// add color of one pixel to another
  /// @note does not check for color component overflow/wraparound!
  /// @param aPixel the pixel to add to
  /// @param aPixelToAdd the pixel to add
  void addToPixel(PixelColor &aPixel, PixelColor aPixelToAdd);

  /// overlay a pixel on top of a pixel (based on alpha values)
  /// @param aPixel the original pixel to add an ovelay to
  /// @param aOverlay the pixel to be laid on top
  void overlayPixel(PixelColor &aPixel, PixelColor aOverlay);

  /// mix two pixels
  /// @param aMainPixel the original pixel which will be modified to contain the mix
  /// @param aOutsidePixel the pixel to mix in
  /// @param aAmountOutside 0..255 (= 0..100%) value to determine how much weight the outside pixel should get in the result
  void mixinPixel(PixelColor &aMainPixel, PixelColor aOutsidePixel, uint8_t aAmountOutside);

  /// convert Web color to pixel color
  /// @param aWebColor web style #ARGB or #AARRGGBB color, alpha (A, AA) is optional, "#" is also optional
  /// @return pixel color. If Alpha is not specified, it is set to fully opaque = 255.
  PixelColor webColorToPixel(const string aWebColor);

  /// convert pixel color to web color
  /// @param aPixelColor pixel color
  /// @return web color in RRGGBB style or AARRGGBB when alpha is not fully opaque (==255)
  string pixelToWebColor(const PixelColor aPixelColor);


  /// @}

  class View;
  typedef boost::intrusive_ptr<View> ViewPtr;

  class View : public P44Obj
  {
    friend class ViewStack;

    bool dirty;

    /// fading
    int targetAlpha; ///< alpha to reach at end of fading, -1 = not fading
    int fadeDist; ///< amount to fade
    MLMicroSeconds startTime; ///< time when fading has started
    MLMicroSeconds fadeTime; ///< how long fading takes
    SimpleCB fadeCompleteCB; ///< fade complete

  public:

    // Orientation
    enum {
      // basic transformation flags
      xy_swap = 0x01,
      x_flip = 0x02,
      y_flip = 0x04,
      // directions of X axis
      right = 0, /// untransformed X goes left to right, Y goes up
      down = xy_swap+x_flip, /// X goes down, Y goes right
      left = x_flip+y_flip, /// X goes left, Y goes down
      up = xy_swap+y_flip, /// X goes down, Y goes right
    };
    typedef uint8_t Orientation;

    enum {
      noWrap = 0, /// do not wrap
      wrapXmin = 0x01, /// wrap in X direction for X<0
      wrapXmax = 0x02, /// wrap in X direction for X>=content size X
      wrapX = wrapXmin|wrapXmax, /// wrap in X direction
      wrapYmin = 0x04, /// wrap in Y direction for Y<0
      wrapYmax = 0x08, /// wrap in Y direction for Y>=content size Y
      wrapY = wrapYmin|wrapYmax, /// wrap in Y direction
      wrapXY = wrapX|wrapY, /// wrap in both directions
      clipXmin = 0x10, /// clip content left of content area
      clipXmax = 0x20, /// clip content right of content area
      clipX = clipXmin|clipXmax, // clip content horizontally
      clipYmin = 0x10, /// clip content below content area
      clipYmax = 0x20, /// clip content above content area
      clipY = clipYmin|clipYmax, // clip content vertically
      clipXY = clipX|clipY, // clip content
    };
    typedef uint8_t WrapMode;


  protected:

    // outer frame
    PixelRect frame;

    // alpha (opacity)
    uint8_t alpha;

    PixelColor backgroundColor; ///< background color
    PixelColor foregroundColor; ///< foreground color

    // content
    PixelRect content; ///< content position and size relative to frame
    Orientation contentOrientation; ///< orientation of content in view (defines content->view coordinate transformation)
    WrapMode contentWrapMode; ///< content wrap mode
    bool contentIsMask; ///< if set, only alpha of content is used on foreground color
    bool localTimingPriority; ///< if set, this view's timing requirements should be treated with priority over child view's
    MLMicroSeconds maskChildDirtyUntil; ///< if>0, child's dirty must not be reported until this time is reached

    #if ENABLE_VIEWCONFIG
    string label; ///< label of the view for addressing it
    #endif


    /// get content pixel color
    /// @param aX content X coordinate
    /// @param aY content Y coordinate
    /// @note aX and aY are NOT guaranteed to be within actual content as defined by contentSizeX/Y
    ///   implementation must check this!
    virtual PixelColor contentColorAt(int aX, int aY) { return backgroundColor; }

    /// helper for implementations: check if aX/aY within set content size
    bool isInContentSize(int aX, int aY);

    /// set dirty - to be called by step() implementation when the view needs to be redisplayed
    void makeDirty() { dirty = true; };

    /// @return if true, dirty childs should be reported
    bool reportDirtyChilds();

    /// helper for determining time of next step call
    /// @param aNextCall time of next call needed known so far, will be updated by candidate if conditions match
    /// @param aCallCandidate time of next call to update aNextCall
    /// @param aCandidatePriorityUntil if set, this means the aCallCandidate must be prioritized when it is before the
    ///   specified time (and the view is enabled for prioritized timing).
    ///   Prioritizing means that reportDirtyChilds() returns false until aCallCandidate has passed, to
    ///   prevent triggering display updates before the prioritized time.
    void updateNextCall(MLMicroSeconds &aNextCall, MLMicroSeconds aCallCandidate, MLMicroSeconds aCandidatePriorityUntil = 0);

  public :

    /// create view
    View();

    virtual ~View();

    /// set the frame within the parent coordinate system
    /// @param aOriginX origin X on pixelboard
    /// @param aOriginY origin Y on pixelboard
    /// @param aSizeX the X width of the view
    /// @param aSizeY the Y width of the view
    virtual void setFrame(int aOriginX, int aOriginY, int aSizeX, int aSizeY);

    /// set the view's background color
    /// @param aBackgroundColor color of pixels not covered by content
    void setBackgroundColor(PixelColor aBackgroundColor) { backgroundColor = aBackgroundColor; makeDirty(); };

    /// @return current background color
    PixelColor getBackgroundColor() { return backgroundColor; }

    /// set foreground color
    void setForegroundColor(PixelColor aColor) { foregroundColor = aColor; makeDirty(); }

    /// get foreground color
    PixelColor getForegroundColor() const { return foregroundColor; }

    /// set content wrap mode
    /// @param aWrapMode the new wrap mode
    void setWrapMode(WrapMode aWrapMode) { contentWrapMode = aWrapMode; makeDirty(); }

    /// get current wrap mode
    /// @return current wrap mode
    WrapMode getWrapMode() { return contentWrapMode; }

    /// set view's alpha
    /// @param aAlpha 0=fully transparent, 255=fully opaque
    void setAlpha(int aAlpha);

    /// get current alpha
    /// @return current alpha value 0=fully transparent=invisible, 255=fully opaque
    uint8_t getAlpha() { return alpha; };

    /// hide (set alpha to 0)
    void hide() { setAlpha(0); };

    /// show (set alpha to max)
    void show() { setAlpha(255); };

    /// set visible (hide or show)
    /// @param aVisible true to show, false to hide
    void setVisible(bool aVisible) { if (aVisible) show(); else hide(); };

    /// fade alpha
    /// @param aAlpha 0=fully transparent, 255=fully opaque
    /// @param aWithIn time from now when specified aAlpha should be reached
    /// @param aCompletedCB is called when fade is complete
    void fadeTo(int aAlpha, MLMicroSeconds aWithIn, SimpleCB aCompletedCB = NULL);

    /// stop ongoing fading
    /// @note: completed callback will not be called
    void stopFading();

    /// @param aOrientation the orientation of the content
    void setOrientation(Orientation aOrientation) { contentOrientation = aOrientation; makeDirty(); }

    /// set content offset
    void setContentOffset(int aOffsetX, int aOffsetY) { content.x = aOffsetX; content.y = aOffsetY; makeDirty(); };

    /// set content size
    void setContentSize(int aSizeX, int aSizeY) { content.dx = aSizeX; content.dy = aSizeY; makeDirty(); };

    /// @return content size X
    int getContentSizeX() const { return content.dx; }

    /// @return content size Y
    int getContentSizeY() const { return content.dy; }


    /// set content size to full frame content with same origin and orientation
    void setFullFrameContent();

    /// set frame size to contain all content
    void sizeFrameToContent();

    /// get color at X,Y
    /// @param aX PlayField X coordinate
    /// @param aY PlayField Y coordinate
    PixelColor colorAt(int aX, int aY);

    /// clear contents of this view
    /// @note base class just resets content size to zero, subclasses might NOT want to do that
    ///   and thus choose NOT to call inherited.
    virtual void clear();

    /// calculate changes on the display, return time of next change
    /// @param aPriorityUntil for views with local priority flag set, priority is valid until this time is reached
    /// @return Infinite if there is no immediate need to call step again, otherwise mainloop time of when to call again latest
    /// @note this must be called as demanded by return value, and after making changes to the view
    virtual MLMicroSeconds step(MLMicroSeconds aPriorityUntil);

    /// return if anything changed on the display since last call
    virtual bool isDirty()  { return dirty; };

    /// call when display is updated
    virtual void updated() { dirty = false; };

    #if ENABLE_VIEWCONFIG

    /// configure view from JSON
    /// @param aViewConfig JSON for configuring view and subviews
    /// @return ok or error in case of real errors (image not found etc., but minor
    ///   issues like unknown properties usually don't cause error)
    virtual ErrorPtr configureView(JsonObjectPtr aViewConfig);

    /// get view by label
    /// @param aLabel label of view to find
    /// @return NULL if not found, labelled view otherwise (first one with that label found in case >1 have the same label)
    virtual ViewPtr getView(const string aLabel);

    #endif

  };

} // namespace p44



#endif /* __pixelboardd_view_hpp__ */
