#!/bin/bash

wget http://www.broadinstitute.org/~carneiro/travis/gcc_4.9.1-1_amd64.deb
sudo apt-get remove cpp libffi-dev
sudo dpkg --install gcc_4.9.1-1_amd64.deb
