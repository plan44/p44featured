//
//  Copyright (c) 2019-2020 plan44.ch / Lukas Zeller, Zurich, Switzerland
//

#ifndef __p44utils__config__
#define __p44utils__config__

#ifndef ENABLE_EXPRESSIONS
  #define ENABLE_EXPRESSIONS 1 // Expression/Script engine support in some of the p44utils components
#endif
#ifndef EXPRESSION_JSON_SUPPORT
  #define EXPRESSION_JSON_SUPPORT 1 // structured JSON object support
#endif
#ifndef ENABLE_P44LRGRAPHICS
  #define ENABLE_P44LRGRAPHICS 1 // p44lrgraphics support in some of the p44utils components
#endif
#ifndef ENABLE_JSON_APPLICATION
  #define ENABLE_JSON_APPLICATION 1 // enables JSON utilities in Application, requires json-c
#endif
#ifndef ENABLE_UBUS
  #if P44_BUILD_OW
    #define ENABLE_UBUS 1 // ubus enabled on OpenWrt
  #endif
#endif



#endif // __p44utils__config__
