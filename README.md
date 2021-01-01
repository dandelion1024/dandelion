# Dandelion Http Server

## What is Dandelion?

Dandelion is a tiny http/1.0 server, mainly developed for embedded web system. It supports static resources and CGI.

## Compiling

The build is CMake-based, so you need to install `cmake` first.  
For debian/ubuntu:
```bash
sudo apt install cmake
```

For Redhat/Fedora/CentOS:
```bash
sudo dnf install cmake
```

For Arch Linux:
```bash
sudo pacman -S cmake
```

If the version of cmake is too low, you need to compile newer cmake from source manually.

Then just run current script in the root of project.
```bash
./build.sh
```

or

```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=YES .
```

Then the executable will be in the bin directory. Just run `./dandelion`, the server will start.

## Usage

Make a directory called `www` in the same level directory of the executable file dandelion, to place your static resource files (such as HTML and JavaScript), and then create a `cgi-bin` directory in the same location to place your executable CGI files.

Then just run `./dandelion`. The server will start.

## Tips

--Note: The CGI parsing is not implemented!--

