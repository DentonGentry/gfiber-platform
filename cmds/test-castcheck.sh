#!/bin/bash
. ./wvtest/wvtest.sh

WVSTART "castcheck test"
CASTCHECK="./castcheck -a ./avahi-browse-fake.sh"

WVPASSEQ "$($CASTCHECK)" "Cast responses from: 1.1.1.1 2.2.2.2 3.3.3.3"
