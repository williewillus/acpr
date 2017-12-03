CS380L Advanced Operating Systems Final Project
Fall 2017
Wally Guzman
Vincent Lee 

Requirements
============
- Boost Filesystem and System libraries, version 1.58 or higher
- Python 3.5 or higher
- stress-ng, if using the -s option of the test script
- sudo access

Building
========
- Create build/ directory in root
- Within build/, execute `cmake ..`
- Within build/, execute `make`
- The binary will be output in `build/acpr`

Testing
=======
- Within test/, execute `chmod +x acpr_test.py`
- Within test/, run `sudo ./acpr_test.py [options] src_dir dest_dir`

Cleaning
========
- Simply delete build/

