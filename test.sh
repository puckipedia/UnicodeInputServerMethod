#!/bin/sh
cd `dirname $0`
make
rm ~/config/non-packaged/add-ons/input_server/methods/UnicodeInputServerMethod || true
cp objects.*-debug/UnicodeInputServerMethod ~/config/non-packaged/add-ons/input_server/methods
