audiosystem-passthrough
=======================

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
 * combination of pulseaudio-modules-droid-glue and audioflingerglue
 * combination of pulseaudio-modules-droid-hidl and audiosystem-passthrough

For media, one of
 * audioflingerglue
 * audiosystem-passthrough-dummy-af (from package audiosystem-passthrough)

If device needs AudioFlinger for both voice calls and media and a combination
of pa-modules-droid-hidl and audiosystem-passthrough is used a single instance
of audiosystem-passthrough running under pulseaudio-modules-droid-hidl is
enough.

#### IQcRilAudio

For voice calls,
 * combination of pulseaudio-modules-droid-hidl and audiosystem-passthrough

Helper for pulseaudio-modules-droid-hidl
----------------------------------------

When run from the droid-hidl module the module tries to autodetect correct
configuration for the helper. If this is not the case you can modify
PulseAudio sysconfig file (/etc/sysconfig/pulseaudio) and add following
environment variables:

    AUDIOSYSTEM_PASSTHROUGH_TYPE={qti,af}
    AUDIOSYSTEM_PASSTHROUGH_IDX={17,18} # only applicable to af type

Type qti is for devices which have qti HIDL interface IQcRilAudio.

Standalone dummy mode
---------------------

Package audiosystem-passthrough-dummy-af contain service file which starts
audiosystem-passthrough in AudioFlinger dummy mode.
This can be used on devices where AudioFlinger service needs to be present
but isn't used for anything.
