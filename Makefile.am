AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

bin_PROGRAMS = p44featured

# p44featured

if DEBUG
p44featured_DEBUG = -D DEBUG=1
else
p44featured_DEBUG =
endif

if P44_BUILD_OW
p44featured_PLATFORM = -D P44_BUILD_OW=1
else
if P44_BUILD_RB
p44featured_PLATFORM = -D P44_BUILD_RB=1
else !P44_BUILD_RB
p44featured_PLATFORM = -D DISABLE_I2C=1
endif !P44_BUILD_RB
endif !P44_BUILD_OW


# WS281x LED chain support on Raspberry Pi (direct PWM/PCM hardware access)
if ENABLE_RPIWS281X

RPIWS281X_SOURCES = \
  src/p44utils/thirdparty/rpi_ws281x/clk.h \
  src/p44utils/thirdparty/rpi_ws281x/gpio.h \
  src/p44utils/thirdparty/rpi_ws281x/dma.h \
  src/p44utils/thirdparty/rpi_ws281x/dma.c \
  src/p44utils/thirdparty/rpi_ws281x/pcm.h \
  src/p44utils/thirdparty/rpi_ws281x/pcm.c \
  src/p44utils/thirdparty/rpi_ws281x/pwm.h \
  src/p44utils/thirdparty/rpi_ws281x/pwm.c \
  src/p44utils/thirdparty/rpi_ws281x/rpihw.h \
  src/p44utils/thirdparty/rpi_ws281x/rpihw.c \
  src/p44utils/thirdparty/rpi_ws281x/mailbox.h \
  src/p44utils/thirdparty/rpi_ws281x/mailbox.c \
  src/p44utils/thirdparty/rpi_ws281x/ws2811.h \
  src/p44utils/thirdparty/rpi_ws281x/ws2811.c
RPIWS281X_FLAGS = \
  -D ENABLE_RPIWS281X=1 \
  -I ${srcdir}/src/p44utils/thirdparty/rpi_ws281x

else

RPIWS281X_FLAGS = -D ENABLE_RPIWS281X=0

endif


# ubus support via libubus
if ENABLE_UBUS

UBUS_SOURCES = \
  src/p44utils/ubus.cpp \
  src/p44utils/ubus.hpp
UBUS_FLAGS = -D ENABLE_UBUS=1

else

UBUS_FLAGS = -D ENABLE_UBUS=0

endif


# websocket support via libuwsc
if ENABLE_UWSC

UWSC_SOURCES = \
  src/p44utils/websocket.cpp \
  src/p44utils/websocket.hpp
UWSC_FLAGS = -D ENABLE_UWSC=1

else

UWSC_FLAGS = -D ENABLE_UWSC=0

endif


# libev based mainloop
if ENABLE_EV

EV_FLAGS = -D MAINLOOP_LIBEV_BASED=1

endif


# PNG support via libpng
if ENABLE_PNG

PNG_FLAGS = \
  -D ENABLE_PNG=1

else

PNG_FLAGS = -D ENABLE_PNG=0

endif


# Note: no programmatic SSL lib loading in civetweb (NO_SSL_DL)
p44featured_LDADD = \
  ${PTHREAD_CFLAGS} \
  ${PTHREAD_LIBS} \
  ${JSONC_LIBS} \
  ${SQLITE3_LIBS} \
  ${PNG_LIBS} \
  ${UBUS_LIBS} \
  ${EV_LIBS} \
  ${UWSC_LIBS} \
  ${RPIWS281X_LIBS}
p44featured_EXTRACFLAGS = -D NO_SSL_DL=1

