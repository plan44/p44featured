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

#ifndef __pixelboardd_viewfactory_hpp__
#define __pixelboardd_viewfactory_hpp__

#include "view.hpp"

#if ENABLE_VIEWCONFIG

#include "jsonobject.hpp"

namespace p44 {

  ErrorPtr createViewFromConfig(JsonObjectPtr aViewConfig, ViewPtr &aNewView, ViewPtr aParentView);

} // namespace p44


#endif // ENABLE_VIEWCONFIG

#endif /* __pixelboardd_viewfactory_hpp__ */
