Metroid Prime Remastered CAUD/CSMP converter
============================================

This tool converts audio files (CAUD/CSMP) from Metroid Prime Remastered to much more useful standard GameCube DSP files.


System Requirements
===================

- a POSIX compatible system with a C compiler


Building
========

```sh
gcc -o csmp csmp.c
```


Usage
=====

- Compile the program
- Create a folder called `files`
- Get your ROM dump of Metroid Prime Remastered
- Extract the romfs somewhere, e.g. to a folder called `romfs`
- Get the pak extraction tool from [here](https://github.com/encounter/retrotool) and build it with `cargo build`
- Create a folder called `files` and enter it
- Extract all pak files of the game:

```sh
for file in `find ../romfs -name '*.pak'`; do paktool-rs pak extract "$file" .; done
```

- Go one folder up again so you're in the folder with the `files` folder in it
- Create a folder called `csmp` and another one called `log`
- Run the csmp program on all CAUD files:

```sh
for file in files/*.CAUD; do echo $file; csmp "$file" > log/$(basename "$file").log 2>&1; done
```

- Now you should have CSMP files renamed according to the names from the CAUD files. You should also have DSP files for most CSMP files.


Limitations
===========

The CAUD format is not fully understood. As a result, only the first sample referenced by a CAUD file is processed.


TODO
====

- Figure out the remaining unknown fields in CSMP files
- Figure out how CAUD files really work
