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
  seenCount(0),
  hidden(false)
{
}


// MARK: ===== WTMac

WTMac::WTMac() :
  seenLast(Never),
  seenFirst(Never),
  seenCount(0),
  lastRssi(-9999),
  bestRssi(-9999),
  worstRssi(9999),
  shownLast(Never),
  hidden(false)
{
}


// MARK: ===== WifiTrack

WifiTrack::WifiTrack(const string aMonitorIf) :
  inherited("wifitrack"),
  monitorIf(aMonitorIf),
  dumpPid(-1),
  rememberWithoutSsid(false),
  minShowInterval(3*Minute),
  minRssi(-70)
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
    if (cmd=="dump") {
      JsonObjectPtr ans = dataDump();
      aRequest->sendResponse(ans, ErrorPtr());
      return ErrorPtr();
    }
    else if (cmd=="save") {
      err = save();
      return err ? err : Error::ok();
    }
    else if (cmd=="load") {
      err = load();
      return err ? err : Error::ok();
    }
    else if (cmd=="hide") {
      if (data->get("ssid", o)) {
        string s = o->stringValue();
        WTSSidMap::iterator pos = ssids.find(s);
        if (pos!=ssids.end()) {
          pos->second->hidden = true;
        }
      }
      else if (data->get("mac", o)) {
        uint64_t mac = stringToMacAddress(o->stringValue().c_str());
        WTMacMap::iterator pos = macs.find(mac);
        if (pos!=macs.end()) {
          pos->second->hidden = true;
        }
      }
      return Error::ok();
    }
    else {
      return inherited::processRequest(aRequest);
    }
  }
  else {
    // decode properties
    if (data->get("minShowInterval", o, true)) {
      minShowInterval = o->doubleValue()*MilliSecond;
    }
    if (data->get("rememberWithoutSsid", o, true)) {
      rememberWithoutSsid = o->boolValue();
    }
    if (data->get("minRssi", o, true)) {
      minRssi = o->int32Value();
    }
    return err ? err : Error::ok();
  }
}


JsonObjectPtr WifiTrack::status()
{
  JsonObjectPtr answer = inherited::status();
  if (answer->isType(json_type_object)) {
    answer->add("minShowInterval", JsonObject::newDouble((double)minShowInterval/MilliSecond));
    answer->add("rememberWithoutSsid", JsonObject::newBool(rememberWithoutSsid));
    answer->add("minRssi", JsonObject::newInt32(minRssi));
  }
  return answer;
}


#define WIFITRACK_STATE_FILE_NAME "wifitrack_state.json"

ErrorPtr WifiTrack::load()
{
  JsonObjectPtr data = JsonObject::objFromFile(Application::sharedApplication()->dataPath(WIFITRACK_STATE_FILE_NAME).c_str(), NULL, 2048*1024);
  return dataImport(data);
}


ErrorPtr WifiTrack::save()
{
  JsonObjectPtr data = dataDump();
  return data->saveToFile(Application::sharedApplication()->dataPath(WIFITRACK_STATE_FILE_NAME).c_str());
}


JsonObjectPtr WifiTrack::dataDump()
{
  JsonObjectPtr ans = JsonObject::newObj();
  // macs
  JsonObjectPtr mans = JsonObject::newObj();
  for (WTMacMap::iterator mpos = macs.begin(); mpos!=macs.end(); ++mpos) {
    JsonObjectPtr m = JsonObject::newObj();
    m->add("lastrssi", JsonObject::newInt32(mpos->second->lastRssi));
    m->add("bestrssi", JsonObject::newInt32(mpos->second->bestRssi));
    m->add("worstrssi", JsonObject::newInt32(mpos->second->worstRssi));
    if (mpos->second->hidden) m->add("hidden", JsonObject::newBool(true));
    m->add("count", JsonObject::newInt64(mpos->second->seenCount));
    m->add("last", JsonObject::newInt64(MainLoop::mainLoopTimeToUnixTime(mpos->second->seenLast)));
    m->add("first", JsonObject::newInt64(MainLoop::mainLoopTimeToUnixTime(mpos->second->seenFirst)));
    JsonObjectPtr sarr = JsonObject::newArray();
    for (WTSSidMap::iterator spos = mpos->second->ssids.begin(); spos!=mpos->second->ssids.end(); ++spos) {
      sarr->arrayAppend(JsonObject::newString(spos->second->ssid));
    }
    m->add("ssids", sarr);
    mans->add(macAddressToString(mpos->first, ':').c_str(), m);
  }
  ans->add("macs", mans);
  // ssid details
  JsonObjectPtr sans = JsonObject::newObj();
  for (WTSSidMap::iterator spos = ssids.begin(); spos!=ssids.end(); ++spos) {
    JsonObjectPtr s = JsonObject::newObj();
    s->add("count", JsonObject::newInt64(spos->second->seenCount));
    s->add("last", JsonObject::newInt64(MainLoop::mainLoopTimeToUnixTime(spos->second->seenLast)));
    s->add("maccount", JsonObject::newInt64(spos->second->macs.size()));
    if (spos->second->hidden) s->add("hidden", JsonObject::newBool(true));
    sans->add(spos->first.c_str(), s);
  }
  ans->add("ssids", sans);
  return ans;
}


