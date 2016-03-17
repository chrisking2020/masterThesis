# Copyright (C) Eta Scale AB. Licensed under the DAEDAL Open Source License. See the LICENSE file for details.

add_library(RemoveRedundantPref MODULE RemoveRedundantPref.cpp)

get_property(MODULE_FILE TARGET RemoveRedundantPref PROPERTY LOCATION)

