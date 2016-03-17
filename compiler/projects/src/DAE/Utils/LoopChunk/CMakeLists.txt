# Copyright (C) Eta Scale AB. Licensed under the DAEDAL Open Source License. See the LICENSE file for details.

add_library(LoopChunk SHARED LoopChunk.cpp
    ${PROJECTS_MAIN_INCLUDE_DIR}/Util/Analysis 
    ${PROJECTS_MAIN_SRC_DIR}/Util/Annotation/MetadataInfo.cpp
    ${PROJECTS_MAIN_INCLUDE_DIR}/Util/Analysis
    )