ErrorPtr WifiTrack::dataImport(JsonObjectPtr aData)
{
  if (!aData || !aData->isType(json_type_object)) return TextError::err("invalid state data - must be JSON object");
  // insert ssids
  JsonObjectPtr sobjs = aData->get("ssids");
  if (!sobjs) return TextError::err("missing 'ssids'");
  sobjs->resetKeyIteration();
  JsonObjectPtr sobj;
  string ssidstr;
  while (sobjs->nextKeyValue(ssidstr, sobj)) {
    WTSSidPtr s;
    WTSSidMap::iterator spos = ssids.find(ssidstr);
    if (spos!=ssids.end()) {
      s = spos->second;
    }
    else {
      s = WTSSidPtr(new WTSSid);
      s->ssid = ssidstr;
      ssids[ssidstr] = s;
    }
    JsonObjectPtr o;
    o = sobj->get("hidden");
    if (o) s->hidden = o->boolValue();
    o = sobj->get("count");
    if (o) s->seenCount += o->int64Value();
    o = sobj->get("last");
    MLMicroSeconds l = Never;
    if (o) l = MainLoop::unixTimeToMainLoopTime(o->int64Value());
    if (l>s->seenLast) s->seenLast = l;
  }
  // insert macs and links to ssids
  JsonObjectPtr mobjs = aData->get("macs");
  if (!mobjs) return TextError::err("missing 'macs'");
  mobjs->resetKeyIteration();
  JsonObjectPtr mobj;
  string macstr;
  while (mobjs->nextKeyValue(macstr, mobj)) {
    bool insertMac = false;
    uint64_t mac = stringToMacAddress(macstr.c_str());
    WTMacPtr m;
    WTMacMap::iterator mpos = macs.find(mac);
    if (mpos!=macs.end()) {
      m = mpos->second;
    }
    else {
      m = WTMacPtr(new WTMac);
      m->mac = mac;
      insertMac = true;
    }
    // links
    JsonObjectPtr sarr = mobj->get("ssids");
    for (int i=0; i<sarr->arrayLength(); ++i) {
      string ssidstr = sarr->arrayGet(i)->stringValue();
      if (!rememberWithoutSsid && ssidstr.empty() && sarr->arrayLength()==1) {
        insertMac = false;
      }
      WTSSidPtr s;
      WTSSidMap::iterator spos = ssids.find(ssidstr);
      if (spos!=ssids.end()) {
        s = spos->second;
      }
      else {
        s = WTSSidPtr(new WTSSid);
        s->ssid = ssidstr;
        ssids[ssidstr] = s;
      }
      m->ssids[ssidstr] = s;
      s->macs[mac] = m;
    }
    if (insertMac) {
      macs[mac] = m;
    }
    // other props
    JsonObjectPtr o;
    o = mobj->get("hidden");
    if (o) m->hidden = o->boolValue();
    o = mobj->get("count");
    if (o) m->seenCount += o->int64Value();
    o = mobj->get("bestrssi");
    int r = -9999;
    if (o) r = o->int32Value();
    if (r>m->bestRssi) m->bestRssi = r;
    o = mobj->get("worstrssi");
    r = 9999;
    if (o) r = o->int32Value();
    if (r<m->worstRssi) m->worstRssi = r;
    o = mobj->get("last");
    MLMicroSeconds l = Never;
    if (o) l = MainLoop::unixTimeToMainLoopTime(o->int64Value());
    if (l>m->seenLast) {
      m->seenLast = l;
      o = mobj->get("lastrssi");
      if (o) m->lastRssi = o->int32Value();
    }
    o = mobj->get("first");
    l = Never;
    if (o) l = MainLoop::unixTimeToMainLoopTime(o->int64Value());
    if (l!=Never && m->seenFirst!=Never && l<m->seenFirst) m->seenFirst = l;
  }
  return ErrorPtr();
}




