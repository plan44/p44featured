//
//  Copyright (c) 2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of lethd.
//
//  lethd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  lethd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with lethd. If not, see <http://www.gnu.org/licenses/>.
//

#include "application.hpp"
#include "digitalio.hpp"
#include "analogio.hpp"
#include "jsoncomm.hpp"
#include "ledchaincomm.hpp"

#include "lethdapi.hpp"

#include "light.hpp"
#include "neuron.hpp"
#include "hermel.hpp"
#include "mixloop.hpp"
#include "dispmatrix.hpp"


using namespace p44;

#define DEFAULT_LOGLEVEL LOG_NOTICE


// MARK: ==== Application

#define MKSTR(s) _MKSTR(s)
#define _MKSTR(s) #s


typedef boost::function<void (JsonObjectPtr aResponse, ErrorPtr aError)> RequestDoneCB;


class JsonWebAPIRequest : public ApiRequest
{
  typedef ApiRequest inherited;

  RequestDoneCB requestDoneCB;

public:

  JsonWebAPIRequest(JsonObjectPtr aRequest, RequestDoneCB aRequestDoneCB) :
    inherited(aRequest),
    requestDoneCB(aRequestDoneCB)
  {
  };


  void sendResponse(JsonObjectPtr aResponse, ErrorPtr aError) override
  {
    if (requestDoneCB) requestDoneCB(aResponse, aError);
  }

};



class LEthD : public CmdLineApp
{
  typedef CmdLineApp inherited;

  // API Server
  SocketCommPtr apiServer;

  // LED+Button
  ButtonInputPtr button;
  IndicatorOutputPtr greenLed;
  IndicatorOutputPtr redLed;

  AnalogIoPtr sensor0;
  AnalogIoPtr sensor1;
  AnalogIoPtr pwmDimmer;
  AnalogIoPtr pwmLeft;
  AnalogIoPtr pwmRight;

  LethdApiPtr lethdApi;


public:

  LEthD()
  {
  }

