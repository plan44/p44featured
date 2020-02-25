//
//  Copyright (c) 2020 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44featured.
//
//  p44featured is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44featured is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44featured. If not, see <http://www.gnu.org/licenses/>.
//

#include "application.hpp"
#include "digitalio.hpp"
#include "analogio.hpp"
#include "jsoncomm.hpp"
#include "ledchaincomm.hpp"

#include "light.hpp"
#include "neuron.hpp"
#include "hermel.hpp"
#include "mixloop.hpp"
#include "wifitrack.hpp"
#include "dispmatrix.hpp"

#if ENABLE_UBUS
  #include "ubus.hpp"
#endif


using namespace p44;

#define DEFAULT_LOGLEVEL LOG_NOTICE

#if ENABLE_UBUS
static const struct blobmsg_policy logapi_policy[] = {
  { .name = "level", .type = BLOBMSG_TYPE_INT8 },
  { .name = "deltastamps", .type = BLOBMSG_TYPE_BOOL },
  { .name = NULL, .type = BLOBMSG_TYPE_INT32 },
};

static const struct blobmsg_policy p44featureapi_policy[] = {
  { .name = "method", .type = BLOBMSG_TYPE_STRING },
  { .name = NULL, .type = BLOBMSG_TYPE_UNSPEC },
};
#endif


// MARK: ==== Application

#define MKSTR(s) _MKSTR(s)
#define _MKSTR(s) #s


class P44FeatureD : public CmdLineApp
{
  typedef CmdLineApp inherited;

  // P44 device management JSON API Server
  SocketCommPtr p44mgmtApiServer;
  int requestsPending;

  #if ENABLE_UBUS
  // ubus API for P44 device management
  UbusServerPtr ubusApiServer;
  #endif

  #if ENABLE_LEDARRANGEMENT
  LEDChainArrangementPtr ledChainArrangement;
  #endif

  // LED+Button
  ButtonInputPtr button;
  IndicatorOutputPtr greenLed;
  IndicatorOutputPtr redLed;

  #if ENABLE_FEATURE_NEURON
  AnalogIoPtr sensor0;
  AnalogIoPtr sensor1;
  #endif
  #if ENABLE_FEATURE_LIGHT
  AnalogIoPtr pwmDimmer;
  #endif
  #if ENABLE_FEATURE_HERMEL
  AnalogIoPtr pwmLeft;
  AnalogIoPtr pwmRight;
  #endif

  FeatureApiPtr featureApi;

public:

  P44FeatureD() :
    requestsPending(0)
  {
  }

  virtual bool processOption(const CmdLineOptionDescriptor &aOptionDescriptor, const char *aOptionValue)
  {
    #if ENABLE_LEDARRANGEMENT
    if (strcmp(aOptionDescriptor.longOptionName,"ledchain")==0) {
      LEDChainArrangement::addLEDChain(ledChainArrangement, aOptionValue);
    }
    else
    #endif
    {
      return inherited::processOption(aOptionDescriptor, aOptionValue);
    }
    return true;
  }