// MARK: ==== wifitrack operation


void WifiTrack::initOperation()
{
  LOG(LOG_NOTICE, "initializing wifitrack");

  ErrorPtr err;
  err = load();
  if (!Error::isOK(err)) {
    LOG(LOG_ERR, "could not load state: %s", Error::text(err).c_str());
  }
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
    // 17:40:22.356367 1.0 Mb/s 2412 MHz 11b -75dBm signal -75dBm signal antenna 0 -109dBm signal antenna 1 BSSID:5c:49:79:6d:28:1a (oui Unknown) DA:5c:49:79:6d:28:1a (oui Unknown) SA:c8:bc:c8:be:0d:0a (oui Unknown) Probe Request (iWay_Fiber_bu725) [1.0* 2.0* 5.5* 11.0* 6.0 9.0 12.0 18.0 Mbit]
    bool decoded = false;
    int rssi = 0;
    uint64_t mac = 0;
    string ssid;
    size_t s,e;
    // - rssi (signal)
    e = line.find(" signal ");
    if (e!=string::npos) {
      s = line.rfind(" ", e-1);
      if (s!=string::npos) {
        sscanf(line.c_str()+s+1, "%d", &rssi);
      }
      // - sender MAC (source address)
      s = line.find("SA:");
      if (s!=string::npos) {
        mac = stringToMacAddress(line.c_str()+s+3);
        // - SSID name
        s = line.find("Probe Request (", s);
        if (s!=string::npos) {
          s += 15;
          e = line.find(") ", s);
          ssid = line.substr(s, e-s);
          decoded = true;
        }
      }
    }
    if (decoded) {
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
        if (!s->ssid.empty() || rememberWithoutSsid) {
          m = WTMacPtr(new WTMac);
          m->mac = mac;
          macs[mac] = m;
        }
      }
      if (m) {
        m->seenCount++;
        m->seenLast = now;
        if (m->seenFirst==Never) m->seenFirst = now;
        m->lastRssi = rssi;
        if (rssi>m->bestRssi) m->bestRssi = rssi;
        if (rssi<m->worstRssi) m->worstRssi = rssi;
        // - connection
        m->ssids[ssid] = s;
        s->macs[mac] = m;
        // process sighting
        processSighting(m, s);
      }
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
  LOG(LOG_NOTICE, "MAC=%s (%ld), RSSI=%d,%d,%d : %s", macAddressToString(aMac->mac,':').c_str(), aMac->seenCount, aMac->worstRssi, aMac->lastRssi, aMac->bestRssi, s.c_str());
  if (!aMac->hidden && aMac->seenLast>aMac->shownLast+minShowInterval) {
    // pick SSID with the least mac links as most unique name
    long minMacs = 999999999;
    WTSSidPtr relevantSSid;
    for (WTSSidMap::iterator pos = aMac->ssids.begin(); pos!=aMac->ssids.end(); ++pos) {
      if (!pos->second->hidden && pos->second->macs.size()<minMacs && !pos->first.empty()) {
        minMacs = pos->second->seenCount;
        relevantSSid = pos->second;
      }
    }
    LOG(LOG_DEBUG, "minMacs = %ld, relevantSSid='%s'", minMacs, relevantSSid ? relevantSSid->ssid.c_str() : "<none>");
    if (relevantSSid) {
      // show it
      aMac->shownLast = aMac->seenLast;
      LOG(LOG_NOTICE, "*** Hello %s! ***", relevantSSid->ssid.c_str());
//      LethdApi::sharedApi()->runJsonScript("scripts/prepssid.json", boost::bind(&WifiTrack::ssidDispReady, this, relevantSSid->ssid), &scriptContext);
      ssidDispReady(relevantSSid->ssid);
    }
  }
}


void WifiTrack::ssidDispReady(string aSSid)
{
  JsonObjectPtr cmd = JsonObject::newObj();
  cmd->add("feature", JsonObject::newString("text"));
  cmd->add("text", JsonObject::newString(" "+aSSid));
  LethdApi::sharedApi()->executeJson(cmd);
  LethdApi::sharedApi()->runJsonScript("scripts/showssid.json", NULL, &scriptContext);
}


