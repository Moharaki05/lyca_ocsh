# OCSH by lyca

A simple shell competitor to bash and zsh using C

# Preview:


# Directory Structure:

```
code
├── bin
├── include
└── src

4 directories
```

# Dependencies:

- Building the shell may need the installation of the GNU C compiler itself ``gcc`` or a LLVM C compiler like ``clang``, you may also need GNU Make ``make``, if you're willing to use it in "Building" section , which is really recommended cause it maintain groups of programs instructions:

  - `gcc` or `clang`
  - `make`
  - `git`

## Installation of Dependencies:

- In a UNIX-like system like Linux, the installation varies depending on the linux distribution:
    
    - Debian/Ubuntu:
    
    ```bash
    sudo apt update -y
    sudo apt install build-essential git -y
    ```
    - Fedora/CentOS/RHEL:
    
    ```bash
    sudo dnf check-update
    sudo dnf group install c-development -y
    sudo dnf install git -y
    ```
    - Arch Linux:
    
    ```bash
    sudo pacman -Syu
    sudo pacman -S --needed base-devel git 
    ```  

# Building Process:

- To build the ``ocsh``, we have 2 approaches, weither to compile it manually using the GNU C compiler or by giving the whole job to GNU Make for (compiling,assembling,linking,and even installing):
 
    - First , we have to clone the repository from our github repository, assuming we are cloning it in the home directory :
   
    ```bash
    git clone https://github.com/Moharaki05/lyca_ocsh
    cd ~/lyca_ocsh/code
    ```
    - Method 1: Compiling with GNU C compiler:

     ```bash
     cc -O1 -Wall -Wextra -Werror -Wshadow -Iinclude src/*.c -o ocsh
     ```
    - Method 2: Using GNU Make (Really Recommended):
    
    ```bash
    make # Tip: use make run (if you prefer to build it and run it immediately) 
    ```
# Installation:

- To install ``ocsh`` in your Linux machine with GNU Make:
    
    ```bash
    make install # before running it , verify if you're in ~/lyca_ocsh/code where the Makefile is stored
    ```
- After adding it you have to add `~/bin` to `$PATH` on your shell config file (zsh or bash):
    
    ```bash
    echo 'export PATH="$HOME/bin:$PATH"' >> .bashrc # or .zshrc if you're using zsh
    ```

# Uninstalling:

- Simply run:

    ```bash
    rm -f ~/bin/ocsh
    ```

