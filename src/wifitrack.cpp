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

#include "wifitrack.hpp"
#include "application.hpp"

using namespace p44;



// MARK: ===== WTSSid

WTSSid::WTSSid() :
  seenLast(Never),
  seenCount(0)
{
}


// MARK: ===== WTMac

WTMac::WTMac() :
  seenLast(Never),
  seenCount(0)
{
}


// MARK: ===== WifiTrack

WifiTrack::WifiTrack(const string aMonitorIf) :
  inherited("wifitrack"),
  monitorIf(aMonitorIf),
  dumpPid(-1)
{
  // check for commandline-triggered standalone operation
  if (CmdLineApp::sharedCmdLineApp()->getOption("wifitrack")) {
    initOperation();
  }
}


WifiTrack::~WifiTrack()
{
}

// MARK: ==== API

ErrorPtr WifiTrack::initialize(JsonObjectPtr aInitData)
{
  initOperation();
  return Error::ok();
}


ErrorPtr WifiTrack::processRequest(ApiRequestPtr aRequest)
{
  ErrorPtr err;
  JsonObjectPtr data = aRequest->getRequest();
  JsonObjectPtr o = data->get("cmd");
  if (o) {
    string cmd = o->stringValue();
//    if (cmd=="hit") {
//      showHit();
//      return Error::ok();
//    }
//    else
    {
      return inherited::processRequest(aRequest);
    }
  }
  else {
    // decode properties
//    if (data->get("accelThreshold", o, true)) {
//      accelThreshold = o->int32Value();
//    }
//    if (data->get("interval", o, true)) {
//      interval = o->doubleValue()*MilliSecond;
//    }
    return err ? err : Error::ok();
  }
}


JsonObjectPtr WifiTrack::status()
{
  JsonObjectPtr answer = inherited::status();
  if (answer->isType(json_type_object)) {
//    answer->add("accelThreshold", JsonObject::newInt32(accelThreshold));
//    answer->add("interval", JsonObject::newDouble((double)interval/MilliSecond));
  }
  return answer;
}


// MARK: ==== hermel operation


void WifiTrack::initOperation()
{
  LOG(LOG_NOTICE, "initializing wifitrack");

#ifdef __APPLE__
  #warning "hardcoded access to mixloop hermel"
//  string cmd = "ssh -p 22 root@hermel-40a36bc18907.local. \"tcpdump -e -i moni0 -s 2000 type mgt subtype probe-req\"";
  string cmd = "ssh -p 22 root@1a8479bcaf76.cust.devices.plan44.ch \"tcpdump -e -i moni0 -s 2000 type mgt subtype probe-req\"";
#else
  string cmd = string_format("tcpdump -e -i %s -s 2000 type mgt subtype probe-req", monitorIf.c_str());
#endif
  int resultFd = -1;
  dumpPid = MainLoop::currentMainLoop().fork_and_system(boost::bind(&WifiTrack::dumpEnded, this, _1), cmd.c_str(), true, &resultFd);
  if (dumpPid>=0 && resultFd>=0) {
    dumpStream = FdCommPtr(new FdComm(MainLoop::currentMainLoop()));
    dumpStream->setFd(resultFd);
    dumpStream->setReceiveHandler(boost::bind(&WifiTrack::gotDumpLine, this, _1), '\n');
    // set up decoder
    // 18:45:37.098313 1.0 Mb/s 2412 MHz 11b -86dBm signal -86dBm signal antenna 0 -107dBm signal antenna 1 BSSID:Broadcast DA:Broadcast SA:54:60:09:c3:ed:42 (oui Unknown) Probe Request (Atelier Teilraum) [1.0 2.0 5.5 6.0 9.0 11.0 12.0 18.0 Mbit]
    ErrorPtr err = streamDecoder.compile(".* (-?\\d+)dBm signal antenna 0.*SA:([0123456789abcdef:]+).*Probe Request \\((.*)\\).*");
    if (!Error::ok(err)) {
      LOG(LOG_ERR, "Error in stream decoder Regexp: %s", err->description().c_str());
    }

  }
  // ready
  setInitialized();
}


void WifiTrack::dumpEnded(ErrorPtr aError)
{
  LOG(LOG_NOTICE, "tcpdump terminated with status: %s", Error::text(aError).c_str());
  restartTicket.executeOnce(boost::bind(&WifiTrack::initOperation, this), 5*Second);
}


void WifiTrack::gotDumpLine(ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    LOG(LOG_ERR, "error reading from tcp output stream: %s", Error::text(aError).c_str());
    return;
  }
  string line;
  if (dumpStream->receiveDelimitedString(line)) {
    LOG(LOG_DEBUG, "TCPDUMP: %s", line.c_str());
    if (streamDecoder.match(line, true)) {
      int rssi = 0;
      sscanf(streamDecoder.getCapture(1).c_str(), "%d", &rssi);
      uint64_t mac = stringToMacAddress(streamDecoder.getCapture(2).c_str());
      string ssid = streamDecoder.getCapture(3);
      LOG(LOG_INFO, "RSSI=%d, MAC=%s, SSID='%s'", rssi, macAddressToString(mac,':').c_str(), ssid.c_str());
      // record
      MLMicroSeconds now = MainLoop::now();
      // - SSID
      WTSSidPtr s;
      WTSSidMap::iterator ssidPos = ssids.find(ssid);
      if (ssidPos!=ssids.end()) {
        s = ssidPos->second;
      }
      else {
        // unknown, create
        s = WTSSidPtr(new WTSSid);
        s->ssid = ssid;
        ssids[ssid] = s;
      }
      s->seenLast = now;
      s->seenCount++;
      // - MAC
      WTMacPtr m;
      WTMacMap::iterator macPos = macs.find(mac);
      if (macPos!=macs.end()) {
        m = macPos->second;
      }
      else {
        // unknown, create
        m = WTMacPtr(new WTMac);
        m->mac = mac;
        macs[mac] = m;
      }
      m->seenLast = now;
      m->seenCount++;
      m->rssi = rssi;
      // - connection
      m->ssids[ssid] = s;
      s->macs[mac] = m;
      // process sighting
      processSighting(m, s);
    }
  }
}


void WifiTrack::processSighting(WTMacPtr aMac, WTSSidPtr aSSid)
{
  string s;
  const char* sep = "";
  for (WTSSidMap::iterator pos = aMac->ssids.begin(); pos!=aMac->ssids.end(); ++pos) {
    string sstr = pos->second->ssid;
    if (sstr.empty()) sstr = "<undefined>";
    string_format_append(s, "%s%s (%ld)", sep, sstr.c_str(), pos->second->seenCount);
    sep = ", ";
  }
  LOG(LOG_NOTICE, "MAC=%s (%ld), RSSI=%d : %s", macAddressToString(aMac->mac,':').c_str(), aMac->seenCount, aMac->rssi, s.c_str());
}