p44featured_CPPFLAGS = \
  -I ${srcdir}/src \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src/p44lrgraphics \
  -I ${srcdir}/src/p44features \
  -I ${srcdir}/src/p44utils/thirdparty/civetweb \
  -idirafter ${srcdir}/src/p44utils/thirdparty \
  -D DISABLE_I2C=0 \
  -D DISABLE_SPI=0 \
  ${RPIWS281X_FLAGS} \
  ${PNG_FLAGS} \
  ${UBUS_FLAGS} \
  ${EV_FLAGS} \
  ${UWSC_FLAGS} \
  ${BOOST_CPPFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${JSONC_CFLAGS} \
  ${SQLITE3_CFLAGS} \
  ${p44featured_EXTRACFLAGS} \
  ${p44featured_PLATFORM} \
  ${p44featured_DEBUG}

p44featured_SOURCES = \
  ${RPIWS281X_SOURCES} \
  ${UBUS_SOURCES} \
  ${UWSC_SOURCES} \
  src/p44utils/analogio.cpp \
  src/p44utils/analogio.hpp \
  src/p44utils/application.cpp \
  src/p44utils/application.hpp \
  src/p44utils/colorutils.cpp \
  src/p44utils/colorutils.hpp \
  src/p44utils/consolekey.cpp \
  src/p44utils/consolekey.hpp \
  src/p44utils/crc32.cpp \
  src/p44utils/crc32.hpp \
  src/p44utils/dcmotor.cpp \
  src/p44utils/dcmotor.hpp \
  src/p44utils/digitalio.cpp \
  src/p44utils/digitalio.hpp \
  src/p44utils/error.cpp \
  src/p44utils/error.hpp \
  src/p44utils/expressions.cpp \
  src/p44utils/expressions.hpp \
  src/p44utils/extutils.cpp \
  src/p44utils/extutils.hpp \
  src/p44utils/fdcomm.cpp \
  src/p44utils/fdcomm.hpp \
  src/p44utils/fnv.cpp \
  src/p44utils/fnv.hpp \
  src/p44utils/gpio.cpp \
  src/p44utils/gpio.h \
  src/p44utils/gpio.hpp \
  src/p44utils/httpcomm.cpp\
  src/p44utils/httpcomm.hpp\
  src/p44utils/i2c-dev-extensions.h \
  src/p44utils/i2c.cpp \
  src/p44utils/i2c.hpp \
  src/p44utils/igmp.cpp \
  src/p44utils/igmp.hpp \
  src/p44utils/iopin.cpp \
  src/p44utils/iopin.hpp \
  src/p44utils/jsoncomm.cpp \
  src/p44utils/jsoncomm.hpp \
  src/p44utils/jsonobject.cpp \
  src/p44utils/jsonobject.hpp \
  src/p44utils/jsonrpccomm.cpp \
  src/p44utils/jsonrpccomm.hpp \
  src/p44utils/jsonwebclient.cpp \
  src/p44utils/jsonwebclient.hpp \
  src/p44utils/ledchaincomm.cpp \
  src/p44utils/ledchaincomm.hpp \
  src/p44utils/logger.cpp \
  src/p44utils/logger.hpp \
  src/p44utils/macaddress.cpp \
  src/p44utils/macaddress.hpp \
  src/p44utils/mainloop.cpp \
  src/p44utils/mainloop.hpp \
  src/p44utils/operationqueue.cpp \
  src/p44utils/operationqueue.hpp \
  src/p44utils/p44obj.cpp \
  src/p44utils/p44obj.hpp \
  src/p44utils/p44script.cpp \
  src/p44utils/p44script.hpp \
  src/p44utils/p44utils_common.hpp \
  src/p44utils/p44utils_defs.hpp \
  src/p44utils/p44utils_minimal.hpp \
  src/p44utils/persistentparams.cpp \
  src/p44utils/persistentparams.hpp \
  src/p44utils/pwm.cpp \
  src/p44utils/pwm.hpp \
  src/p44utils/regexp.cpp \
  src/p44utils/regexp.hpp \
  src/p44utils/rfid.cpp \
  src/p44utils/rfid.hpp \
  src/p44utils/serialcomm.cpp \
  src/p44utils/serialcomm.hpp \
  src/p44utils/serialqueue.cpp \
  src/p44utils/serialqueue.hpp \
  src/p44utils/socketcomm.cpp \
  src/p44utils/socketcomm.hpp \
  src/p44utils/spi.cpp \
  src/p44utils/spi.hpp \
  src/p44utils/sqlite3persistence.cpp \
  src/p44utils/sqlite3persistence.hpp \
  src/p44utils/ssdpsearch.cpp \
  src/p44utils/ssdpsearch.hpp \
  src/p44utils/timeutils.cpp \
  src/p44utils/timeutils.hpp \
  src/p44utils/utils.cpp \
  src/p44utils/utils.hpp \
  src/p44utils/valueanimator.cpp \
  src/p44utils/valueanimator.hpp \
  src/p44utils/valueunits.cpp \
  src/p44utils/valueunits.hpp \
  src/p44utils/thirdparty/civetweb/civetweb.c \
  src/p44utils/thirdparty/civetweb/civetweb.h \
  src/p44utils/thirdparty/civetweb/handle_form.inl \
  src/p44utils/thirdparty/civetweb/md5.inl \
  src/p44utils/thirdparty/civetweb/sha1.inl \
  src/p44utils/thirdparty/slre/slre.c \
  src/p44utils/thirdparty/slre/slre.h \
  src/p44utils/thirdparty/sqlite3pp/sqlite3pp.cpp \
  src/p44utils/thirdparty/sqlite3pp/sqlite3pp.h \
  src/p44utils/thirdparty/sqlite3pp/sqlite3ppext.cpp \
  src/p44utils/thirdparty/sqlite3pp/sqlite3ppext.h \
  src/p44lrgraphics/canvasview.cpp \
  src/p44lrgraphics/canvasview.hpp \
  src/p44lrgraphics/coloreffectview.cpp \
  src/p44lrgraphics/coloreffectview.hpp \
  src/p44lrgraphics/epxview.cpp \
  src/p44lrgraphics/epxview.hpp \
  src/p44lrgraphics/imageview.cpp \
  src/p44lrgraphics/imageview.hpp \
  src/p44lrgraphics/lifeview.cpp \
  src/p44lrgraphics/lifeview.hpp \
  src/p44lrgraphics/lightspotview.cpp \
  src/p44lrgraphics/lightspotview.hpp \
  src/p44lrgraphics/p44lrg_common.hpp \
  src/p44lrgraphics/p44view.cpp \
  src/p44lrgraphics/p44view.hpp \
  src/p44lrgraphics/textview.cpp \
  src/p44lrgraphics/textview.hpp \
  src/p44lrgraphics/torchview.cpp \
  src/p44lrgraphics/torchview.hpp \
  src/p44lrgraphics/viewfactory.cpp \
  src/p44lrgraphics/viewfactory.hpp \
  src/p44lrgraphics/viewscroller.cpp \
  src/p44lrgraphics/viewscroller.hpp \
  src/p44lrgraphics/viewsequencer.cpp \
  src/p44lrgraphics/viewsequencer.hpp \
  src/p44lrgraphics/viewstack.cpp \
  src/p44lrgraphics/viewstack.hpp \
  src/p44features/featureapi.cpp \
  src/p44features/featureapi.hpp \
  src/p44features/light.cpp \
  src/p44features/light.hpp \
  src/p44features/hermel.cpp \
  src/p44features/hermel.hpp \
  src/p44features/mixloop.cpp \
  src/p44features/mixloop.hpp \
  src/p44features/rfids.cpp \
  src/p44features/rfids.hpp \
  src/p44features/indicators.cpp \
  src/p44features/indicators.hpp \
  src/p44features/inputs.cpp \
  src/p44features/inputs.hpp \
  src/p44features/splitflaps.cpp \
  src/p44features/splitflaps.hpp \
  src/p44features/wifitrack.cpp \
  src/p44features/wifitrack.hpp \
  src/p44features/neuron.cpp \
  src/p44features/neuron.hpp \
  src/p44features/dispmatrix.cpp \
  src/p44features/dispmatrix.hpp \
  src/p44features/feature.cpp \
  src/p44features/feature.hpp \
  src/p44features/p44features_common.hpp \
  src/p44features_config.hpp \
  src/p44utils_config.hpp \
  src/p44featured_main.cpp
