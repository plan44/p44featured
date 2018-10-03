//
//  Copyright (c) 2018 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  This file is part of lethd/hermeld
//
//  lethd/hermeld is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  lethd/hermeld is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with lethd/hermeld. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __lethd_hermel_hpp__
#define __lethd_hermel_hpp__

#include "analogio.hpp"

#include "feature.hpp"
#include <math.h>

namespace p44 {

  class HermelShoot : public Feature
  {
    typedef Feature inherited;

    AnalogIoPtr pwmLeft;
    AnalogIoPtr pwmRight;
    MLTicket pulseTicket;

  public:

    HermelShoot(AnalogIoPtr aPwmLeft, AnalogIoPtr aPwmRight);

    void shoot(double aAngle, double aIntensity, MLMicroSeconds aPulseLength);

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

  private:

    void initOperation();

    ErrorPtr shoot(ApiRequestPtr aRequest);
    void endPulse();

  };

} // namespace p44



#endif /* __lethd_hermel_hpp__ */
