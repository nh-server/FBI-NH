#!/bin/bash

read -p "Type the IP address of your 3DS: " -e input
python servefiles.py $input .
