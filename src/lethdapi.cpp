//
//  Copyright (c) 2016-2017 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Authors: Ueli Wahlen <ueli@hotmail.com>, Lukas Zeller <luz@plan44.ch>
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

#include "lethdapi.hpp"

#include "feature.hpp"
#include "macaddress.hpp"
#include "application.hpp"

using namespace p44;


// MARK: ===== LethdApiRequest

LethdApiRequest::LethdApiRequest(JsonObjectPtr aRequest, JsonCommPtr aConnection) :
  inherited(aRequest),
  connection(aConnection)
{
}


LethdApiRequest::~LethdApiRequest()
{
}


void LethdApiRequest::sendResponse(JsonObjectPtr aResponse, ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    aResponse = JsonObject::newObj();
    aResponse->add("Error", JsonObject::newString(aError->description()));
  }
  if (connection) connection->sendMessage(aResponse);
  LOG(LOG_INFO,"API answer: %s", aResponse->c_strValue());
}


// MARK: ===== InternalRequest

InternalRequest::InternalRequest(JsonObjectPtr aRequest) :
  inherited(aRequest)
{
  LOG(LOG_INFO,"Internal API request: %s", aRequest->c_strValue());
}


InternalRequest::~InternalRequest()
{
}


void InternalRequest::sendResponse(JsonObjectPtr aResponse, ErrorPtr aError)
{
  LOG(LOG_INFO,"Internal API answer: %s", aResponse->c_strValue());
}



// MARK: ===== LethdApi


LethdApi::LethdApi()
{
}


LethdApi::~LethdApi()
{
}


void LethdApi::executeJson(JsonObjectPtr aJsonCmds)
{
  if (aJsonCmds->isType(json_type_array)) {
    // array of commands
    for (int i=0; i<aJsonCmds->arrayLength(); i++) {
      ApiRequestPtr req = ApiRequestPtr(new InternalRequest(aJsonCmds->arrayGet(i)));
      processRequest(req);
    }
  }
  else {
    // single command
    ApiRequestPtr req = ApiRequestPtr(new InternalRequest(aJsonCmds));
    processRequest(req);
  }
}



void LethdApi::addFeature(FeaturePtr aFeature)
{
  featureMap[aFeature->getName()] = aFeature;
}


SocketCommPtr LethdApi::apiConnectionHandler(SocketCommPtr aServerSocketComm)
{
  JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
  conn->setMessageHandler(boost::bind(&LethdApi::apiRequestHandler, this, conn, _1, _2));
  conn->setClearHandlersAtClose(); // close must break retain cycles so this object won't cause a mem leak

  connection = conn;
  return conn;
}


void LethdApi::apiRequestHandler(JsonCommPtr aConnection, ErrorPtr aError, JsonObjectPtr aRequest)
{
  if (Error::isOK(aError)) {
    LOG(LOG_INFO,"API request: %s", aRequest->c_strValue());
    ApiRequestPtr req = ApiRequestPtr(new LethdApiRequest(aRequest, aConnection));
    aError = processRequest(req);
  }
  if (!Error::isOK(aError)) {
    // error
    JsonObjectPtr resp = JsonObject::newObj();
    resp->add("Error", JsonObject::newString(aError->description()));
    aConnection->sendMessage(resp);
    LOG(LOG_INFO,"API answer: %s", resp->c_strValue());
  }
}


void LethdApi::handleRequest(ApiRequestPtr aRequest)
{
  ErrorPtr err = processRequest(aRequest);
  if (err) {
    // something to send (empty response or error)
    aRequest->sendResponse(JsonObject::newObj(), err);
  }
}



ErrorPtr LethdApi::processRequest(ApiRequestPtr aRequest)
{
  JsonObjectPtr reqData = aRequest->getRequest();
  JsonObjectPtr o;
  // first check for feature selector
  if (reqData->get("feature", o, true)) {
    if (!o->isType(json_type_string)) {
      return LethdApiError::err("'feature' attribute must be a string");
    }
    string featurename = o->stringValue();
    FeatureMap::iterator f = featureMap.find(featurename);
    if (f==featureMap.end()) {
      return LethdApiError::err("unknown feature '%s'", featurename.c_str());
    }
    if (!f->second->isInitialized()) {
      return LethdApiError::err("feature '%s' is not yet initialized", featurename.c_str());
    }
    // let feature handle it
    ErrorPtr err = f->second->processRequest(aRequest);
    if (!Error::isOK(err)) {
      err->prefixMessage("Feature '%s' cannot process request: ", featurename.c_str());
    }
    return err;
  }
  else {
    // must be global command
    if (!reqData->get("cmd", o, true)) {
      return LethdApiError::err("missing 'feature' or 'cmd' attribute");
    }
    string cmd = o->stringValue();
    if (cmd=="init") {
      return init(aRequest);
    }
    else if (cmd=="reset") {
      return reset(aRequest);
    }
    else if (cmd=="now") {
      return now(aRequest);
    }
    else if (cmd=="status") {
      return status(aRequest);
    }
    else if (cmd=="ping") {
      return ping(aRequest);
    }
    else {
      return LethdApiError::err("unknown global command '%s'", cmd.c_str());
    }
  }
}



