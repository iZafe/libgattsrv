# Build a static library (libgattsrv.a), standalone and install both in --prefix and --bindir path
lib_LIBRARIES = libgattsrv.a


libgattsrv_a_CXXFLAGS = -fPIC -Wall -Wextra -std=gnu++17 $(GLIB_CFLAGS) $(GIO_CFLAGS) $(GOBJECT_CFLAGS)
libgattsrv_a_SOURCES = DBusInterface.cpp \
                   DBusInterface.h \
                   DBusMethod.cpp \
                   DBusMethod.h \
                   DBusObject.cpp \
                   DBusObject.h \
                   DBusObjectPath.h \
                   DosellGatt.cpp \
                   DosellGatt.h\
                   GattCharacteristic.cpp \
                   GattCharacteristic.h \
                   GattDescriptor.cpp \
                   GattDescriptor.h \
                   GattInterface.cpp \
                   GattInterface.h \
                   GattProperty.cpp \
                   GattProperty.h \
                   GattService.cpp \
                   GattService.h \
                   GattUuid.h \
                   Globals.h \
                   Gobbledegook.cpp \
                   ../include/Gobbledegook.h \
                   HciAdapter.cpp \
                   HciAdapter.h \
                   HciSocket.cpp \
                   HciSocket.h \
                   Init.cpp \
                   Init.h \
                   Logger.cpp \
                   Logger.h \
                   Mgmt.cpp \
                   Mgmt.h \
                   ServerUtils.cpp \
                   ServerUtils.h \
                   standalone.cpp \
                   TickEvent.h \
                   Utils.cpp \
                   Utils.h
# Install only the Gobbledegook.h header file
include_HEADERS = ../include/Gobbledegook.h

# Build our standalone server (linking statically with libgattsrv.a, linking dynamically with GLib)
# We can remove this after test
bin_PROGRAMS = standalone
standalone_CXXFLAGS = -fPIC -Wall -Wextra -std=gnu++17 
standalone_SOURCES = standalone.cpp
standalone_LDADD = libgattsrv.a -lglib-2.0 -lgio-2.0 -lgobject-2.0 
standalone_LDLIBS = $(GLIB_LIBS) $(GIO_LIBS) $(GOBJECT_LIBS) 

