# ODS2

ODS2 a program to read images of VAX/VMS disks.

This is a version of ods2 written by Paul Nankervis (paulnank@au1.ibm.com) subsequently modified by Hunter Goatley (goathunter@goatley.com), again by crwolff and again modified by me. Those email addresses might not work anymore.

These sources were snatched from the github [HERE](https://github.com/crwolff/ods2) and hacked up quite a bit. I don't know how to contact crwolff to let him know.

I only built and tested this version on a few different Linux systems. Specifically Fedora 39, Ubuntu, Ubuntu under WSL and PiOS v8 (32 bit version on a Pi5). Added rules to build
on Mingw-64 and MSYS2 (either 32 or 64 bit). It won't build under MinGw-32. The readline libs are a potential problem under Windows. You might want to always build with USE_READLINE=0 when
building under MinGW and MSYS2. YMMV.

The changes I made:

* Fixed some compiler complaints when building with -Wall -ansi -pedantic -Wwrite-strings
* Added optional support for using readline() libs in order to get command line edit and history recall.
* Added build rules for _LARGEFILE64_SOURCE so it would unpack 4GB disk images with the 32bit API (lseek, etc).
* Added a '/full' option to the dir command to get more details about a file.
* Added the following options to the copy command:
  * /DIRS - make .DIR files while also making individual directories.
  * /IGNORE - don't stop if record error found. Switch to raw read mode and copy the rest of the file including the bad record count bytes.
  * /QUIET - don't report information messages about creating files.
  * /STREAM - copy stream format files in raw mode (don't look for a line end character and don't insert anything in output).
  * /TEST - don't copy anythihg, just report what would have been copied.
  * /TIME - keep file's creation date/time on copied file.
  * /VFC - Interpret carriage control on VFC Formatted files 
  * /VERBOSE - Squawk about what is being copied
* Report text description of RMS error message(s) in addition to the error number (didn't add text for others).
* Quite a bit of re-work to sys_get() and the copy command in ods2.

**NOTE 1: As of Feb 11, 2024, there continues to be an error in the handling of file extents if they exceed the default (20).
As a workaround, I set EXTMAX to 100 in makefile.linux which made it work with the images I had as samples. Your mileage may vary.**

**NOTE 2: VMS stored important information in the file's meta data. Namely the record's format, size and attributes.
Recovering files from VMS to plain Linux or Windows files could lose this important information. So to avoid data loss, some of that
meta data is appended to the file name. Not always, but in the cases where it would otherwise be lost. Those cases are if the record
format is FIXED whether or not there are record attributes or the format VAR or VFC and there are no record attributes. Or if during
recovery of a VAR/VFC format file the record count is corrupt or otherwise in error. Note in all those cases the file is copied without
modification. I.e. in binary mode. It is left as a exercise to the reader to further decode the file's contents. As an example, a recovered
file might appear as name.ext;ver;RFM;MRS;RAT(s) where 'name' is the filename, 'exe' is the file's extension, 'ver' is the file version number
'RFM' is the record format (one of UNDEF, FIXED, VAR, VFC, STMCRLF, STMLF, STMCR), 'MRS' is maximum record size (actual record size if RFM=FIXED),
'RAT(s)' are the record attributes (one or more of FTN, CR, PRN or BLK) indicating how the record should be terminated. The ';'s will be replaced
with the delimiter as provided by the --delim command line option.**

**NOTE 3: As an option to help recover files when copying, if a record's format is VAR/VFC but the attributes are 0 (none) or there was a problem
with the record's length (corruption), then the entire file is copied in binary. Which means it will output the 2 byte record count, little endian, along
with the record's data. That leaves the record structure in place. This will be indicated by having the record format appended to the filename as shown
in the NOTE 2 above. If the record count is the problem, additional text ';isCorruptAt;n' is appeneded to the filename where 'n' is the offset in the
file where the first instance of a bad record count was found.**

**NOTE 4: Look in fileFormats.html for some details about how VMS file formats are stored on disk.**

## Building on Linux
```
make -f Makefile.linux
```
## Building on Windows under MinGW64
```
make -f Makefile.mingw64
```
## Building on Windows under Msys2
```
make -f Makefile.msys2
```
## Building on PiOS
```
make -f Makefile.pios32
```
## Building on Unix
```
make -f makefile.unix (hasn't been tested. Might not work anymore)
```
## Usage
### Mount CD/DVD/Disk image
```
(NOTE: All filenames have to be expressed in VMS syntax. So no '/'s or '\'s can appear in them;
On Linux and Unix I suggest one use symlinks in the current default directory.
I don't know what to suggest as a workaround on Windows systems.
This means the second example won't work).
$> mount sr0
$> mount /dev/sr0     (This won't actually work due to the '/'s)
$> mount <image_file> (Likewise, make sure there are no '/'s or '\'s here either)
```
### Navigate contents
```
$> dir
$> set default [XYZ]
$> set default [.ABC]
$> set default [XYZ.ABC]
```
### Copy everything (flat hierarchy)
```
$> copy [...]*.* *.*
```
### Copy everything (preserve hierarchy)
```
$> copy [...]*.* [*]*.*
```

## Credits

Paul Nankervis / Hunter Goatley

Original README(s) in [aaareadme.txt](aaareadme.txt) and [aaareadme.too](aaareadme.too)

