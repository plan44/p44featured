//
//  Copyright (c) 2019-2020 plan44.ch / Lukas Zeller, Zurich, Switzerland
//

#ifndef __p44utils__config__
#define __p44utils__config__

#ifndef ENABLE_NAMED_ERRORS
  #define ENABLE_NAMED_ERRORS P44_CPP11_FEATURE // Enable if compiler can do C++11
#endif
#ifndef ENABLE_EXPRESSIONS
  #define ENABLE_EXPRESSIONS 0 // "Expressions" is deprecated, use "p44Script" instead
  #ifndef ENABLE_P44SCRIPT
    #define ENABLE_P44SCRIPT 1 // Scripting support in some of the p44utils components
  #endif
#else
  #ifdef ENABLE_P44SCRIPT
    #warning "ENABLE_EXPRESSIONS is mutually exclusive with ENABLE_P44SCRIPT"
  #else
    #define ENABLE_P44SCRIPT 0
  #endif
#endif
#ifndef ENABLE_P44LRGRAPHICS
  #define ENABLE_P44LRGRAPHICS 1 // p44lrgraphics support in some of the p44utils components
#endif
#ifndef ENABLE_APPLICATION_SUPPORT
  #define ENABLE_APPLICATION_SUPPORT 1 // support for Application (e.g. domain specific commandline options) in other parts of P44 utils
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
