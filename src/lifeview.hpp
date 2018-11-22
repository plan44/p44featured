//
//  Copyright (c) 2017-2018 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __pixelboardd_lifeview_hpp__
#define __pixelboardd_lifeview_hpp__

#include "view.hpp"


namespace p44 {

  class LifeView : public View
  {
    typedef View inherited;

    std::vector<uint32_t> cells; ///< internal representation

    int dynamics;
    int population;

    int staticcount;
    int maxStatic;
    int minPopulation;
    int minStatic;

    MLMicroSeconds generationInterval;
    MLMicroSeconds lastGeneration;

  public :

    LifeView();
    virtual ~LifeView();

    /// set generation interval
    /// @param aInterval time between generations, Never = stopped
    virtual void setGenerationInterval(MLMicroSeconds aInterval);

    /// clear and resize to new
    virtual void clear() P44_OVERRIDE;

    /// calculate changes on the display, return time of next change
    /// @param aPriorityUntil for views with local priority flag set, priority is valid until this time is reached
    /// @return Infinite if there is no immediate need to call step again, otherwise mainloop time of when to call again latest
    /// @note this must be called as demanded by return value, and after making changes to the view
    virtual MLMicroSeconds step(MLMicroSeconds aPriorityUntil) P44_OVERRIDE;

    #if ENABLE_VIEWCONFIG

    /// configure view from JSON
    /// @param aViewConfig JSON for configuring view and subviews
    /// @return ok or error in case of real errors (image not found etc., but minor
    ///   issues like unknown properties usually don't cause error)
    virtual ErrorPtr configureView(JsonObjectPtr aViewConfig);

    #endif

  protected:

    /// get content pixel color
    /// @param aPt content coordinate
    /// @note aPt is NOT guaranteed to be within actual content as defined by contentSize
    ///   implementation must check this!
    virtual PixelColor contentColorAt(PixelCoord aPt) P44_OVERRIDE;

    bool prepareCells();
    void nextGeneration();
    void timeNext();
    void revive();
    int cellindex(int aX, int aY, bool aWrap);
    void calculateGeneration();
    void createRandomCells(int aMinCells, int aMaxCells);
    void placePattern(uint16_t aPatternNo, bool aWrap=true, int aCenterX=-1, int aCenterY=-1, int aOrientation=-1);
  };
  typedef boost::intrusive_ptr<LifeView> LifeViewPtr;



} // namespace p44



#endif /* __pixelboardd_lifeview_hpp__ */
