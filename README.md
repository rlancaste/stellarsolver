# StellarSolver
## The Cross Platform SEP-based Star Extractor and Astrometry.net-Based Internal Astrometric Solver

![StellarSolver Pretty](/images/StellarSolverPretty.png "StellarSolver solving a pretty image.")

 - An Astrometric Plate Solver for Mac, Linux, and Windows, built on Astrometry.net and SEP (SExtractor)
 - Meant to be an internal library for use in a program like KStars for internal plate solving on all supported operating systems
 - Python is not required for the library
 - Netpbm is not required for the library
 - Internal Library, so calls to external programs are not required
 - No Astrometry.cfg file is needed, the settings are internal to the program
 - Directly loads the image data into SEP and then passes the generated xy-list into astrometry.net, so there is no need to save any files.
 - No temporary files need to be created for solving and no WCS file needs to be created to read the solved information.
 - The Index Files are still required for solving images, but the program or the user can specify the folder locations rather than putting them in the config file.
 - Note: The executable created by MainWindow is only meant for testing purposes.  It is for developing and improving StellarSolver.cpp and the included libraries in the astrometry and sep folders.  It can be used to compare the library to an existing installation of astrometry.net on the computer to perfect the settings.

![StellarSolver In Action](/images/StellarSolverInAction.png "StellarSolver running the same solve on different platforms.")
## Based on:
 - Astrometry.net [Astrometry README](http://astrometry.net/doc/readme.html)
 - SExtractor [Sextractor Documentation](https://sextractor.readthedocs.io/en/latest/)
 - SEP (SExtractor) [SEP Documentation](https://sep.readthedocs.io/en/v1.0.x/api/sep.extract.html)
## Designed for:
 - KStars [KStars Documentation](https://edu.kde.org/kstars/)
 - Any other Cross Platform Astronomical Program based on C++ and QT
## The History
Astrometry.net is a fantastic astrometric plate solver, but it is command line only and has many dependencies such as python, netpbm, libjpeg, cfitsio, and many other things.  It is fairly easy to install on Linux and works incredibly well in that evironment.  With the correct recipes in homebrew, craft, macports, or other package managers, it can be installed and run from the command line on Mac OS X as well.  On Windows, however, it must be run within a compatibility layer such as Microsoft Subsystem for Linux, Cygwin, or ANSVR.  None of these things will stop the program from running, but it does make it more difficult to set up and use and all of the files and dependencies and configuration files must be setup properly in order to get it to work.

I have been working for the last several years with the Open Source KStars and INDI projects and we have been calling on astrometry.net as an external program using QProcess on Linux for a number of years.  When I and a couple of others ported KStars to Mac OS X a couple of years ago, I spent a lot of time getting astrometry.net built and set up within the app bundle along with its dependencies so that users would have a much easier time getting KStars working and wouldn't have to spend lots of time trying to install Homebrew and dependencies to get it all working.  Unfortunately, we have had some issues over the years with certain Macs running the code and also with python configurations on some people's computers.  It worked for the majority of users, but there were some issues.  In February of 2020, in response to those issues, I came up with another plan based on information on Astrometry.net's website that if SExtractor was used on the data first, we could avoid using python and netpbm.  This worked very well and in fact, the process of using SExtractor first and then Astrometry.net second seemed to give us a boost in speed.  

## Goal 1: Test and Perfect Settings in Astrometry and SExtractor to make it more efficient.
After we released the new version KStars and many people reported that it sped up plate solving and the people who were having problems said that it fixed their problems, I decided that it might be a good idea to setup a separate program as a playground to see if I could perfect some of the parameters for SExtractor and Astrometry to make them more efficient and speed it up even more.  So that is why I created this repository.  I copied in code from KStars to support loading images, display images, and star extracting images since that work was already done.  Then I worked to make as many of the parameters adjustable as possible and made it display the results along with the parameters and the times in a table.  This way we should be able to perfect all of the settings.

## Goal 2: Access Astrometry.net and SEP as an internal library rather than external processes.
Another goal that I had in mind when I created this repository was that we could test my idea of accessing astrometry.net using it as a library rather than the external programs being called like it works in KStars now.  At first, I was planning to use the dynamic libraries that get built with astrometry.net, which ended up working pretty well, but then I abanonded that idea because then the project would depend on having the development version of the libastrometry package installed.  Plus, building the program internally would allow me to leave out parts that we don't need and to fix any issues with running it on other operating systems.

## Goal 3: Making the Internal Solver Library work on Windows
Another thought I had in the back of my mind was that there were a copule of reasons that astrometry.net has trouble on windows.  The first is the fact that it is commmand line.  The second is all of the linux dependencies and POSIX requirements.  I was thinking that since we already have an internal library version of SEP that works on windows and since we would be running this as a library rather than on the command line, possibly I would be able to remove a number of the parts of astrometry.net that prevent it from running on windows and that I would be able to port the rest of the requirements to make it work.

## Results:
 - Goal 1 was partially completed when I got the program set up and ready for testing in February 2020.  Now testing should continue so that we can see what parameters are needed, what makes it more efficient, and how to make it better.
 - Goal 2 was completed in March 2020, but some changes might yet be made.  Astrometry and SExtractor are now fully integraded for Linux and Mac Computers and no external configuration files are needed anymore.  Netpbm and Python are not required either.
 - Goal 3 is mostly completed by April 2020.  by the end of March 2020, I had it compiling on Windows at least with the MinGW compiler.  As of April 2nd, I have it compiling in the MSVC compiler.  And on Windows it is solving images successfully just like on Mac and Linux.
 
# Installing the program

## Linux
 - Download the stellarsolver-tester git repository in a terminal window or download it in your browser
 
 		git clone https://github.com/rlancaste/stellarsolver.git
 		
 - Run the installLinux.sh script
 
 		./installLinux.sh
 		
 - It will build and install the program and create a shortcut on the desktop
 
## Mac
 - Download the latest release DMG
 - Double click the DMG and drag the icon to your /Applications folder
 - Right click the icon in your /Applications Folder and select "open"

## Windows
 - Download the lastest release exe installer
 - Double click the exe and follow the dialogs to install the program
 - Go to your Start Menu and Open the program

# What else do I need to install
One of the big goals of the StellarSolver is to eliminate all the external programs, configuration files, and setup currently required by programs like 
Astrometry.net.  But no matter what, you will still need some index files which contain the star positions astrometry will match your stars with to get a solve.
That being said, it would really help for the testing purposes to install the other methods of plate solving for comparison purposes.  Also note that another
goal of this project is to try to improve the parameters and the way that we use the external programs as well for an alternative plate solving method.

## Index Files
Anybody who has used Astrometry.net is familiar with index files.  If you don't have them, or need to know more about them, you can read up on them here: 
[Astrometry Readme document](http://astrometry.net/doc/readme.html).  You will need to download the ones that are relevant to your images.  This is mainly
determined by the field size of your images.  You can read all about that on the website.  This program can work with the index files in whatever folder you
put them, however the default locations it searches for the index files are the default locations on the operating systems astrometry.net looks for the files in.
So that is where I would put them.  On Windows, it looks in the ANSVR location by default, but can also do the cygwin location.  On Mac, it can either default
to the homebrew index file location or if you have KStars installed, it can use the KStars default location instead.  On Linux, it can either do the KStars
"local" index file location or the Linux default location.

## Alternate programs/methods for solving
This program has several methods of plate solving images, one is using the internal StellarSolver Library, but the other methods all rely on external programs.
You don't have to install any of them, but sometimes the other methods are better for certain images and we would like to compare the methods and improve all
of them.  The goal is not just to replace all of these other ways of plate solving with StellarSolver, but to provide alternatives.  To get them all set up

- Astrometry - [http://astrometry.net/doc/readme.html](http://astrometry.net/doc/readme.html)
- ANSVR (Astrometry on Windows) [https://adgsoftware.com/ansvr/](https://adgsoftware.com/ansvr/)
- ASTAP - [http://www.hnsky.org/astap.htm](http://www.hnsky.org/astap.htm)
- Watney Astrometric Solver [https://github.com/Jusas/WatneyAstrometry](https://github.com/Jusas/WatneyAstrometry)

- SExtractor is a little more difficult.  It can't be installed on Windows to my knowledge.  The installation of astrometry.net on ubuntu installs 
SExtractor as well so it is taken care of.  And on Macs, you can install it with astrometry.net in homebrew.

# Using the StellarSolver Tester Program
Remember that the main goal of this StellarSolver tester program is to perfect the internal libraries, determine what settings need to be avaiable for the user to use,
determine what settings should be set to fixed values and hidden from the user, and to figure out what effect some settings have on the program.  With that goal in mind,
please test plate solving all kinds of images on different systems.  Try star extracting all kinds of images on different systems.  Compare the results in the results table
and in the star table.  Find out what works and what does not work.  Send feedback!

## Star Extracting Images
You can either use the Internal SEP or the External SExtractor (Assuming you have one installed, I'm not sure it can be installed on Windows.)
We would like to perfect the Internal SEP so that it works the best, but we also need to perfect the settings for the external one.
There are many settings for the Star Extractor in the left panel of options, please play with them, find out what works best, and see how we can make it better.
When you do the star extraction, the program will load the results into the star table at the right and the stars will get circles around them in the image.
We want the Star Extractor to be fairly fast, accurately detect stars (or other objects) for various purposes, and report things like Magnitude and Flux.
One goal is to use the extracted stars to solve images, the other is to use the extracted stars for other reasons like guiding and photometry.

![StellarSolver Star Extractor](/images/Sextractor.png "StellarSolver extracting stars into the star table.")

## Solving Images
You can use the Internal Solver or the External Solver(s).  There are numerous options like using internal SEP and external astrometry.net, or using ASTAP to solve images.
We want to support various methods of solving images because different systems work better for different people or goals.  The Internal Solver is the ultimate goal of this repo, but the others are important too.
There are a number of settings in the left panel of options that you can set for solving images.  We want to solve images quickly but accurately.
So please play around with the settings and find out what can work the best.

![StellarSolver Solver](/images/Solver.png "StellarSolver solving an image using different methods.")

# Building the program

## Linux
You can follow this set of steps on ubuntu build the program StellarSolver on Linux if you don't want to use the installer above.

	sudo apt -y install git cmake qtbase5-dev libcfitsio-dev libgsl-dev wcslib-dev
	git clone https://github.com/rlancaste/stellarsolver.git
	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTER=ON ../
	make -j $(expr $(nproc) + 2)
	sudo make install


## Mac
You should probably use craft to get it set up on Mac.  You don't need to do so, but it would be easiest
since there are dependencies like cfitsio which are more challenging to install without using craft.
You can set it up using:
 [Mac Craft Installation Instructions](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Mac#Installation_using_Craft)
Then just type:

	craft -vi StellarSolverTester

and it will build.

## Windows
You should also use craft to set it up on Windows.  On Windows, right now you have to use craft to get gsl, cfitsio, QT,
and everything else all set up.  I recommend Visual Studio and the MSVC compiler.
You can set it up using:
 [Windows Craft Installation Instructions](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Windows)
Then just type:

	craft -vi StellarSolver

and it will build.
