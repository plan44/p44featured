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

#ifndef __pixelboardd_viewstack_hpp__
#define __pixelboardd_viewstack_hpp__

#include "view.hpp"

namespace p44 {

  class ViewStack : public View
  {
    typedef View inherited;

    typedef std::list<ViewPtr> ViewsList;

    ViewsList viewStack;
    WrapMode positioningMode; ///< mode for positioning views with pushView, purging with purgeView, and autoresizing on child changes

  public :

    /// create view stack
    ViewStack();

    virtual ~ViewStack();

    /// set the positioning, purging and autosizing mode
    /// @param aPositioningMode where to append or purge views. Using wrapMode constants as follows:
    /// - for pushView: wrapXmax means appending in positive X direction, wrapXmin means in negative X direction, etc.
    /// - for pushView, purgeView and change of child view sizes: any clip bits set means *NO* automatic change of content bounds
    /// - for purgeViews: wrapXmax means measuring new size from max X coordinate in negative X direction,
    ///   wrapXmin means from min X coordinate in positive X direction, etc.
    void setPositioningMode(WrapMode aPositioningMode) { positioningMode = aPositioningMode; }

    /// get the current positioning mode as set by setPositioningMode()
    WrapMode getPositioningMode() { return positioningMode; }

    /// push view onto top of stack
    /// @param aView the view to push in front of all other views
    /// @param aSpacing extra pixels between appended views
    void pushView(ViewPtr aView, int aSpacing = 0);

    /// purge views that are outside the specified content size in the specified direction
    /// @param aKeepDx keep views with frame completely or partially within this new size (measured according to positioning mode)
    /// @param aKeepDy keep views with frame completely or partially within this new size (measured according to positioning mode)
    /// @param aCompletely if set, keep only views which are completely within the new size
    void purgeViews(int aKeepDx, int aKeepDy, bool aCompletely);

    /// remove topmost view
    void popView();

    /// remove specific view
    /// @param aView the view to remove from the stack
    void removeView(ViewPtr aView);


    /// clear stack, means remove all views
    virtual void clear() P44_OVERRIDE;

    /// calculate changes on the display, return time of next change
    /// @param aPriorityUntil for views with local priority flag set, priority is valid until this time is reached
    /// @return Infinite if there is no immediate need to call step again, otherwise mainloop time of when to call again latest
    /// @note this must be called as demanded by return value, and after making changes to the view
    virtual MLMicroSeconds step(MLMicroSeconds aPriorityUntil) P44_OVERRIDE;

    /// return if anything changed on the display since last call
    virtual bool isDirty() P44_OVERRIDE;

    /// call when display is updated
    virtual void updated() P44_OVERRIDE;

    /// child view has changed geometry (frame, content rect)
    virtual void childGeometryChanged(ViewPtr aChildView, PixelRect aOldFrame, PixelRect aOldContent) P44_OVERRIDE;


    #if ENABLE_VIEWCONFIG

    /// configure view from JSON
    /// @param aViewConfig JSON for configuring view and subviews
    /// @return ok or error in case of real errors (image not found etc., but minor
    ///   issues like unknown properties usually don't cause error)
    virtual ErrorPtr configureView(JsonObjectPtr aViewConfig) P44_OVERRIDE;

    /// get view by label
    /// @param aLabel label of view to find
    /// @return NULL if not found, labelled view otherwise (first one with that label found in case >1 have the same label)
    virtual ViewPtr getView(const string aLabel) P44_OVERRIDE;

    #endif


  protected:

    /// get content pixel color
    /// @param aPt content coordinate
    /// @note aPt is NOT guaranteed to be within actual content as defined by contentSize
    ///   implementation must check this!
    virtual PixelColor contentColorAt(PixelCoord aPt) P44_OVERRIDE;

  private:

    void getEnclosingContentRect(PixelRect &aBounds);
    void recalculateContentSize();

  };
  typedef boost::intrusive_ptr<ViewStack> ViewStackPtr;

} // namespace p44



#endif /* __pixelboardd_viewstack_hpp__ */
