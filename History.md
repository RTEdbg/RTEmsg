## RTEmsg revision history

### v1.00.00 - 2024-05-11
* Initial version

### v1.00.01 - 2024-10-12
* ms and us can also be used in the -time= switch
* Fixed a bug in a special mode of single shot data decoding
* Improved comments and fixed typos

### v1.00.02 - 2024-11-03
* Added Issues and Pull request templates

### v1.00.03 - 2024-12-16
* Added command line argument -ts
* Several small fixes - most of them translation to english and language correction of comments and variable/function names

### v1.01.00 - 2025-02-16
* Improved robustness
* Enhanced code documentation
* Fixed indexed text processing bug

### v1.01.01 - 2025-05-10
* Change to enable execution also under Windows 7

### v1.02.00 - 2025-11-22
* Added support for exporting timing data to Value Change Dump (VCD) files. <br> This allows users to visualize signal changes and timing relationships in standard waveform viewing tools.
* Added automatic generation of .gtkw files for GTKWave
* Empty strings are now allowed in format definitions.
* Support for RTE_MSG5() ... RTE_MSG8() macros added
* Fixed issue with indexed text printing
* Short text strings up to 6 characters long can be stored/recalled using MEMO functionality
* Enhanced validation of format definitions
* Default -ts=a;b parameter values changed
* If programmers do not provide a newline character (\n) at the end of a format string, one will be added automatically when printing to Main.log. The new algorithm prevents double newlines in Main.log.