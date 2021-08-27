#ifndef NVM_TRANSACTION_FLASH_WR
#define NVM_TRANSACTION_FLASH_WR

#include <vector>
#include "assert.h"
#include "../nvm_chip/flash_memory/FlashTypes.h"
#include "../nvm_chip/NVM_Types.h"
#include "NVM_Transaction_Flash.h"
#include "NVM_Transaction_Flash_RD.h"
#include "NVM_Transaction_Flash_ER.h"

namespace SSD_Components
{
	enum class WriteExecutionModeType { SIMPLE, COPYBACK };
	class NVM_Transaction_Flash_ER;
	class NVM_Transaction_Flash_RD;
	class NVM_Transaction_Flash_WR : public NVM_Transaction_Flash
	{
	public:

		//FGM - Transaction format: LPA, PPA, No address
		NVM_Transaction_Flash_WR(Transaction_Source_Type source, 
			stream_id_type stream_id,
			unsigned int data_size_in_byte, 
			LPA_type lpa, 
			PPA_type ppa, 
			SSD_Components::User_Request* user_io_request,
			NVM::memory_content_type content,
			NVM_Transaction_Flash_RD* related_read, 
			page_status_type write_sectors_bitmap, 
			data_timestamp_type data_timestamp);

		//FGM - Transaction format: LPA, PPA, Address
		NVM_Transaction_Flash_WR(Transaction_Source_Type source, 
			stream_id_type stream_id,
			unsigned int data_size_in_byte, 
			LPA_type lpa, 
			PPA_type ppa, 
			const NVM::FlashMemory::Physical_Page_Address& address, 
			SSD_Components::User_Request* user_io_request, 
			NVM::memory_content_type content,
			NVM_Transaction_Flash_RD* related_read, 
			page_status_type write_sectors_bitmap, 
			data_timestamp_type data_timestamp);

		//FGM - Transaction format: LPA, No PPA, No address
		NVM_Transaction_Flash_WR(Transaction_Source_Type source, 
			stream_id_type stream_id,
			unsigned int data_size_in_byte, 
			LPA_type lpa, 
			SSD_Components::User_Request* user_io_request, 
			NVM::memory_content_type content,
			page_status_type write_sectors_bitmap, 
			data_timestamp_type data_timestamp);
	
		NVM::memory_content_type Content; //The content of this transaction
		data_timestamp_type data_timestamp;

		//If this write request must be preceded by a read (for partial page
		//write), this variable is used to point to the corresponding read
		//request
		std::vector<NVM_Transaction_Flash_RD*> RelatedReads; 
		std::vector<data_timestamp_type> DataTimeStamps;
		NVM_Transaction_Flash_ER* RelatedErase;
		page_status_type write_sectors_bitmap;
		WriteExecutionModeType ExecutionMode;


		//FGM - Utility functions to support multiple data timestamps
		unsigned int num_data_timestamps ();
		data_timestamp_type get_data_timestamp ();
		data_timestamp_type get_data_timestamp (unsigned int idx);
		void set_data_timestamp (data_timestamp_type data_timestamp);
		void set_data_timestamp (data_timestamp_type data_timestamp, unsigned int idx);

		//FGM - Utility functions to support multiple related reads
		unsigned int num_related_reads();
		NVM_Transaction_Flash_RD* get_related_read();
		NVM_Transaction_Flash_RD* get_related_read(unsigned int idx);
		void set_related_read (NVM_Transaction_Flash_RD* related_read);
		void set_related_read (NVM_Transaction_Flash_RD* related_read, unsigned int idx);

		//FGM - Utility functions to support dynamic bitmaps
		page_status_type get_sector_bitmap();
		page_status_type get_sector_bitmap(unsigned int idx, unsigned int sectors_per_mu);
		void set_sector_bitmap(page_status_type new_bitmap);
		void set_sector_bitmap(page_status_type new_bitmap, unsigned int idx, unsigned int sectors_per_mu);

		//FGM - Update transaction based on the mapping units per page 
		void update_transaction (
			unsigned int data_size_in_byte,
			LPA_type lpa,
			PPA_type ppa,
			unsigned int sectors_per_mu,
			page_status_type sector_bitmap,
			data_timestamp_type data_timestamp,
			NVM_Transaction_Flash_RD* related_read);
		
	};
		
}

#endif // !WRITE_TRANSACTION_H
