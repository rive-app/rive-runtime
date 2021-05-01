# rive-cpp
C++ runtime for Rive

## Build
In the ```rive``` directory, run ```build.sh``` to debug build and ```build.sh release``` for a release build.

## Testing
In the ```dev``` directory, run ```test.sh``` to compile and execute the tests.

The tests live in ```rive/test```. To add new tests, create a new ```xxx_test.cpp``` file here. The test harness will automatically pick up the new file.

## Memory Checks
Note that if you're on MacOS you'll want to install valgrind, which is somewhat complicated these days. This is the easiest solution (please PR a better one when it becomes available).
```
brew tap LouisBrunner/valgrind
brew install --HEAD LouisBrunner/valgrind/valgrind
```
You can now run the all the tests through valgrind by running ```test.sh memory```.