  virtual int main(int argc, char **argv)
  {
    const char *usageText =
      "Usage: %1$s [options]\n";
    const CmdLineOptionDescriptor options[] = {
      #if ENABLE_FEATURE_DISPMATRIX
      { 0  , "dispmatrix",     true,  "numcols;start display matrix" },
      #endif
      #if ENABLE_FEATURE_NEURON
      { 0  , "neuron",         true,  "mvgAvgCnt,threshold,nAxonLeds,nBodyLeds;start neuron" },
      { 0  , "sensor0",        true,  "pinspec;analog sensor0 input to use" },
      { 0  , "sensor1",        true,  "pinspec;analog sensor1 input to use" },
      #endif
      #if ENABLE_FEATURE_RFIDS
      #endif
      #if ENABLE_FEATURE_LEDBARS
      #endif
      #if ENABLE_LEDARRANGEMENT
      CMDLINE_LEDCHAIN_OPTIONS,
      #endif
      #if ENABLE_FEATURE_HERMEL
      { 0  , "pwmleft",        true,  "pinspec;PWM left bumper output pin" },
      { 0  , "pwmright",       true,  "pinspec;PWM right bumper output pin" },
      { 0  , "hermel",         false, "start hermel" },
      #endif
      #if ENABLE_FEATURE_MIXLOOP
      { 0  , "mixloop",        false, "start mixloop" },
      { 0  , "ledchain2",      true,  "devicepath;ledchain2 device to use" },
      { 0  , "ledchain3",      true,  "devicepath;ledchain3 device to use" },
      #endif
      #if ENABLE_FEATURE_LIGHT
      { 0  , "light",          false, "start light" },
      { 0  , "pwmdimmer",      true,  "pinspec;PWM dimmer output pin" },
      #endif
      #if ENABLE_FEATURE_WIFITRACK
      { 0  , "wifitrack",      false, "start wifitrack" },
      { 0  , "wifimonif",      true,  "interface;wifi monitoring interface to use" },
      #endif
      { 0  , "featureapiport", true,  "port;server port number for Feature JSON API (default=none)" },
      { 0  , "initjson",       true,  "jsonfile;run the command(s) from the specified JSON text file." },
      { 0  , "featuretool",    true,  "feature;start a feature's command line tool" },
      { 0  , "jsonapiport",    true,  "port;server port number for management/web JSON API (default=none)" },
      #if ENABLE_UBUS
      { 0  , "ubusapi",        false, "enable ubus API for management/web" },
      #endif
      { 0  , "jsonapinonlocal",false, "allow JSON API from non-local clients" },
      { 0  , "jsonapiipv6",    false, "JSON API on IPv6" },
      { 0  , "button",         true,  "input pinspec;device button" },
      { 0  , "greenled",       true,  "output pinspec;green device LED" },
      { 0  , "redled",         true,  "output pinspec;red device LED" },
      DAEMON_APPLICATION_LOGOPTIONS,
      CMDLINE_APPLICATION_PATHOPTIONS,
      CMDLINE_APPLICATION_STDOPTIONS,
      { 0, NULL } // list terminator
    };

    // parse the command line, exits when syntax errors occur
    setCommandDescriptors(usageText, options);
    parseCommandLine(argc, argv);

    if ((numOptions()<1) || numArguments()>0) {
      // show usage
      showUsage();
      terminateApp(EXIT_SUCCESS);
    }

    // build objects only if not terminated early
    if (!isTerminated()) {
      int loglevel = DEFAULT_LOGLEVEL;
      getIntOption("loglevel", loglevel);
      SETLOGLEVEL(loglevel);
      int errlevel = LOG_ERR; // testing by default only reports to stdout
      getIntOption("errlevel", errlevel);
      SETERRLEVEL(errlevel, !getOption("dontlogerrors"));
      SETDELTATIME(getOption("deltatstamps"));

      // create button input
      button = ButtonInputPtr(new ButtonInput(getOption("button","missing")));
      button->setButtonHandler(boost::bind(&P44FeatureD::buttonHandler, this, _1, _2, _3), true, Second);
      // create LEDs
      greenLed = IndicatorOutputPtr(new IndicatorOutput(getOption("greenled","missing")));
      redLed = IndicatorOutputPtr(new IndicatorOutput(getOption("redled","missing")));

      #if ENABLE_LEDARRANGEMENT
      if (ledChainArrangement) {
        // led chain arrangement options
        int maxOutValue;
        if (getIntOption("ledchainmax", maxOutValue)) {
          ledChainArrangement->setMaxOutValue(maxOutValue);
        }
      }
      #endif

      // create API
      featureApi = FeatureApi::sharedApi();
      // add features
      #if ENABLE_FEATURE_LIGHT
      // - light
      pwmDimmer = AnalogIoPtr(new AnalogIo(getOption("pwmdimmer","missing"), true, 0)); // off to begin with
      featureApi->addFeature(FeaturePtr(new Light(
        pwmDimmer
      )));
      #endif
      #if ENABLE_FEATURE_HERMEL
      // - hermel
      pwmLeft = AnalogIoPtr(new AnalogIo(getOption("pwmleft","missing"), true, 0)); // off to begin with
      pwmRight = AnalogIoPtr(new AnalogIo(getOption("pwmright","missing"), true, 0)); // off to begin with
      featureApi->addFeature(FeaturePtr(new HermelShoot(
        pwmLeft, pwmRight
      )));
      #endif
      #if ENABLE_FEATURE_MIXLOOP
      // - mixloop
      featureApi->addFeature(FeaturePtr(new MixLoop(
        getOption("ledchain2","/dev/null"),
        getOption("ledchain3","/dev/null")
      )));
      #endif
      #if ENABLE_FEATURE_WIFITRACK
      // - wifitrack
      featureApi->addFeature(FeaturePtr(new WifiTrack(
        getOption("wifimonif","")
      )));
      #endif
      #if ENABLE_FEATURE_NEURON
      // - neuron
      sensor0 =  AnalogIoPtr(new AnalogIo(getOption("sensor0","missing"), false, 0));
      featureApi->addFeature(FeaturePtr(new Neuron(
        getOption("ledchain1","/dev/null"),
        getOption("ledchain2","/dev/null"),
        sensor0
      )));
      #endif
      #if ENABLE_FEATURE_DISPMATRIX
      // - dispmatrix
      featureApi->addFeature(FeaturePtr(new DispMatrix(ledChainArrangement)));
      #endif


      // use feature tools, if specified
      string featuretool;
      if (getStringOption("featuretool", featuretool)) {
        FeaturePtr tf = featureApi->getFeature(featuretool);
        if (tf) {
          terminateAppWith(tf->runTool());
        }
        else {
          terminateAppWith(TextError::err("No feature '%s' exists", featuretool.c_str()));
        }
      }
      if (!isTerminated()) {
        // run the initialisation command file
        string initFile;
        if (getStringOption("initjson", initFile)) {
          ErrorPtr err = featureApi->runJsonFile(initFile);
          if (!Error::isOK(err)) {
            terminateAppWith(err);
          }
        }
        // start p44featured TCP API server
        string apiport;
        if (getStringOption("featureapiport", apiport)) {
          featureApi->start(apiport);
        }
        // - create and start mg44 style API server for web interface
        if (getStringOption("jsonapiport", apiport)) {
          p44mgmtApiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
          p44mgmtApiServer->setConnectionParams(NULL, apiport.c_str(), SOCK_STREAM, getOption("jsonapiipv6") ? AF_INET6 : AF_INET);
          p44mgmtApiServer->setAllowNonlocalConnections(getOption("jsonapinonlocal"));
          p44mgmtApiServer->startServer(boost::bind(&P44FeatureD::apiConnectionHandler, this, _1), 3);
        }
        #if ENABLE_UBUS
        // - create and start UBUS API server for web interface on OpenWrt
        if (getOption("ubusapi")) {
          initUbusApi();
        }
        #endif
      } // if !terminated
    } // if !terminated
    // app now ready to run (or cleanup when already terminated)
    return run();
  }


