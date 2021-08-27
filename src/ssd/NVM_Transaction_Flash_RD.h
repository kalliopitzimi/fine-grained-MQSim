#ifndef NVM_TRANSACTION_FLASH_RD_H
#define NVM_TRANSACTION_FLASH_RD_H
#include <vector>
#include "assert.h"
#include "../nvm_chip/flash_memory/FlashTypes.h"
#include "NVM_Transaction_Flash.h"
#include "NVM_Transaction_Flash_ER.h"
#include "NVM_Transaction_Flash_WR.h"


namespace SSD_Components
{
	class NVM_Transaction_Flash_WR;
	class NVM_Transaction_Flash_ER;
	class NVM_Transaction_Flash_RD : public NVM_Transaction_Flash
	{
	public:
		//FGM - LPA, PPA, No Address, Related Write
		NVM_Transaction_Flash_RD(
				Transaction_Source_Type source, 
				stream_id_type stream_id,
				unsigned int data_size_in_byte, 
				LPA_type lpa,
				PPA_type ppa,
				SSD_Components::User_Request* related_user_IO_request, 
				NVM::memory_content_type content, 
				NVM_Transaction_Flash_WR* related_write,
				page_status_type read_sectors_bitmap, 
				data_timestamp_type data_timestamp);

		//FGM - LPA, PPA, Address, No Related Write, Related Erase
		NVM_Transaction_Flash_RD(
				Transaction_Source_Type source, 
				stream_id_type stream_id,
				unsigned int data_size_in_byte, 
				LPA_type lpa,
				PPA_type ppa,
				const NVM::FlashMemory::Physical_Page_Address& address,
				SSD_Components::User_Request* related_user_IO_request, 
				NVM::memory_content_type content, 
				NVM_Transaction_Flash_ER* related_erase,
				page_status_type read_sectors_bitmap, 
				data_timestamp_type data_timestamp);

		//FGM - LPA, PPA, No Adderss, No Related Write, No Related Erase
		NVM_Transaction_Flash_RD(
				Transaction_Source_Type source, 
				stream_id_type stream_id,
				unsigned int data_size_in_byte, 
				LPA_type lpa,
				PPA_type ppa,
				SSD_Components::User_Request* related_user_IO_request, 
				NVM::memory_content_type content,
				page_status_type read_sectors_bitmap, 
				data_timestamp_type data_timestamp);


		NVM::memory_content_type Content; //The content of this transaction	
		page_status_type read_sectors_bitmap;

		std::vector<data_timestamp_type> DataTimeStamps;
		std::vector<NVM_Transaction_Flash_WR*> RelatedWrites;//Is this read request related to another write request and provides update data (for partial page write)

		//FGM - Added Related Erases to GC RD Transactions
		NVM_Transaction_Flash_ER* RelatedErase;
		NVM_Transaction_Flash_WR* RelatedWrite;
		data_timestamp_type DataTimeStamp;
	
		//FGM - Utility functions to support multiple data timestamps
		unsigned int num_data_timestamps ();
		data_timestamp_type get_data_timestamp ();
		data_timestamp_type get_data_timestamp (unsigned int idx);
		void set_data_timestamp (data_timestamp_type data_timestamp);
		void set_data_timestamp (data_timestamp_type data_timestamp, unsigned int idx);

		//FGM - Utility functions to support multiple related writes
		unsigned int num_related_writes();
		NVM_Transaction_Flash_WR* get_related_write();
		NVM_Transaction_Flash_WR* get_related_write(unsigned int idx);
		void set_related_write( NVM_Transaction_Flash_WR* write_tr);
		void set_related_write( NVM_Transaction_Flash_WR* write_tr, unsigned int idx);

		//FGM - Utility functions to support dynamic bitmaps
		page_status_type get_sector_bitmap();
		page_status_type get_sector_bitmap(unsigned int idx, unsigned int sectors_per_mu);
		void set_sector_bitmap(page_status_type new_bitmap);
		void set_sector_bitmap(page_status_type new_bitmap, unsigned int idx, unsigned int sectors_per_mu);

		//FGM - Update transaction based on the mapping units per page 
		void update_transaction(
			unsigned int data_size_in_byte,
			LPA_type lpa,
			PPA_type ppa,
			unsigned int sectors_per_mu,
			page_status_type sector_bitmap,
			data_timestamp_type data_timestamp,
			NVM_Transaction_Flash_WR* related_write);
	};
}

#endif