ErrorPtr LethdApi::reset(ApiRequestPtr aRequest)
{
  bool featureFound = false;
  for (FeatureMap::iterator f = featureMap.begin(); f!=featureMap.end(); ++f) {
    if (aRequest->getRequest()->get(f->first.c_str())) {
      featureFound = true;
      LOG(LOG_NOTICE, "resetting feature '%s'", f->first.c_str());
      f->second->reset();
    }
  }
  if (!featureFound) {
    return LethdApiError::err("reset does not address any known features");
  }
  return Error::ok(); // cause empty response
}



ErrorPtr LethdApi::init(ApiRequestPtr aRequest)
{
  bool featureFound = false;
  ErrorPtr err;
  JsonObjectPtr reqData = aRequest->getRequest();
  JsonObjectPtr o = reqData->get("gridcoordinate");
  if (o) {
    gridcoordinate = o->stringValue();
  }
  for (FeatureMap::iterator f = featureMap.begin(); f!=featureMap.end(); ++f) {
    JsonObjectPtr initData = reqData->get(f->first.c_str());
    if (initData) {
      featureFound = true;
      err = f->second->initialize(initData);
      if (!Error::isOK(err)) {
        err->prefixMessage("Feature '%s' init failed: ", f->first.c_str());
        return err;
      }
    }
  }
  if (!featureFound) {
    return LethdApiError::err("init does not address any known features");
  }
  return Error::ok(); // cause empty response
}


ErrorPtr LethdApi::now(ApiRequestPtr aRequest)
{
  JsonObjectPtr answer = JsonObject::newObj();
  answer->add("now", JsonObject::newInt64(MainLoop::unixtime()/MilliSecond));
  aRequest->sendResponse(answer, ErrorPtr());
  return ErrorPtr();
}


ErrorPtr LethdApi::status(ApiRequestPtr aRequest)
{
  JsonObjectPtr answer = JsonObject::newObj();
  // - list initialized features
  JsonObjectPtr features = JsonObject::newObj();
  for (FeatureMap::iterator f = featureMap.begin(); f!=featureMap.end(); ++f) {
    features->add(f->first.c_str(), f->second->status());
  }
  answer->add("features", features);
  // - grid coordinate
  answer->add("gridcoordinate", JsonObject::newString(gridcoordinate));
  // - MAC address and IPv4
  answer->add("macaddress", JsonObject::newString(macAddressToString(macAddress(), ':')));
  answer->add("ipv4", JsonObject::newString(ipv4ToString(ipv4Address())));
  answer->add("now", JsonObject::newInt64(MainLoop::unixtime()/MilliSecond));
  answer->add("version", JsonObject::newString(Application::sharedApplication()->version()));
  // - return
  aRequest->sendResponse(answer, ErrorPtr());
  return ErrorPtr();
}


ErrorPtr LethdApi::ping(ApiRequestPtr aRequest)
{
  JsonObjectPtr answer = JsonObject::newObj();
  answer->add("pong", JsonObject::newBool(true));
  aRequest->sendResponse(answer, ErrorPtr());
  return ErrorPtr();
}


void LethdApi::start(const string aApiPort)
{
  apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
  apiServer->setConnectionParams(NULL, aApiPort.c_str(), SOCK_STREAM, AF_INET6);
  apiServer->setAllowNonlocalConnections(true);
  apiServer->startServer(boost::bind(&LethdApi::apiConnectionHandler, this, _1), 3);
  LOG(LOG_INFO, "LEthDApi listening on %s", aApiPort.c_str());
}

void LethdApi::send(double aValue)
{
  if(!connection) return;
  JsonObjectPtr aMessage = JsonObject::newObj();
  aMessage->add("sensor", JsonObject::newDouble(aValue));
  connection->sendMessage(aMessage);
}

ErrorPtr LethdApiError::err(const char *aFmt, ...)
{
  Error *errP = new LethdApiError();
  va_list args;
  va_start(args, aFmt);
  errP->setFormattedMessage(aFmt, args);
  va_end(args);
  return ErrorPtr(errP);
}
