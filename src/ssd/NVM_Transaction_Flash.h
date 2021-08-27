#ifndef NVM_TRANSACTION_FLASH_H
#define NVM_TRANSACTION_FLASH_H

#include <string>
#include <list>
#include <cstdint>
#include "../sim/Sim_Defs.h"
#include "../sim/Sim_Event.h"
#include "../sim/Engine.h"
#include "../nvm_chip/flash_memory/FlashTypes.h"
#include "../nvm_chip/flash_memory/Flash_Chip.h"
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "NVM_Transaction.h"
#include "User_Request.h"

namespace SSD_Components
{
	class User_Request;
	class NVM_Transaction_Flash : public NVM_Transaction
	{
	public:

		NVM_Transaction_Flash(
				Transaction_Source_Type source, 
				Transaction_Type type, 
				stream_id_type stream_id,
				unsigned int data_size_in_byte, 
				LPA_type lpa,
				PPA_type ppa,
				User_Request* user_request);


		NVM_Transaction_Flash(
				Transaction_Source_Type source, 
				Transaction_Type type, 
				stream_id_type stream_id,
				unsigned int data_size_in_byte, 
				LPA_type lpa,
				PPA_type ppa,
				const NVM::FlashMemory::Physical_Page_Address& address, 
				User_Request* user_request);
		
		//FGM - number of bytes contained in the request = bytes in the real page + bytes of metadata
		unsigned int Data_and_metadata_size_in_byte; 
	
		//FGM - based on device page size, i.e., physical page number at the chip level
		NVM::FlashMemory::Physical_Page_Address Address; 
	
		bool SuspendRequired;
		bool Physical_address_determined;
		sim_time_type Estimated_alone_waiting_time;
		//Used in scheduling methods, such as FLIN, where fairness and QoS is
		//considered in scheduling
		bool FLIN_Barrier;

		//FGM - Dynamic structure definition
		std::vector<LPA_type> LPAs;
		std::vector<PPA_type> PPAs; 

		//FGM - Utility functions that support multiple LPAs
		unsigned int num_lpas ();
		LPA_type get_lpa ();
		LPA_type get_lpa (int idx);
		void set_lpa (LPA_type lpa);
		void set_lpa (LPA_type lpa, int idx);

		//FGM - Utility functions that support multiple PPAs
		unsigned int num_ppas ();
		PPA_type get_ppa ();
		PPA_type get_ppa (int idx);
		void set_ppa (PPA_type ppa);
		void set_ppa (PPA_type ppa, int idx);

		//FGM - Utility functions that support the multiple LPAs during GC activities
		void replace_lpa (LPA_type lpa, int idx, int i);
		void set_waiting_lpas (LPA_type lpa);
		LPA_type get_waiting_lpas (int idx);	

		//FGM - The correct LPA is in the following vector, and will be assigned in the GC Base class.
		std::vector<LPA_type> Waiting_LPAs; 
	};
}

#endif // !FLASH_TRANSACTION_H
