//
//  Copyright (c) 2018 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __lethd_dispmatrix_hpp__
#define __lethd_dispmatrix_hpp__

#include "ledchaincomm.hpp"

#include "feature.hpp"
#include "viewscroller.hpp"
#include "textview.hpp"


namespace p44 {


  class DispPanel : public P44Obj
  {
    friend class DispMatrix;

    LEDChainCommPtr chain; ///< the led chain for this panel
    int offsetX; ///< X offset within entire display
    int cols; ///< total number of columns (including hidden LEDs)
    int rows; ///< number of rows
    int borderRight; ///< number of hidden LEDs at far end
    int borderLeft; ///< number of hidden LEDs at near (connector) end
    int orientation; ///< orientation of content

    ViewScrollerPtr dispView; ///< the scroll view showing part of the contents
    ViewPtr contents; ///< the view containing the contents

    MLMicroSeconds lastUpdate;

  public:

    DispPanel(const string aChainName, int aOffsetX, int aRows, int aCols, int aBorderLeft, int aBorderRight, int aOrientation);
    virtual ~DispPanel();

    MLMicroSeconds step();


  private:

    void setOffsetX(double aOffsetX);
    void setText(const string aText);
    ErrorPtr installScene(JsonObjectPtr aSceneConfig);
    ErrorPtr reconfigure(const string aViewLabel, JsonObjectPtr aConfig);
    MLMicroSeconds updateDisplay();

    void setBackgroundColor(const PixelColor aColor);
    void setTextColor(const PixelColor aColor);
    void setTextSpacing(int aSpacing);
  };
  typedef boost::intrusive_ptr<DispPanel> DispPanelPtr;



  class DispMatrix : public Feature
  {
    typedef Feature inherited;

    static const int numChains = 3;
    string chainNames[numChains];
    DispPanelPtr panels[numChains];
    int usedPanels;

    MLTicket stepTicket;

  public:

    DispMatrix(const string aChainName1, const string aChainName2, const string aChainName3);
    virtual ~DispMatrix();

    /// reset the feature to uninitialized/re-initializable state
    virtual void reset() override;

    /// initialize the feature
    /// @param aInitData the init data object specifying feature init details
    /// @return error if any, NULL if ok
    virtual ErrorPtr initialize(JsonObjectPtr aInitData) override;

    /// handle request
    /// @param aRequest the API request to process
    /// @return NULL to send nothing at return (but possibly later via aRequest->sendResponse),
    ///   Error::ok() to just send a empty response, or error to report back
    virtual ErrorPtr processRequest(ApiRequestPtr aRequest) override;

    /// @return status information object for initialized feature, bool false for uninitialized
    virtual JsonObjectPtr status() override;

    /// set a handler that is called when one of the panels runs out of display content
    void setNeedContentHandler(NeedContentCB aNeedContentCB);


  private:

    void step(MLTimer &aTimer);
    void initOperation();

  };
  typedef boost::intrusive_ptr<DispMatrix> DispMatrixPtr;



} // namespace p44

#endif /* __lethd_dispmatrix_hpp__ */
