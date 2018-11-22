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

#ifndef __pixelboardd_textview_hpp__
#define __pixelboardd_textview_hpp__

#include "p44utils_common.hpp"

#include "view.hpp"

namespace p44 {

  class TextView : public View
  {
    typedef View inherited;

    // text rendering
    string text; ///< internal representation of text
    int textSpacing; ///< pixels between characters
    string textPixelCols; ///< string of text column bytes

  public :

    TextView();

    virtual ~TextView();

    /// set new text
    /// @note: sets the content size of the view according to the text
    void setText(const string aText);

    /// get current text
    string getText() const { return text; }

    /// set character spacing
    void setTextSpacing(int aTextSpacing) { textSpacing = aTextSpacing; renderText(); }

    /// get text color
    int getTextSpacing() const { return textSpacing; }

    /// clear contents of this view
    virtual void clear() P44_OVERRIDE;

    #if ENABLE_VIEWCONFIG
    /// configure view from JSON
    virtual ErrorPtr configureView(JsonObjectPtr aViewConfig) P44_OVERRIDE;
    #endif

  protected:

    /// get content color at aPt
    virtual PixelColor contentColorAt(PixelCoord aPt) P44_OVERRIDE;

  private:

    void renderText();

  };
  typedef boost::intrusive_ptr<TextView> TextViewPtr;


} // namespace p44



#endif /* __pixelboardd_textview_hpp__ */
