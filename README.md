# Iota Wallet Implementation 

[TOC]

Implements  an object that takes care of tracking all the assets linked to an account on the IOTA protocol.


## Installing the library 

### From source code
```
git clone https://github.com/EddyTheCo/qWallet-IOTA.git 

mkdir build
cd build
qt-cmake -G Ninja -DCMAKE_INSTALL_PREFIX=installDir -DCMAKE_BUILD_TYPE=Release -DUSE_THREADS=ON -DBUILD_EXAMPLES=OFF -DQTDEPLOY=OFF -DUSE_QML=OFF -DBUILD_DOCS=OFF ../qWallet-IOTA

cmake --build . 

cmake --install . 
```
where `installDir` is the installation path, `QTDEPLOY` install the examples and Qt dependencies using the 
[cmake-deployment-api](https://www.qt.io/blog/cmake-deployment-api). Setting the `USE_QML` variable produce or not the QML module.
One can choose to build or not the examples and the documentation with the `BUILD_EXAMPLES` and `BUILD_DOCS` variables.
The use or not of multithreading is set by the `USE_THREADS` variable.

### From GitHub releases
Download the releases from this repo. 

## Adding the libraries to your CMake project 

```CMake
include(FetchContent)
FetchContent_Declare(
	IotaWallet	
	GIT_REPOSITORY https://github.com/EddyTheCo/qWallet-IOTA.git
	GIT_TAG vMAJOR.MINOR.PATCH 
	FIND_PACKAGE_ARGS MAJOR.MINOR CONFIG  
	)
FetchContent_MakeAvailable(IotaWallet)

target_link_libraries(<target> <PRIVATE|PUBLIC|INTERFACE> IotaWallet::wallet)
```
If want to use the QML module also add
```
$<$<STREQUAL:$<TARGET_PROPERTY:IotaWallet::wallet,TYPE>,STATIC_LIBRARY>:IotaWallet::walletplugin>
```


## API reference

You can read the [API reference](https://eddytheco.github.io/Qwallet-IOTA/) here, or generate it yourself like
```
cmake -DBUILD_DOCS=ON ../
cmake --build . --target doxygen_docs
```

## Contributing

We appreciate any contribution!


You can open an issue or request a feature.
You can open a PR to the `develop` branch and the CI/CD will take care of the rest.
Make sure to acknowledge your work, and ideas when contributing.



