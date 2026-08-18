/* header/Vera_Interrupt/APLIC.h */
#pragma once

#include "../Vera_Utils/utils.h"
#include "../Vera_Device_Driver/driver_support.h"



void init_aplic_informations(struct APLIC_base_information* base);


vera_state driver_init_aplic();

