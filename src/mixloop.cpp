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

#include "mixloop.hpp"
#include "application.hpp"

using namespace p44;

MixLoop::MixLoop(const string aLedChain1Name, const string aLedChain2Name) :
  ledChain1Name(aLedChain1Name),
  ledChain2Name(aLedChain2Name),
  inherited("mixloop"),
  // params
  accelThreshold(1),
  accelChangeCutoff(0),
  accelIntegrationGain(0.1),
  integralFadeOffset(1.5),
  integralFadeScaling(0.95),
  maxIntegral(300),
  numLeds(10),
  integralDispOffset(0),
  integralDispScaling(0.01),
  // state
  accelIntegral(0)
{
  // check for commandline-triggered standalone operation
  if (CmdLineApp::sharedCmdLineApp()->getOption("mixloop")) {
    initOperation();
  }
}

// MARK: ==== light API

ErrorPtr MixLoop::initialize(JsonObjectPtr aInitData)
{
  initOperation();
  return Error::ok();
}


ErrorPtr MixLoop::processRequest(ApiRequestPtr aRequest)
{
  ErrorPtr err;
  JsonObjectPtr data = aRequest->getRequest();
  JsonObjectPtr o = data->get("cmd");
  if (o) {
    string cmd = o->stringValue();
//  if (cmd=="trigger") {
//    return triggerEffect(aRequest);
//  }
    return inherited::processRequest(aRequest);
  }
  else {
    // decode properties
    if (data->get("accelThreshold", o, true)) {
      accelThreshold = o->int32Value();
    }
    if (data->get("accelChangeCutoff", o, true)) {
      accelChangeCutoff = o->doubleValue();
    }
    if (data->get("accelIntegrationGain", o, true)) {
      accelIntegrationGain = o->doubleValue();
    }
    if (data->get("integralFadeOffset", o, true)) {
      integralFadeOffset = o->doubleValue();
    }
    if (data->get("integralFadeScaling", o, true)) {
      integralFadeScaling = o->doubleValue();
    }
    if (data->get("maxIntegral", o, true)) {
      maxIntegral = o->doubleValue();
    }
    if (data->get("numLeds", o, true)) {
      numLeds = o->int32Value();
    }
    if (data->get("integralDispOffset", o, true)) {
      integralDispOffset = o->doubleValue();
    }
    if (data->get("integralDispScaling", o, true)) {
      integralDispScaling = o->doubleValue();
    }
    return err ? err : Error::ok();
  }
}


JsonObjectPtr MixLoop::status()
{
  JsonObjectPtr answer = inherited::status();
  if (answer->isType(json_type_object)) {
    answer->add("accelThreshold", JsonObject::newInt32(accelThreshold));
    answer->add("accelChangeCutoff", JsonObject::newDouble(accelChangeCutoff));
    answer->add("accelIntegrationGain", JsonObject::newDouble(accelIntegrationGain));
    answer->add("integralFadeOffset", JsonObject::newDouble(integralFadeOffset));
    answer->add("integralFadeScaling", JsonObject::newDouble(integralFadeScaling));
    answer->add("maxIntegral", JsonObject::newDouble(maxIntegral));
    answer->add("numLeds", JsonObject::newInt32(numLeds));
    answer->add("integralDispOffset", JsonObject::newDouble(integralDispOffset));
    answer->add("integralDispScaling", JsonObject::newDouble(integralDispScaling));
  }
  return answer;
}


//ErrorPtr MixLoop::triggerEffect(ApiRequestPtr aRequest)
//{
//  JsonObjectPtr data = aRequest->getRequest();
//  JsonObjectPtr o;
//  double angle = 0; // straight
//  if (data->get("angle", o, true)) angle = o->doubleValue();
//  double intensity = 1; // full power
//  if (data->get("intensity", o, true)) intensity = o->doubleValue();
//  MLMicroSeconds pulseLength = 500*MilliSecond;
//  if (data->get("pulse", o, true)) pulseLength = o->int64Value() * MilliSecond;
//  shoot(angle, intensity, pulseLength);
//  return Error::ok();
//}


// MARK: ==== hermel operation


void MixLoop::initOperation()
{
  LOG(LOG_NOTICE, "initializing mixloop");
  ledChain1 = LEDChainCommPtr(new LEDChainComm(LEDChainComm::ledtype_ws281x, ledChain1Name, 100));
  ledChain2 = LEDChainCommPtr(new LEDChainComm(LEDChainComm::ledtype_ws281x, ledChain2Name, 100));
  ledChain1->begin();
  ledChain1->show();
  ledChain2->begin();
  ledChain2->show();
  setInitialized();
  // ADXL345 accelerometer @ SPI bus 1.0 (/dev/spidev1.0 software SPI)
  accelerometer = SPIManager::sharedManager().getDevice(10, "generic-HP@00");
  measureTicket.executeOnce(boost::bind(&MixLoop::accelInit, this), 1*Second);
}



