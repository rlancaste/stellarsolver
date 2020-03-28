# SexySolver
## The Cross Platform Sextractor and Astrometry.net-Based Internal Astrometric Solver

 - An Astrometric Plate Solver for Mac, Linux, and hopefully Windows
 - Python is not required
 - Netpbm is not required
 - Internal Library, so external method calls not required
 - No Astrometry.cfg file is needed
 - The Index Files are still required for solving images, but the program or the user can specify the folder locations rather than putting them in the config file.
 - Note: The SexySolver executable created by MainWindow is only meant for testing purposes.  It is for developing and improving SexySolver.cpp and the included libraries in the astrometry and sep folders

![SexySolver In Action](/SexySolverInAction.png "SexySolver running the same solve on different platforms.")

## The History
Astrometry.net is a fantastic astrometric plate solver, but it is command line only and has many dependencies such as python, netpbm, libjpeg, cfitsio, and many other things.  It is fairly easy to install on Linux and works incredibly well in that evironment.  With the correct recipes in homebrew, craft, macports, or other package managers, it can be installed and run from the command line on Mac OS X as well.  On Windows, however, it must be run within a compatibility layer such as Microsoft Subsystem for Linux, Cygwin, or Ansvr.  None of these things will stop the program from running, but it does make it more difficult to set up and use and all of the files and dependencies and configuration files must be setup properly in order to get it to work.

I have been working for the last several years with the Open Source KStars and INDI projects and we have been calling on astrometry.net as an external program using QProcess on Linux for a number of years.  When I and a couple of others ported KStars to Mac OS X a couple of years ago, I spent a lot of time getting astrometry.net built and set up within the app bundle along with its dependencies so that users would have a much easier time getting KStars working and wouldn't have to spend lots of time trying to install Homebrew and dependencies to get it all working.  Unfortunately, we have had some issues over the years with certain Macs running the code and also with python configurations on some people's computers.  It worked for the majority of users, but there were some issues.  In February of 2020, in response to those issues, I came up with another plan based on information on Astrometry.net's website that if Sextractor was used on the data first, we could avoid using python and netpbm.  This worked very well and in fact, the process of using Sextractor first and then Astrometry.net second seemed to give us a boost in speed.  

## Goal 1: Test and Perfect Settings in Astrometry and Sextractor to make it more efficient.
After we released the new version KStars and many people reported that it sped up plate solving and the people who were having problems said that it fixed their problems, I decided that it might be a good idea to setup a separate program as a playground to see if I could perfect some of the parameters for Sextractor and Astrometry to make them more efficient and speed it up even more.  So that is why I created this repository.  I copied in code from KStars to support loading images, display images, and sextracting images since that work was already done.  Then I worked to make as many of the parameters adjustable as possible and made it display the results along with the parameters and the times in a table.  This way we should be able to perfect all of the settings.

## Goal 2: Access Astrometry.net and Sextractor as an internal library rather than external processes.
Another goal that I had in mind when I created this repository was that we could test my idea of accessing astrometry.net using it as a library rather than the external programs being called like it works in KStars now.  At first, I was planning to use the dynamic libraries that get built with astrometry.net, which ended up working pretty well, but then I abanonded that idea because then the project would depend on having the development version of the libastrometry package installed.  Plus, building the program internally would allow me to leave out parts that we don't need and to fix any issues with running it on other operating systems.

## Goal 3: Making the Internal Solver Library work on Windows
Another thought I had in the back of my mind was that there were a copule of reasons that astrometry.net has trouble on windows.  The first is the fact that it is commmand line.  The second is all of the linux dependencies and POSIX requirements.  I was thinking that since we already have an internal library version of Sextractor that works on windows and since we would be running this as a library rather than on the command line, possibly I would be able to remove a number of the parts of astrometry.net that prevent it from running on windows and that I would be able to port the rest of the requirements to make it work.

## Results:
 - Goal 1 was partially completed when I got the program set up and ready for testing in February 2020.  Now testing should continue so that we can see what parameters are needed, what makes it more efficient, and how to make it better.
 - Goal 2 was completed in March 2020, but some changes might yet be made.  Astrometry and Sextractor are now fully integraded for Linux and Mac Computers and no external configuration files are needed anymore.  Netpbm and Python are not required either.
 - Goal 3 is ongoing.  by the end of March 2020, I do have it compiling on Windows at least with the MinGW compiler.  And on Windows it is solving images successfully as long as you specify the scale and a starting RA/Dec for the search.
 
# Setting up the program for testing.

## Linux
You can follow this set of steps on ubuntu to get the program SexySolver set up to run.

	sudo apt -y install git cmake qt5-default libcfitsio-dev libgsl-dev
	git clone https://github.com/rlancaste/sexysolver-tester.git
	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo ../sexysolver-tester/
	make -j $(expr $(nproc) + 2)
	make install


## Mac
You should probably use craft to get it set up on Mac.  You don't need to do so, but it would be easiest
since there are dependencies like cfitsio which are more challenging to install without using craft.
You can set it up using:
 [Mac Craft Installation Instructions](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Mac#Installation_using_Craft)
Once you set up craft, just copy the SexySolver recipe in the craft-blueprint folder of this repo into the folder
/etc/blueprints/locations/craft-blueprints-kde/libs of your root craft directory.  Then just type:

	craft -v -i SexySolver

and it will install

## Windows
The Windows build is still very much experimental.  On Windows, right now you have to use craft to get gsl, cfitsio, QT,
and everything else all set up, and you have to use the MingGW compiler.  It won't build yet using MSVC and Visual Studio.
You can set it up using:
 [Windows Craft Installation Instructions](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Windows)
Once you set up craft, just copy the SexySolver recipe in the craft-blueprint folder of this repo into the folder
/etc/blueprints/locations/craft-blueprints-kde/libs of your root craft directory.  Then just type:

	craft -v -i SexySolver

and it will install.  If there is a build problem due to boost-regex, please edit the CMakelists.txt file to point to the boost-regex 
library.  There are instructions in the file.  I will fix this in the future.