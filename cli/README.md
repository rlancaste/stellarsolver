# Command Line Interface For StellarSolver

Allows the usage of StellarSolver. The output and parameters are quite similar to astrometry.net solve-field. In many cases it should work as drop in replacement but with better performance.

*Build*

    mkdir build
	cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_CLI=ON ../ && make -j $(expr $(nproc) + 2)

*View Help*

    ./stellarsolver-cli -h

*Index files*
    By default `stellarsolver-cli` expects the index files in `/usr/share/astrometry` but you can also specify another path with the `--index-files` Parameter

*Usage*
Basic Usage:

    stellarsolver-cli --index-files <path_to_index_file> <path_to_picture>