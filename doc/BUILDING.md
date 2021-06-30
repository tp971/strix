# Building

## Dependencies

 - C++ compiler supporting C++17 and OpenMP, e.g. [GCC](https://gcc.gnu.org/) 8 and later.
 - [GNU Make](https://www.gnu.org/software/make/).
 - [Boost](http://www.boost.org/) 1.53 or higher with the following libraries: program options, filesystem, iostreams and system.
 - [zlib](http://www.zlib.net/).
 - [CMake](https://cmake.org/) 3.11 or higher.
 - JDK 12, either from [Oracle](http://www.oracle.com/technetwork/java/javase/downloads/index.html) or [OpenJDK](http://openjdk.java.net/projects/jdk/12/).

**Ubuntu 19.10**

On Ubuntu 19.10, all dependencies can be installed as follows:
```
sudo apt install g++ cmake make libboost-dev libboost-program-options-dev libboost-filesystem-dev libboost-iostreams-dev zlib1g-dev openjdk-13-jdk
```

**Arch Linux**

On Arch Linux, all dependencies can be installed as follows:
```
sudo pacman -S gcc cmake make boost boost-libs zlib jdk-openjdk
sudo archlinux-java set java-12-openjdk
```

## Compilation

If the repository has been cloned with git, first the submodules have to be initialized.
If Strix has been obtained as a release zip file, this step can be skipped.
```
git submodule init
git submodule update
```

The compilation process can be started by:
```
make
```

After compilation, the folder `bin` will be created and should contain the executable `strix` and the library `owl.jar`.
To use Strix, the executable `strix` has to be invoked, but the `owl.jar` library has to remain in the same directory,
or the directory containing it has to be given with the `--owl-jar-dir` option.

## Optional dependencies

To run the test suite for Strix (`make test`) or the benchmarks (`benchmarks/run_benchmarks.sh`),
additional dependencies are needed to convert input files and verify the output:

- [SyfCo](https://github.com/meyerphi/syfco) for TLSF conversion.
- [combine-aiger](https://github.com/meyerphi/combine-aiger) for combining specification and implementation.
- [NuSMV](http://nusmv.fbk.eu/index.html) with `ltl2smv` binary in version 2.6.0.
- [nuXmv](https://es-static.fbk.eu/tools/nuxmv/index.php) model checker in version 2.0.0.
- [AIGER tools](http://fmv.jku.at/aiger/) with `smvtoaig` binary in version 1.9.4 or higher.

The [install dependencies script](scripts/install_dependencies.sh) can be
used to install all these dependencies, but may need to be adapted for other systems.