  virtual void initialize()
  {
    LOG(LOG_NOTICE, "p44featured initialize()");
  }


  // MARK: ==== Button


  void buttonHandler(bool aState, bool aHasChanged, MLMicroSeconds aTimeSincePreviousChange)
  {
    LOG(LOG_INFO, "Button state now %d%s", aState, aHasChanged ? " (changed)" : " (same)");
  }



  // MARK: - ubus API

  #if ENABLE_UBUS

  void initUbusApi()
  {
    ubusApiServer = UbusServerPtr(new UbusServer(MainLoop::currentMainLoop()));
    UbusObjectPtr u = new UbusObject("p44featured", boost::bind(&P44FeatureD::ubusApiRequestHandler, this, _1, _2, _3));
    u->addMethod("log", logapi_policy);
    u->addMethod("featureapi", p44featureapi_policy);
    u->addMethod("quit");
    ubusApiServer->registerObject(u);
  }

  void ubusApiRequestHandler(UbusRequestPtr aUbusRequest, const string aMethod, JsonObjectPtr aJsonRequest)
  {
    if (aMethod=="log") {
      if (aJsonRequest) {
        JsonObjectPtr o;
        if (aJsonRequest->get("level", o)) {
          int oldLevel = LOGLEVEL;
          int newLevel = o->int32Value();
          SETLOGLEVEL(newLevel);
          LOG(newLevel, "\n\n========== changed log level from %d to %d ===============", oldLevel, newLevel);
        }
        if (aJsonRequest->get("deltastamps", o)) {
          SETDELTATIME(o->boolValue());
        }
      }
      aUbusRequest->sendResponse(JsonObjectPtr());
    }
    else if (aMethod=="quit") {
      LOG(LOG_WARNING, "terminated via UBUS quit method");
      terminateApp(1);
      aUbusRequest->sendResponse(JsonObjectPtr());
    }
    else if (aMethod=="featureapi") {
      ErrorPtr err;
      JsonObjectPtr result;
      if (aJsonRequest) {
        // run on featureAPI
        LOG(LOG_INFO,"ubus feature API request: %s", aJsonRequest->c_strValue());
        ApiRequestPtr req = ApiRequestPtr(new APICallbackRequest(aJsonRequest, boost::bind(&P44FeatureD::ubusFeatureApiRequestDone, this, aUbusRequest, _1, _2)));
        featureApi->handleRequest(req);
        return;
      }
      else {
        err = TextError::err("missing API command object");
      }
      ubusFeatureApiRequestDone(aUbusRequest, result, err);
    }
    else {
      // no other methods implemented yet
      aUbusRequest->sendResponse(JsonObjectPtr(), UBUS_STATUS_INVALID_COMMAND);
    }
  }

