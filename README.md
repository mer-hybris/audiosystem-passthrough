audiosystem-passthrough
=======================

**For new adaptations audioflingerglue (and pulseaudio-modules-droid-glue)
should not be used!**

Features and command line arguments
-----------------------------------

### Operation modes (types)

 * af - AudioFlinger
   * Normal mode
   * Dummy mode - exposes the service but only replies OK to all method calls)
 * qti - IQcRilAudio
   * QCom devices which use vendor.qti.hardware.radio.am@1.0::IQcRilAudioCallback
     hidl service for setting voice call related parameters
 * hw2_0 - android.hardware.audio@2.0
   * Devices which use android.hardware.audio@2.0::IDevicesFactory/default and
     android.hardware.audio@2.0::IDevice services for setting voice call related parameters,
     but it have only dummy implementation for now

### Command line arguments

    -v --verbose   Verbose logging from audiosystem-passthrough
    -a --address   D-Bus address for PulseAudio module interface
    -i --idx       Starting index for binder calls, only applicable with operation type af
    -t --type      Passthrough type. Can be af or qti
    --module       Run as child process of PulseAudio module

    Also passthrough type and idx arguments can be provided from environment variables
    AUDIOSYSTEM_PASSTHROUGH_TYPE={qti,af,hw2_0}
    AUDIOSYSTEM_PASSTHROUGH_IDX={17,18} # only applicable to af type

How to use audiosystem-passthrough
----------------------------------

### As helper for pulseaudio-modules-droid-hidl

When run from the droid-hidl module the module tries to autodetect correct
configuration for the helper. If autodetection fails you can modify
PulseAudio sysconfig file (/etc/sysconfig/pulseaudio) and add following
environment variables:

    AUDIOSYSTEM_PASSTHROUGH_TYPE={qti,af,hw2_0}
    AUDIOSYSTEM_PASSTHROUGH_IDX={17,18} # only applicable to af type

Type qti is for devices which have qti HIDL interface IQcRilAudio.

### In standalone dummy mode

Package audiosystem-passthrough-dummy-af contains a service file which starts
audiosystem-passthrough in AudioFlinger dummy mode.
This can be used on devices where AudioFlinger service needs to be present
but isn't used for anything.

Package audiosystem-passthrough-dummy-hw2_0 contain service file which
starts audiosystem-passthrough in android.hardware.audio@2.0 dummy mode.
This can be used on devices where android.hardware.audio@2.0 service
needs to be present but isn't used for anything.

Configuration possibilites matrix
---------------------------------

What is needed and when

### Qualcomm chipsets

Android <= 8:
 * for voice calls: AudioFlinger
 * for media (camera etc): AudioFlinger (o)

Android >= 9:
 * for voice calls: IQcRilAudio
 * for media (camera etc): AudioFlinger (o)

### Other chipsets

All? Android versions:
 * for media (camera etc): AudioFlinger (o)

(o): Depends, that is, some devices might not need this but others do.

### What can satisfy dependencies

#### AudioFlinger

For voice calls, one of
 * pulseaudio-modules-droid-hidl and audiosystem-passthrough (with af type)
 * pulseaudio-modules-droid-glue and audioflingerglue

For media, one of
 * audiosystem-passthrough-dummy-af (from package audiosystem-passthrough)
 * audioflingerglue

If device needs AudioFlinger for both voice calls and media and a combination
of pa-modules-droid-hidl and audiosystem-passthrough is used a single instance
of audiosystem-passthrough running under pulseaudio-modules-droid-hidl is
enough.

#### IQcRilAudio

For voice calls,
 * pulseaudio-modules-droid-hidl and audiosystem-passthrough (with qti type)
