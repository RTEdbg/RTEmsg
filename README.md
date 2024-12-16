# RTEmsg command line application for binary log file decoding

![LogoRTEdbg–small](https://github.com/RTEdbg/RTEdbg/assets/144953452/e123f541-1d05-44ca-a85e-34a7abeded22)

## Introduction

**RTEmsg** is a Windows-based application that decodes binary captured data. It decodes according to format definition header files. The application is also a pre-build tool for syntax checking format definition files and enumerating format IDs and filter numbers. It inserts #define directives with filter and format ID numbers. See the *RTEdbg manual* for a detailed description - sections *RTEmsg MESSAGE DECODING APPLICATION* and *FORMAT DEFINITIONS*.

The RTEmsg application is part of the **[RTEdbg toolkit](https://github.com/RTEdbg/RTEdbg)**. With this toolkit, it is possible to instrument the firmware of embedded systems and log data in real time. Since the data is not encoded or tagged, logging is very fast. Each logged block of data contains an index (format ID) that specifies which format definition should be used for decoding. The assignment of numbers to format IDs is automatic and transparent to the programmer. Format definitions are printf-style strings familiar to programmers. See the key benefits and features of the toolkit in the **[RTEdbg Toolkit Presentation](https://github.com/RTEdbg/RTEdbg/releases/download/Documentation/RTEdbg.Presentation.pdf)**.

Coding the RTEmsg application in C was done for two main reasons:
1. The data decoding should be as fast as possible, so that the programmers don't have to wait long to analyze the data.
2. The binary data decoding is based on the C library function fprintf(). It is actually fprintf() running on the host instead of the embedded system.

The RTEmsg application was developed using the Visual Studio 2022 Community Edition toolchain. The code currently runs only on the Windows operating system. The main author is currently working on other parts of the RTEdbg project and porting RTEmsg is at the bottom of the to-do list. Credits go to Stefan Milivojčev, who participated in the development of the RTEmsg format definition parser as part of his master's thesis and helped with the Github workflows.

The following three configurations are available in the project
* **Debug** - basic debugging (default Visual Studio toolchain)
* **ReleaseWithDLL** - standard release version with DLL's (LLVM toolchain must be installed additionally)
* **ReleaseNoDLL** -  standard release version with direct access to the libray (LLVM toolchain must be installed additionally)

The LLVM (clang-cl) toolchain has been used to achieve faster code execution (compared to the Visual Studio Release version). The *ReleaseWithDLL* RTEmsg.exe version is included in the RTEdbg release ZIP file. The version generated with the *ReleaseNoDLL* configuration provides about 20% faster data decoding, but is not included in the RTEdbg toolkit release because some antivirus tools detect a virus in it.

The author puts great emphasis on robustness and simplicity of the code. The GNU complexity level of the current RTEmsg source code is below 10 for all functions, and only 6 of them have complexity above 5. The author has also tested the code review capabilities of an AI-powered code editor on this repository. The result is positive - improved documentation (code comments) and some minor bug fixes, especially considering the relatively small amount of time spent on this.
Bugs may still exist. Please report bugs or suggest improvements/corrections. The code is documented in Doxygen style, but the project is not ready to automatically generate documentation with Doxygen.

## Getting started
Complete documentation can be found in the **[RTEdbg manual](https://github.com/RTEdbg/RTEdbg/releases/download/Documentation/RTEdbg.library.and.tools.manual.pdf)**. See the **GETTING STARTED GUIDE** section for quick start instructions. 
A ZIP file containing the complete documentation and demo projects is available for download - go to the **[download page](https://github.com/RTEdbg/RTEdbg/releases)**.

## Getting help
Follow the [Contribution Guidelines](https://github.com/RTEdbg/RTEdbg/blob/master/docs/CONTRIBUTING.md) for bug reports and feature requests regarding RTEmsg. 
Please use **[RTEdbg.freeforums.net](https://rtedbg.freeforums.net/)** for general discussions about the RTEdbg toolkit. 
When asking a support question, be clear and take the time to explain your problem properly. If your problem is not strictly related to this toolkit, we recommend that you use [Stack Overflow](https://stackoverflow.com/) or similar forums instead.

## Repository structure
This repository contains only the RTEmsg application. See the **[RTEdbg main repository structure](https://github.com/RTEdbg/RTEdbg?tab=readme-ov-file#Repository-Structure)** for links to all RTEdbg repositories. <br>
The *Extract_msg* utility is used in the post-build of RTEmsg. See the [Extract_msg](https://github.com/RTEdbg/Extract_msg) repository for details.
