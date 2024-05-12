# RTEmsg command line application for binary log file decoding

![LogoRTEdbg–small](https://github.com/RTEdbg/RTEdbg/assets/144953452/e123f541-1d05-44ca-a85e-34a7abeded22)

## Introduction

RTEmsg is a Windows-based application that decodes binary captured data. It decodes according to format definition header files. The application is also a prebuild tool for syntax checking format definition files and enumerating format IDs and filter numbers. It inserts #define directives with filter and format ID numbers.

The RTEmsg application is part of the **[RTEdbg toolkit](https://github.com/RTEdbg/RTEdbg)**. With this toolkit it is possible to instrument the firmware of embedded systems and log data in real time. Since the data is not encoded or tagged, logging is very fast. Each logged data block contains an index (format ID) that specifies which format definition should be used for decoding. The assignment of numbers to format IDs is automatic and transparent to the programmer. Format definitions are printf-style strings familiar to programmers. See the key benefits and features of the new toolkit in **[RTEdbg Toolkit Presentation](https://github.com/RTEdbg/RTEdbg/releases/download/Documentation/RTEdbg.Presentation.pdf)**.

 **Note:** The source code for the RTEmsg application will be made available in this repository at a later date, after the documentation has been translated into English.

Coding the RTEmsg application in C was done for two main reasons:
1. The data decoding should be as fast as possible, so that the programmers don't have to wait long to analyze the data.
2. The binary data decoding is based on the C library function fprintf().

The author puts great emphasis on robustness and simplicity of the code. The GNU complexity level of the RTEmsg software is below 10 for all functions. However, bugs may still exist. Please report bugs or suggest improvements / corrections. The code is documented in Doxygen style, but the project is not yet ready to automatically generate documentation with Doxygen.

## Getting started
Complete documentation can be found in the **[RTEdbg manual](https://github.com/RTEdbg/RTEdbg/releases/download/Documentation/RTEdbg.library.and.tools.manual.pdf)**. See the **GETTING STARTED GUIDE** section for quick start instructions. 
A ZIP file containing the complete documentation and demo projects is available for download - go to the **[download page](https://github.com/RTEdbg/RTEdbg/releases)**.

## Getting help
Follow the [Contributing Guidelines](https://github.com/RTEdbg/RTEdbg/blob/master/Docs/CONTRIBUTING.md) for bug reports and feature requests regarding the RTEmsg. 
Please use **[RTEdbg.freeforums.net](https://rtedbg.freeforums.net/)** for general discussions about the RTEdbg toolkit. 
When asking a support question, be clear and take the time to explain your problem properly. If your problem is not strictly related to this toolkit, we recommend that you use [Stack Overflow](https://stackoverflow.com/) or similar forums instead.

## Repository structure
This repository only contains the RTEmsg application. See the **[RTEdbg main repository](https://github.com/RTEdbg/RTEdbg)** for links to all RTEdbg repositories &Rightarrow; *Repository Structure*.