  void ubusFeatureApiRequestDone(UbusRequestPtr aUbusRequest, JsonObjectPtr aResult, ErrorPtr aError)
  {
    JsonObjectPtr response = JsonObject::newObj();
    if (aResult) response->add("result", aResult);
    if (aError) response->add("error", JsonObject::newString(aError->description()));
    LOG(LOG_INFO,"ubus feature API answer: %s", response->c_strValue());
    aUbusRequest->sendResponse(response);
  }


  #endif // ENABLE_UBUS



  // MARK: ==== p44 mg44 type API access


  SocketCommPtr apiConnectionHandler(SocketCommPtr aServerSocketComm)
  {
    JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
    conn->setMessageHandler(boost::bind(&P44FeatureD::apiRequestHandler, this, conn, _1, _2));
    conn->setClearHandlersAtClose(); // close must break retain cycles so this object won't cause a mem leak
    return conn;
  }


  void apiRequestHandler(JsonCommPtr aConnection, ErrorPtr aError, JsonObjectPtr aRequest)
  {
    // Decode mg44-style request (HTTP wrapped in JSON)
    if (Error::isOK(aError)) {
      LOG(LOG_INFO,"mg44 API request: %s", aRequest->c_strValue());
      JsonObjectPtr o;
      o = aRequest->get("method");
      if (o) {
        string method = o->stringValue();
        string uri;
        o = aRequest->get("uri");
        if (o) uri = o->stringValue();
        JsonObjectPtr data;
        bool upload = false;
        bool action = (method!="GET");
        // check for uploads
        string uploadedfile;
        if (aRequest->get("uploadedfile", o, true)) {
          uploadedfile = o->stringValue();
          upload = true;
          action = false; // other params are in the URI, not the POSTed upload
        }
        if (action) {
          // JSON data is in the request
          data = aRequest->get("data");
        }
        else {
          // URI params is the JSON to process
          data = aRequest->get("uri_params");
          if (!action && data) {
            data->resetKeyIteration();
            string k;
            JsonObjectPtr v;
            while (data->nextKeyValue(k, v)) {
              if (k!="rqvaltok") {
                // GET, but with query_params other than "rqvaltok": treat like PUT/POST with data
                action = true;
                break;
              }
            }
          }
          if (upload) {
            // move that into the request
            data->add("uploadedfile", JsonObject::newString(uploadedfile));
          }
        }
        // request elements now: uri and data
        requestsPending++;
        LOG(LOG_INFO, "+++ New request pending, total now %d", requestsPending);
        if (processRequest(uri, data, action, boost::bind(&P44FeatureD::requestHandled, this, aConnection, _1, _2))) {
          // done, callback will send response and close connection
          return;
        }
        requestsPending--;
        LOG(LOG_INFO, "--- Request handled, remaining pending now %d", requestsPending);
        // request cannot be processed, return error
        aError = WebError::webErr(404, "No handler found for request to %s", uri.c_str());
        LOG(LOG_ERR,"mg44 API: %s", aError->description().c_str());
      }
      else {
        aError = WebError::webErr(415, "Invalid JSON request format");
        LOG(LOG_ERR,"mg44 API: %s", aError->description().c_str());
      }
    }
    // return error
    requestHandled(aConnection, JsonObjectPtr(), aError);
  }