  virtual int main(int argc, char **argv)
  {
    const char *usageText =
      "Usage: %1$s [options]\n";
    const CmdLineOptionDescriptor options[] = {
      { 0  , "pwmdimmer",      true,  "pinspec;PWM dimmer output pin" },
      { 0  , "pwmleft",        true,  "pinspec;PWM left bumper output pin" },
      { 0  , "pwmright",       true,  "pinspec;PWM right bumper output pin" },
      { 0  , "sensor0",        true,  "pinspec;analog sensor0 input to use" },
      { 0  , "sensor1",        true,  "pinspec;analog sensor1 input to use" },
      { 0  , "ledchain1",      true,  "devicepath;ledchain1 device to use" },
      { 0  , "ledchain2",      true,  "devicepath;ledchain2 device to use" },
      { 0  , "ledchain3",      true,  "devicepath;ledchain3 device to use" },
      { 0  , "lethdapiport",   true,  "port;server port number for lETHd JSON API (default=none)" },
      { 0  , "initjson",       true,  "jsonfile;run the command(s) from the specified JSON text file." },
      { 0  , "neuron",         true,  "mvgAvgCnt,threshold,nAxonLeds,nBodyLeds;start neuron" },
      { 0  , "light",          false, "start light" },
      { 0  , "hermel",         false, "start hermel" },
      { 0  , "mixloop",        false, "start mixloop" },
      { 0  , "dispmatrix",     true,  "numcols;start display matrix" },
      { 0  , "jsonapiport",    true,  "port;server port number for JSON API (default=none)" },
      { 0  , "jsonapinonlocal",false, "allow JSON API from non-local clients" },
      { 0  , "jsonapiipv6",    false, "JSON API on IPv6" },
      { 0  , "button",         true,  "input pinspec;device button" },
      { 0  , "greenled",       true,  "output pinspec;green device LED" },
      { 0  , "redled",         true,  "output pinspec;red device LED" },
      { 'l', "loglevel",       true,  "level;set max level of log message detail to show on stdout" },
      { 0  , "errlevel",       true,  "level;set max level for log messages to go to stderr as well" },
      { 0  , "dontlogerrors",  false, "don't duplicate error messages (see --errlevel) on stdout" },
      { 0  , "deltatstamps",   false, "show timestamp delta between log lines" },
      { 'r', "resourcepath",   true,  "path;path to the images and sounds folders" },
      { 'd', "datapath",       true,  "path;path to the r/w persistent data" },
      { 'h', "help",           false, "show this text" },
      { 'V', "version",        false, "show version" },
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
      button->setButtonHandler(boost::bind(&LEthD::buttonHandler, this, _1, _2, _3), true, Second);
      // create LEDs
      greenLed = IndicatorOutputPtr(new IndicatorOutput(getOption("greenled","missing")));
      redLed = IndicatorOutputPtr(new IndicatorOutput(getOption("redled","missing")));

      // create sensor input(s)
      sensor0 =  AnalogIoPtr(new AnalogIo(getOption("sensor0","missing"), false, 0));
      sensor1 =  AnalogIoPtr(new AnalogIo(getOption("sensor1","missing"), false, 0));
      // create PWM output for light
      pwmDimmer = AnalogIoPtr(new AnalogIo(getOption("pwmdimmer","missing"), true, 0)); // off to begin with

      // create PWM outputs for bumpers
      pwmLeft = AnalogIoPtr(new AnalogIo(getOption("pwmleft","missing"), true, 0)); // off to begin with
      pwmRight = AnalogIoPtr(new AnalogIo(getOption("pwmright","missing"), true, 0)); // off to begin with

      // create API
      lethdApi = LethdApi::sharedApi();
      // add features
      // - light
      lethdApi->addFeature(FeaturePtr(new Light(
        pwmDimmer
      )));
      // - hermel
      lethdApi->addFeature(FeaturePtr(new HermelShoot(
        pwmLeft, pwmRight
      )));
      // - mixloop
      lethdApi->addFeature(FeaturePtr(new MixLoop(
        getOption("ledchain2","/dev/null"),
        getOption("ledchain3","/dev/null")
      )));
      // - neuron
      lethdApi->addFeature(FeaturePtr(new Neuron(
        getOption("ledchain1","/dev/null"),
        getOption("ledchain2","/dev/null"),
        sensor0,
        boost::bind(&LEthD::neuronSpike, this, _1)
      )));
      // - dispmatrix
      lethdApi->addFeature(FeaturePtr(new DispMatrix(
        getOption("ledchain1","/dev/null"),
        getOption("ledchain2","/dev/null"),
        getOption("ledchain3","/dev/null")
      )));
      // run the initialisation command file
      string initFile;
      if (getStringOption("initjson", initFile)) {
        lethdApi->runJsonScript(initFile);
      }
      // start lethd API server for leths server
      string apiport;
      if (getStringOption("lethdapiport", apiport)) {
        lethdApi->start(apiport);
      }
      // - create and start mg44 style API server for web interface
      if (getStringOption("jsonapiport", apiport)) {
        apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
        apiServer->setConnectionParams(NULL, apiport.c_str(), SOCK_STREAM, getOption("jsonapiipv6") ? AF_INET6 : AF_INET);
        apiServer->setAllowNonlocalConnections(getOption("jsonapinonlocal"));
        apiServer->startServer(boost::bind(&LEthD::apiConnectionHandler, this, _1), 3);
      }
    } // if !terminated
    // app now ready to run (or cleanup when already terminated)
    return run();
  }


  virtual void initialize()
  {
    LOG(LOG_NOTICE, "lethd initialize()");
  }


  void neuronSpike(double aValue) {
    lethdApi->send(aValue);
  }



  // MARK: ==== Button


  void buttonHandler(bool aState, bool aHasChanged, MLMicroSeconds aTimeSincePreviousChange)
  {
    LOG(LOG_INFO, "Button state now %d%s", aState, aHasChanged ? " (changed)" : " (same)");
  }




  // MARK: ==== API access


  SocketCommPtr apiConnectionHandler(SocketCommPtr aServerSocketComm)
  {
    JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
    conn->setMessageHandler(boost::bind(&LEthD::apiRequestHandler, this, conn, _1, _2));
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
        if (processRequest(uri, data, action, boost::bind(&LEthD::requestHandled, this, aConnection, _1, _2))) {
          // done, callback will send response and close connection
          return;
        }
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
    if (aUri=="lethd") {
      // lethd API wrapper
      if (!aIsAction) {
        aRequestDoneCB(JsonObjectPtr(), WebError::webErr(415, "lethd API calls must be action-type (e.g. POST)"));
      }
      ApiRequestPtr req = ApiRequestPtr(new JsonWebAPIRequest(aData, aRequestDoneCB));
      lethdApi->handleRequest(req);
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
  static LEthD application;
  // pass control
  return application.main(argc, argv);
}
