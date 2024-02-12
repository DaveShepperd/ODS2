# ODS2

ODS2 a program to read images of VAX/VMS disks.

This is a version of ods2 written by Paul Nankervis (paulnank@au1.ibm.com) subsequently modified by Hunter Goatley (goathunter@goatley.com), again by crwolff and again modified by me. Those email addresses might not work anymore.

These sources were snatched from the github [HERE](https://github.com/crwolff/ods2) and hacked up quite a bit. I don't know how to contact crwolff to let him know.

I only built and tested this version on a few different Linux systems. Specifically Fedora 39, Ubuntu, Ubuntu under WSL and PiOS v8 (32 bit version on a Pi5).

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

**NOTE 2: As an option to help recover files when copying `*.*`, if a record's format is VAR but the attributes are 0 (none), then the file is copied in binary.
Which means it will output the 2 byte record count, little endian, along with the record's data. That leaves the record structure in place. This will be indicated
by having the text `.binary` appended to the filename. I.e. if the source file was FOO.BAR, the result file will be FOO.BAR.binary.**

**NOTE 3: If while copying a file with a format of VAR or VFC and the record's count is bad and the /IGNORE copy option has been selected, the file will continue
to be copied but as binary. This means the bad record count and all remaining data will be copied to the output unchanged with nothing further omitted or added.
This action will be indicated by having the text `.corrupt_at_offset_xx` appended to the output's filename where `xx` is the offset in bytes from the start of
the output file. I.e. if the source file was FOO.BAR and it had an error at the output file's offset 1234, the output filename would be `FOO.BAR.corrupt_at_offset_1234`.**

## Building on Linux
```
make -f makefile.linux
```
## Building on Unix
```
make -f makefile.unix
```
## Usage
### Mount CD/DVD/Disk image
```
$> mount sr0
$> mount /dev/sr0
$> mount <image_file>
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

