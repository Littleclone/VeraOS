/* header/Vera_Interrupt/IMSIC.h */
#pragma once

#include "../Vera_Utils/utils.h"
#include "../Vera_Device_Driver/driver_support.h"


void IMSIC_init_informations(struct IMSICS_base_information* base_config);

vera_state driver_initialise_IMSIC();


