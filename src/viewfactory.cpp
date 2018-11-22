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

#include "viewfactory.hpp"

#if ENABLE_VIEWCONFIG

#include "imageview.hpp"
#include "textview.hpp"
#include "viewanimator.hpp"
#include "viewstack.hpp"
#include "viewscroller.hpp"
#include "lifeview.hpp"

using namespace p44;

// MARK: ===== View factory function

ErrorPtr p44::createViewFromConfig(JsonObjectPtr aViewConfig, ViewPtr &aNewView, ViewPtr aParentView)
{
  JsonObjectPtr o;
  if (aViewConfig->get("type", o)) {
    string vt = o->stringValue();
    if (vt=="text") {
      aNewView = ViewPtr(new TextView);
    }
    else if (vt=="image") {
      aNewView = ViewPtr(new ImageView);
    }
    else if (vt=="animator") {
      aNewView = ViewPtr(new ViewAnimator);
    }
    else if (vt=="stack") {
      aNewView = ViewPtr(new ViewStack);
    }
    else if (vt=="scroller") {
      aNewView = ViewPtr(new ViewScroller);
    }
    else if (vt=="life") {
      aNewView = ViewPtr(new LifeView);
    }
    else if (vt=="plain") {
      aNewView = ViewPtr(new View);
    }
    else {
      return TextError::err("unknown view type '%s'", vt.c_str());
    }
  }
  else {
    return TextError::err("missing 'type'");
  }
  aNewView->setParent(aParentView);
  // now let view configure itself
  return aNewView->configureView(aViewConfig);
}

#endif // ENABLE_VIEWCONFIG

