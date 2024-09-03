# Berberis

Dynamic binary translator to run Android apps with riscv64 native code on x86_64 devices or emulators.

Supported extensions include Zb* (bit manipulation) and most of Zv (vector). Some less commonly used vector instructions are not yet implemented, but Android CTS and some Android apps run with the current set of implemented instructions.

Public mailing list: berberis-discuss@googlegroups.com

## Getting Started

Note: Googlers, read go/berberis and go/berberis-start first.

### Build

From your Android root checkout, run:

```
source build/envsetup.sh
lunch sdk_phone64_x86_64_riscv64-trunk_staging-eng
m berberis_all
```

For development, we recommend building all existing targets before uploading changes, since they are currently not always synchronized with `berberis_all`:

```
mmm frameworks/libs/binary_translation
```

### Run Hello World

```
out/host/linux-x86/bin/berberis_program_runner_riscv64 \
out/target/product/emu64xr/testcases/berberis_hello_world_static.native_bridge/x86_64/berberis_hello_world_static
```

On success `Hello!` will be printed.

### Run unit tests on host

```
m berberis_all berberis_run_host_tests
```

or

```
out/host/linux-x86/nativetest64/berberis_host_tests/berberis_host_tests
```

### Build and run emulator with Berberis support

```
m
emulator -memory 4096 -writable-system -partition-size 65536 -qemu -cpu host &
```

### Run unit tests on device or emulator

Note: Requires a running device or emulator with Berberis support.

1. Sync tests to the device:

```
adb root
adb sync data
```

2. Run guest loader tests:

```
adb shell /data/nativetest64/berberis_guest_loader_riscv64_tests/berberis_guest_loader_riscv64_tests
```

3. Run program tests:

```
adb shell /data/nativetest64/berberis_ndk_program_tests/berberis_ndk_program_tests
```

## Bionic unit tests

Note: Requires a running device or emulator with Berberis support.

1. Build Bionic unit tests:

```
m TARGET_PRODUCT=aosp_riscv64 bionic-unit-tests
```

2. Push tests to emulator or device:

```
adb push out/target/product/generic_riscv64/data/nativetest64/bionic-loader-test-libs /data/local/tmp
adb push out/target/product/generic_riscv64/data/nativetest64/bionic-unit-tests /data/local/tmp
```

3. Run Bionic tests:

```
adb shell /system/bin/berberis_program_runner_riscv64 /data/local/tmp/bionic-unit-tests/bionic-unit-tests
```

## Development

### Running Android Apps (APKs) on Berberis Emulator

The steps for running Android apps on a Berberis emulator simply involve building and running the emulator (see instructions above), building your Android app, then installing it onto the emulator using `adb`.

For example you can build then run the JNI test APK (see `tests/jni_tests/README.txt`) by running:

```
m TARGET_BUILD_VARIANT=userdebug TARGET_PRODUCT=aosp_riscv64 berberis_jni_tests
```

Install the app:

```
adb install out/target/product/generic_riscv64/testcases/berberis_jni_tests/riscv64/berberis_jni_tests.apk
```

Start the app:

```
adb shell am instrument -w com.berberis.jnitests/androidx.test.runner.AndroidJUnitRunner
```

Uninstall the app:

```
adb uninstall com.berberis.jnitests
```

## Debugging

### Crash Reporting for Guest State

When native code crashes a basic crash dump is written to `logcat` and a more detailed tombstone file is written to `/data/tombstones`. The tombstone file contains extra data about the crashed process. In particular, it contains stack traces for all the host threads and guest threads in the crashing process (not just the thread that caught the signal), a full memory map, and a list of all open file descriptors.