  void requestHandled(JsonCommPtr aConnection, JsonObjectPtr aResponse, ErrorPtr aError)
  {
    requestsPending--;
    LOG(LOG_INFO, "--- Request handled, remaining pending now %d", requestsPending);
    if (!aResponse) {
      aResponse = JsonObject::newObj(); // empty response
    }
    if (!Error::isOK(aError)) {
      aResponse->add("Error", JsonObject::newString(aError->description()));
    }
    LOG(LOG_INFO,"mg44 API answer: %s", aResponse->c_strValue());
    aConnection->sendMessage(aResponse);
    aConnection->closeAfterSend();
  }


  bool processRequest(string aUri, JsonObjectPtr aData, bool aIsAction, RequestDoneCB aRequestDoneCB)
  {
    ErrorPtr err;
    JsonObjectPtr o;
    if (aUri=="featureapi") {
      // p44featured API wrapper
      if (!aIsAction) {
        aRequestDoneCB(JsonObjectPtr(), WebError::webErr(415, "p44featured API calls must be action-type (e.g. POST)"));
      }
      ApiRequestPtr req = ApiRequestPtr(new APICallbackRequest(aData, aRequestDoneCB));
      featureApi->handleRequest(req);
      return true;
    }
    else if (aUri=="log") {
      if (aIsAction) {
        if (aData->get("level", o, true)) {
          int oldLevel = LOGLEVEL;
          SETLOGLEVEL(o->int32Value());
          LOG(LOGLEVEL, "\n==== changed log level from %d to %d ====\n", oldLevel, LOGLEVEL);
          aRequestDoneCB(JsonObjectPtr(), ErrorPtr());
          return true;
        }
      }
    }
    return false;
  }


  ErrorPtr processUpload(string aUri, JsonObjectPtr aData, const string aUploadedFile)
  {
    ErrorPtr err;

    string cmd;
    JsonObjectPtr o;
    if (aData->get("cmd", o, true)) {
      cmd = o->stringValue();
//      if (cmd=="imageupload") {
//        displayPage->loadPNGBackground(aUploadedFile);
//        gotoPage("display", false);
//        updateDisplay();
//      }
//      else
      {
        err = WebError::webErr(500, "Unknown upload cmd '%s'", cmd.c_str());
      }
    }
    return err;
  }

};





int main(int argc, char **argv)
{
  // prevent debug output before application.main scans command line
  SETLOGLEVEL(LOG_EMERG);
  SETERRLEVEL(LOG_EMERG, false); // messages, if any, go to stderr
  // create app with current mainloop
  static P44FeatureD application;
  // pass control
  return application.main(argc, argv);
}