static bool adxl345_writebyte(SPIDevicePtr aSPiDev, uint8_t aReg, uint8_t aValue)
{
  // Bit7: 0=write, 1=read
  // Bit6: 0=singlebyte, 1=multibyte
  // Bit5..0: register address
  uint8_t msg[2];
  msg[0] = aReg & 0x3F; // single byte register write
  msg[1] = aValue;
  return aSPiDev->getBus().SPIRawWriteRead(aSPiDev.get(), 2, msg, 0, NULL);
}


static bool adxl345_readbyte(SPIDevicePtr aSPiDev, uint8_t aReg, uint8_t &aValue)
{
  // Bit7: 0=write, 1=read
  // Bit6: 0=singlebyte, 1=multibyte
  // Bit5..0: register address
  uint8_t wr;
  wr = (aReg & 0x3F) | 0x80; // single byte register read
  return aSPiDev->getBus().SPIRawWriteRead(aSPiDev.get(), 1, &wr, 1, &aValue);
}


static bool adxl345_readword(SPIDevicePtr aSPiDev, uint8_t aReg, uint16_t &aValue)
{
  // Bit7: 0=write, 1=read
  // Bit6: 0=singlebyte, 1=multibyte
  // Bit5..0: register address
  uint8_t wr;
  uint8_t res[2];
  wr = (aReg & 0x3F) | 0xC0; // multi byte register read
  bool ok = aSPiDev->getBus().SPIRawWriteRead(aSPiDev.get(), 1, &wr, 2, res);
  if (ok) {
    aValue = res[0] + (res[1]<<8);
  }
  return ok;
}


void MixLoop::accelInit()
{
  // null previous
  lastaccel[0] = 0;
  lastaccel[1] = 0;
  lastaccel[2] = 0;
  // - set power register
  adxl345_writebyte(accelerometer, 0x2D, 0x28);
  uint8_t b;
  if (adxl345_readbyte(accelerometer, 0x2D, b)) {
    if (b==0x28) {
      // 4-wire SPI, full resolution, justfy right (LSB mode), 2G range
      adxl345_writebyte(accelerometer, 0x31, 0x08);
      // set FIFO mode
      adxl345_writebyte(accelerometer, 0x38, 0x00); // FIFO bypassed
      // set data rate
      adxl345_writebyte(accelerometer, 0x2C, 0x09); // high power, 50 Hz data rate / 25Hz bandwidth
      // ready now, start sampling
      LOG(LOG_NOTICE, "accelerometer ready -> start sampling now");
      accelMeasure();
      return;
    }
  }
  // retry later again
  LOG(LOG_INFO, "waiting for accelerometer to get ready, reg 0x2D=0x%02x", b);
  measureTicket.executeOnce(boost::bind(&MixLoop::accelInit, this), 1*Second);
}



void MixLoop::accelMeasure()
{
  bool changed = false;
  double changeamount = 0;
  for (int ai=0; ai<3; ai++) {
    // 0x32, 0x34 and 0x36 are accel data registers, 1/256g = LSB
    uint16_t uw;
    adxl345_readword(accelerometer, 0x32+2*ai, uw);
    int16_t a = (int16_t)uw;
    if (abs(a-lastaccel[ai])>accelThreshold) {
      accel[ai] = a;
      changeamount += fabs(accel[ai]-lastaccel[ai]);
      lastaccel[ai] = a;
      changed = true;
    }
  }
  if (changed) {
    LOG(LOG_INFO, "X = %5hd, Y = %5hd, Z = %5hd, raw changeAmount = %.0f", accel[0], accel[1], accel[2], changeamount);
  }
  // process
  changeamount -= accelChangeCutoff;
  if (changeamount<0) changeamount = 0;
  changeamount *= accelIntegrationGain;
  // integrate
  accelIntegral += changeamount - integralFadeOffset;
  accelIntegral *= integralFadeScaling;
  if (accelIntegral<0) accelIntegral = 0;
  if (accelIntegral>maxIntegral) accelIntegral = maxIntegral;
  if (accelIntegral>0) {
    LOG(LOG_INFO, "     changeAmount = %.0f, integral = %.0f", changeamount, accelIntegral);
  }
  // show
  showAccel(accelIntegral*integralDispScaling+integralDispOffset);
  // retrigger
  measureTicket.executeOnce(boost::bind(&MixLoop::accelMeasure, this), 33*MilliSecond);
}


void MixLoop::showAccel(double aFraction)
{
  if (ledChain1) {
    int onLeds = aFraction*numLeds;
    LOG(LOG_DEBUG, "onLeds=%d", onLeds);
    for (int i=0; i<numLeds; i++) {
      if (i<onLeds) {
        ledChain1->setColor(i, 255, 255-(255*i/numLeds), 0);
      }
      else {
        ledChain1->setColor(i, 0, 0, 0);
      }
    }
    ledChain1->show();
  }
}
