# DAEDAL

DAEDAL provides LLVM tools for Decoupled Access Execute.
It is currently being developed at Uppsala University.

## Getting Started

* Clone this repository to your local machine
* Compile **DAEDAL** by running make

## How to Generate DAEDAL Code

There are several pre-defined levels of indirection and granularity, as well as compilation target.

* Indirection indicates the maximum numver of indirections that should be considered for data prefetch. All loads with an indirection lower or equal to this number will be turned into prefetches.
* Granularity indicates the number of iterations that should be prefetched and consumed at a time
* Target can be choosen from: **DAE**, **CAE** and **ORIGINAL**, where DAE is Decoupled Access-Execute applied with both indirections and granularities, CAE is coupled Access-Execute applied with only granularities and ORIGINAL is default compiled without any transformations.

Changes of settings can be done under:
```
$(path to daedal)/sources/common/DAE/Makefile.targets
```
Variables *INDIR_COUNT* and *GRAN_COUNT* can be changed in #line.8 and #line.9 respectively.

Targets can be modified at #line.24.


## Small Benchmark Example

To illustrate the usage of DAEDAL, we provide one example benchmark which can be found at:
```
$(path to daedal)/sources/myBenchmark
```

* In order to build binaries, change $(path to daedal)/sources/common/Makefile.environment to contain the correct path to **DAEDAL**:
```
COMPILER_LIB=$(path to daedal)/daedal/compiler/build/projects-build/lib
LLVM_BIN=$(path to daedal)/daedal/compiler/build/llvm-build/bin/
```

* Run make in $(path to daedal)/sources (parallel make is allowed):
```
make
```

### File Generation

#### The flow of generated output files from **TARGET=DAE** goes as follows (which can also be observed from **Makefile.defaults**):

1) all **.c/.cpp/.cc** files get compiled to generate their respective **.ll** files

2) all **.ll** files are used to generate **.stats.ll** files which are used for purpose of observations on executable runtime statistics (access/execute times)

3) all **.stats.ll** files are used to mark hot loops for transformation and output **.marked.ll** files

4) all **.marked.ll** files are used to chunck marked loops and output **.gran.ll** files

5) two global files **DAE-header.ll** and **Globals.ll** are genarated (once only). They contain the value that should be used for granulairty. For each granularity
**DAEDAL** will create a copy and replace the granularity with the current one. This copy will then be linked into the final benchmark.

6) based on *INDIR_COUNT* setting, for each indirection Y,

  6.1) based on *GRAN_COUNT* settings, for each granularity X **.gran.ll** files are used for loop extraction and output **.gran**X**.extract.ll** files
  
  6.1.a) remove redundant prefetches from **.gran**X**.extract.ll** file and output **.gran**X**.indir**Y**.dae.ll** file
    
  6.1.b) Apply pretches on \_\_kernel\_\_ marked functions from **.gran**X**.indir**Y**.dae.ll** file and output **.gran**X**.indir**Y**.dae.O3.ll** file
    
  6.1.c) Global information from **DAE-header.ll** and **Globals.ll** are attached together and a new file **.gran**X**.indir**Y**.dae.GV_DAE.ll** is created
    
  6.1.d) with files from 6.1.b) and 6.1.c), an executable file **.gran**X**.indir**Y**.dae** then gets generated
    

#### The flow of generated output files for **TARGET=CAE** is analogous but simpler by skipping steps 6.1.a) and 6.1.b) from above, in step 6.1.d) files are from 6.1) and 6.1.c).


#### All compilation process is recorded and can be found in:
```
$(path to daedal)/sources/myBenchmark/bin/log.txt
```

### Adapt **DAEDAL** to run your own benchmarks
Feel free to try the example benchmark and change/extend to your own applications.

* Copy the **myBenchmark** directory and replace the sources with your application's source.
* Change the **Makefile** in  your new benchmark directory to use the correct sources, compiler flags, and name:
```
$(path to daedal)/sources/myBenchmark/src/Makefile
```
* Change the source **Makefile** in order to build your benchmark. Add the name of the directory of your benchmark to the list in here:
```
$(path to daedal)/sources/Makefile
```
* Mark the loop to transform (using a vectorize pragma). See an example here:
```
$(path to daedal)/sources/myBenchmark/src/small_benchmark.cpp
```

* You should be ready to compile your own benchmark now!

# Others

More detailed information on **DAEDAL** can be found in [**Multiversioned decoupled access-execute: the key to energy-efficient compilation of general-purpose programs**. Compiler Construction,  March 17-18, 2016: 121-131](http://dl.acm.org/citation.cfm?doid=2892208.2892209).

For more information please visit our [website](https://www.argodsm.com/).

Please contact us at contact@etascale.com
