//
//  Copyright (c) 2016-2017 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Ueli Wahlen <ueli@hotmail.com>
//
//  This file is part of lethd.
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

#include "neuron.hpp"

using namespace p44;

Neuron::Neuron(LEDChainCommPtr aLedChain, LethdApiPtr aLethdApi, NeuronMeasureCB aNeuronMesure)
{
  ledChain = aLedChain;
  lethdApi = aLethdApi;
  neuronMeasure = aNeuronMesure;
}

void Neuron::update()
{
  double value = neuronMeasure();
  avg = (avg * (MOVING_AVERAGE_COUNT - 1) + value) / MOVING_AVERAGE_COUNT;
}
