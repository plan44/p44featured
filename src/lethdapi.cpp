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
  LOG(LOG_DEBUG,"Internal API request: %s", aRequest->c_strValue());
}


InternalRequest::~InternalRequest()
{
}


void InternalRequest::sendResponse(JsonObjectPtr aResponse, ErrorPtr aError)
{
  LOG(LOG_DEBUG,"Internal API answer: %s", aResponse->c_strValue());
}



// MARK: ===== LethdApi


static LethdApiPtr lethdApi;


LethdApiPtr LethdApi::sharedApi()
{
  if (!lethdApi) {
    lethdApi = LethdApiPtr(new LethdApi);
  }
  return lethdApi;
}



LethdApi::LethdApi()
{
}


LethdApi::~LethdApi()
{
}


ErrorPtr LethdApi::runJsonFile(const string aScriptPath, SimpleCB aFinishedCallback, ScriptContextPtr* aContextP, SubstitutionMap* aSubstitutionsP)
{
  ErrorPtr err;
  string jsonText;
  string fpath = Application::sharedApplication()->resourcePath(aScriptPath).c_str();
  FILE* f = fopen(fpath.c_str(), "r");
  if (f==NULL) {
    err = SysError::errNo();
    err->prefixMessage("cannot open JSON script file '%s': ", fpath.c_str());
    LOG(LOG_WARNING, "Script loading error: %s", Error::text(err).c_str());
  }
  else {
    string_fgetfile(f, jsonText);
    fclose(f);
    err = runJsonString(jsonText, aFinishedCallback, aContextP, aSubstitutionsP);
  }
  return err;
}


ErrorPtr LethdApi::runJsonString(string aJsonString, SimpleCB aFinishedCallback, ScriptContextPtr* aContextP, SubstitutionMap* aSubstitutionsP)
{
  ErrorPtr err;
  if (aSubstitutionsP) {
    // perform substitution: syntax of placeholders:
    //   @{name}
    size_t p = 0;
    while ((p = aJsonString.find("@{",p))!=string::npos) {
      size_t e = aJsonString.find("}",p+2);
      if (e==string::npos) {
        // syntactically incorrect, no closing "}"
        err = TextError::err("unterminated placeholder: %s", aJsonString.c_str()+p);
        break;
      }
      string var = aJsonString.substr(p+2,e-2-p);
      SubstitutionMap::iterator pos = aSubstitutionsP->find(var);
      if (pos==aSubstitutionsP->end()) {
        err = TextError::err("unknown placeholder: %s", var.c_str());
        break;
      }
      // replace, even if rep is empty
      aJsonString.replace(p, e-p+1, pos->second);
      p+=pos->second.size();
    }
  }
  if (Error::isOK(err)) {
    JsonObjectPtr script = JsonObject::objFromText(aJsonString.c_str(), -1, &err);
    if (Error::isOK(err) && script) {
      err = lethdApi->executeJson(script, aFinishedCallback, aContextP);
    }
  }
  if (!Error::isOK(err)) { LOG(LOG_WARNING, "Script execution error: %s", Error::text(err).c_str()); }
  return err;
}


ErrorPtr LethdApi::executeJson(JsonObjectPtr aJsonCmds, SimpleCB aFinishedCallback, ScriptContextPtr* aContextP)
{
  JsonObjectPtr cmds;
  if (!aJsonCmds->isType(json_type_array)) {
    cmds = JsonObject::newArray();
    cmds->arrayAppend(aJsonCmds);
  }
  else {
    cmds = aJsonCmds;
  }
  ScriptContextPtr context;
  if (aContextP && *aContextP) {
    context = *aContextP;
  }
  else {
    context = ScriptContextPtr(new ScriptContext);
    if (aContextP) *aContextP = context;
  }
  context->kill();
  executeNextCmd(cmds, 0, context, aFinishedCallback);
  return ErrorPtr();
}


void LethdApi::executeNextCmd(JsonObjectPtr aCmds, int aIndex, ScriptContextPtr aContext, SimpleCB aFinishedCallback)
{
  if (!aCmds || aIndex>=aCmds->arrayLength()) {
    // done
    if (aFinishedCallback) aFinishedCallback();
    return;
  }
  // run next command
  MLMicroSeconds delay = 0;
  JsonObjectPtr cmd = aCmds->arrayGet(aIndex);
  // check for delay
  JsonObjectPtr o = cmd->get("delayby");
  if (o) {
    delay = o->doubleValue()*MilliSecond;
  }
  // now execute
  aContext->scriptTicket.executeOnce(boost::bind(&LethdApi::runCmd, this, aCmds, aIndex, aContext, aFinishedCallback), delay);
}


void LethdApi::runCmd(JsonObjectPtr aCmds, int aIndex, ScriptContextPtr aContext, SimpleCB aFinishedCallback)
{
  JsonObjectPtr cmd = aCmds->arrayGet(aIndex);
  JsonObjectPtr o = cmd->get("callscript");
  if (o) {
    runJsonFile(o->stringValue(), boost::bind(&LethdApi::executeNextCmd, this, aCmds, aIndex+1, aContext, aFinishedCallback), &aContext);
    return;
  }
  ApiRequestPtr req = ApiRequestPtr(new InternalRequest(cmd));
  processRequest(req);
  executeNextCmd(aCmds, aIndex+1, aContext, aFinishedCallback);
}


FeaturePtr LethdApi::getFeature(const string aFeatureName)
{
  FeatureMap::iterator pos = featureMap.find(aFeatureName);
  if (pos==featureMap.end()) return FeaturePtr();
  return pos->second;
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
    if (cmd=="nop") {
      // no operation (e.g. script steps that only wait)
      return Error::ok();
    }
    if (cmd=="call") {
      return call(aRequest);
    }
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


ErrorPtr LethdApi::call(ApiRequestPtr aRequest)
{
  JsonObjectPtr reqData = aRequest->getRequest();
  JsonObjectPtr o;
  // check for subsititutions
  LethdApi::SubstitutionMap subst;
  o = reqData->get("substitutions");
  if (o) {
    string var;
    JsonObjectPtr val;
    o->resetKeyIteration();
    while (o->nextKeyValue(var, val)) {
      subst[var] = val->stringValue();
    }
  }
  o = reqData->get("script");
  if (o) {
    string scriptName = o->stringValue();
    ErrorPtr err = runJsonFile(scriptName, NULL, NULL, &subst);
    if (Error::isOK(err)) return Error::ok();
    return err;
  }
  o = reqData->get("scripttext");
  if (o) {
    string scriptText = o->stringValue();
    ErrorPtr err = runJsonString(scriptText, NULL, NULL, &subst);
    if (Error::isOK(err)) return Error::ok();
    return err;
  }
  o = reqData->get("json");
  if (o) {
    ErrorPtr err = executeJson(o, NULL, NULL);
    if (Error::isOK(err)) return Error::ok();
    return err;
  }
  return LethdApiError::err("missing 'script', 'scripttext' or 'json' attribute");
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
  apiServer->startServer(boost::bind(&LethdApi::apiConnectionHandler, this, _1), 10);
  LOG(LOG_INFO, "LEthDApi listening on %s", aApiPort.c_str());
}


ErrorPtr LethdApi::sendMessage(JsonObjectPtr aMessage)
{
  if (!connection) return TextError::err("No API connection exists, cannot send message");
  return connection->sendMessage(aMessage);
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