To find the tombstone file, use `adb` to access the device or emulator (run `adb root` if you don't have permissions) and locate the file:

```
$ adb shell ls /data/tombstones/
tombstone_00  tombstone_00.pb
```
`tombstone_00` is the output in human-readable text.

Note: Guest thread information follows host thread information whenever it is available.

Example tombstone output:

```
*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
Build fingerprint: 'Android/sdk_phone64_x86_64_riscv64/emu64xr:VanillaIceCream/MAIN/eng.sijiec.20240510.182325:eng/test-keys'
Revision: '0'
ABI: 'x86_64'
Guest architecture: 'riscv64'
Timestamp: 2024-05-13 18:38:56.175592859+0000
Process uptime: 3s
Cmdline: com.berberis.jnitests
pid: 2875, tid: 2896, name: roidJUnitRunner  >>> com.berberis.jnitests <<<
uid: 10147
signal 11 (SIGSEGV), code -6 (SI_TKILL), fault addr --------
    rax 0000000000000000  rbx 00007445aebb0000  rcx 000074458c73df08  rdx 000000000000000b
    r8  00007442f18487d0  r9  00007442f18487d0  r10 00007442ecc87770  r11 0000000000000206
    r12 0000000000000000  r13 00000000002e4e64  r14 00007445aebaf020  r15 00007442ed113d10
    rdi 0000000000000b3b  rsi 0000000000000b50
    rbp 00000000aebaf401  rsp 00007442ed111948  rip 000074458c73df08

7 total frames
backtrace:
      #00 pc 0000000000081f08  /apex/com.android.runtime/lib64/bionic/libc.so (syscall+24) (BuildId: 071397dbd1881d18b5bff5dbfbd86eb7)
      #01 pc 00000000014cca92  /system/lib64/libberberis_riscv64.so (berberis::RunGuestSyscall(berberis::ThreadState*)+82) (BuildId: f3326eacda7666bc0e85d13ef7281630)
      #02 pc 000000000037d955  /system/lib64/libberberis_riscv64.so (berberis::Decoder<berberis::SemanticsPlayer<berberis::Interpreter> >::DecodeSystem()+133) (BuildId: f3326eacda7666bc0e85d13ef7281630)
      #03 pc 000000000037b4cf  /system/lib64/libberberis_riscv64.so (berberis::Decoder<berberis::SemanticsPlayer<berberis::Interpreter> >::DecodeBaseInstruction()+831) (BuildId: f3326eacda7666bc0e85d13ef7281630)
      #04 pc 000000000037a9f4  /system/lib64/libberberis_riscv64.so (berberis::InterpretInsn(berberis::ThreadState*)+100) (BuildId: f3326eacda7666bc0e85d13ef7281630)
      #05 pc 00000000002c7325  /system/lib64/libberberis_riscv64.so (berberis_entry_Interpret+21) (BuildId: f3326eacda7666bc0e85d13ef7281630)
      #06 pc 114f9329b57c0dac  <unknown>

memory near rbx:
    00007445aebaffe0 0000000000000000 0000000000000000  ................
    00007445aebafff0 0000000000000000 0000000000000000  ................
    00007445aebb0000 0000000000000000 00007442ecdb2000  ......... ..Bt..
    00007445aebb0010 0000000000125000 0000000000010000  .P..............
    00007445aebb0020 0000000000125000 00007442eced6ff0  .P.......o..Bt..
    00007445aebb0030 00007445aebae000 000074428dee7000  ....Et...p..Bt..
    00007445aebb0040 000074428dee8000 00007445aebaf020  ....Bt.. ...Et..
    00007445aebb0050 0000000000000000 0000000000000000  ................
    00007445aebb0060 00007442e7d9b448 00007443b7836ce0  H...Bt...l..Ct..
    00007445aebb0070 00007442ed111ab0 0000000000000000  ....Bt..........
    00007445aebb0080 0000000000000000 0000000000000000  ................
    00007445aebb0090 0000000000000000 0000000000000000  ................
    00007445aebb00a0 0000000000000000 0000000000000000  ................
    00007445aebb00b0 0000000000000000 0000000000000000  ................
    00007445aebb00c0 0000000000000000 0000000000000000  ................
    00007445aebb00d0 0000000000000000 0000000000000000  ................

...snippet

05-13 18:38:55.898  2875  2896 I TestRunner: finished: testGetVersion(com.berberis.jnitests.JniTests)
05-13 18:38:55.900  2875  2896 I TestRunner: started: testRegisterNatives(com.berberis.jnitests.JniTests)
--- --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---
Guest thread information for tid: 2896
    pc  00007442942e4e64  ra  00007442ecc88b08  sp  00007442eced6fc0  gp  000074428dee8000
    tp  00007445aebae050  t0  0000000000000008  t1  00007442942ffb4c  t2  0000000000000000
    t3  00007442942e4e60  t4  0000000000000000  t5  8d38b33c8bd53145  t6  736574696e6a2e73
    s0  00007442eced6fe0  s1  000000000000002a  s2  0000000000000000  s3  0000000000000000
    s4  0000000000000000  s5  0000000000000000  s6  0000000000000000  s7  0000000000000000
    s8  0000000000000000  s9  0000000000000000  s10 0000000000000000  s11 0000000000000000
    a0  0000000000000b3b  a1  0000000000000b50  a2  000000000000000b  a3  00007442ecc87770
    a4  00007442f18487d0  a5  00007442f18487d0  a6  00007442f18487d0  a7  0000000000000083
    vlenb 0000000000000000

3 total frames
backtrace:
      #00 pc 000000000008de64  /system/lib64/riscv64/libc.so (tgkill+4) (BuildId: 7daa7d467f152da57592545534afd2ee)
      #01 pc 0000000000001b04  /data/app/~~_CJlJwewmTxNSIr4kxVv7w==/com.berberis.jnitests-7MJzLGAPUFFAMt5wl-D-Hg==/base.apk!libberberis_jni_tests.so (offset 0x1000) ((anonymous namespace)::add42(_JNIEnv*, _jclass*, int)+18) (BuildId: 665cb51828ad4b5e3ddf149af15b31cc)
      #02 pc 0000000000001004  /system/lib64/riscv64/libnative_bridge_vdso.so (BuildId: 3df95df99d97cad076b80c56aa20c552)

memory near pc (/system/lib64/riscv64/libc.so):
    00007442942e4e40 0000007308100893 01157363288578fd  ....s....x.(cs..
    00007442942e4e50 b39540a005338082 0000001300000013  ..3..@..........
    00007442942e4e60 0000007308300893 01157363288578fd  ..0.s....x.(cs..
    00007442942e4e70 b39140a005338082 0000001300000013  ..3..@..........
    00007442942e4e80 000000730d600893 01157363288578fd  ..`.s....x.(cs..
    00007442942e4e90 b31540a005338082 0000001300000013  ..3..@..........
    00007442942e4ea0 000000730dd00893 01157363288578fd  ....s....x.(cs..
    00007442942e4eb0 b31140a005338082 0000001300000013  ..3..@..........
    00007442942e4ec0 0000007307500893 01157363288578fd  ..P.s....x.(cs..
    00007442942e4ed0 b1d540a005338082 0000001300000013  ..3..@..........
    00007442942e4ee0 000000730a500893 01157363288578fd  ..P.s....x.(cs..
    00007442942e4ef0 b1d140a005338082 0000001300000013  ..3..@..........
    00007442942e4f00 0000007308d00893 01157363288578fd  ....s....x.(cs..
    00007442942e4f10 b15540a005338082 0000001300000013  ..3..@U.........
    00007442942e4f20 0000007308c00893 01157363288578fd  ....s....x.(cs..
    00007442942e4f30 b15140a005338082 0000001300000013  ..3..@Q.........

...snippet
```
