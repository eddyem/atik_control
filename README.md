# atik_control
The same as fli_control but for Atik cameras

Simple CLI tool to work with Atik CCD cameras: cooling, expositions, save FITS.


## Installation
1. Download and install latest Atik development files.
2. mkdir mk && cd mk && cmake .. && make && su -c "make install".
3. You also can save files into RAW or PNG formats:
	* -DUSE_PNG=yes - use png output
	* -DUSE_RAW=yes - use raw output
4. Option -DUSE_BTA=yes will add BTA information in FITS-header

