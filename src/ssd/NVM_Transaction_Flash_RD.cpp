#include "NVM_Transaction_Flash_RD.h"
#include "../nvm_chip/NVM_Types.h"

namespace SSD_Components
{
	//FGM - LPA, PPA, No Address, Related Write
	NVM_Transaction_Flash_RD::NVM_Transaction_Flash_RD(
			Transaction_Source_Type source, 
			stream_id_type stream_id,
			unsigned int data_size_in_byte, 
			LPA_type lpa,
			PPA_type ppa,
			SSD_Components::User_Request* related_user_IO_request,
			NVM::memory_content_type content, 
			NVM_Transaction_Flash_WR* related_write, 
			page_status_type read_sectors_bitmap, 
			data_timestamp_type data_timestamp) :
		NVM_Transaction_Flash(
				source, 
				Transaction_Type::READ, 
				stream_id, 
				data_size_in_byte, 
				lpa, 
				ppa, 
				related_user_IO_request),
		Content(content),  
		read_sectors_bitmap(read_sectors_bitmap)
	{
		RelatedWrites.push_back(related_write);
		DataTimeStamps.push_back (data_timestamp);
	}

	//FGM - LPA, PPA, Address, No Related Write, Related Erase
	NVM_Transaction_Flash_RD::NVM_Transaction_Flash_RD(
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
			data_timestamp_type data_timestamp) :
		NVM_Transaction_Flash(
				source, 
				Transaction_Type::READ, 
				stream_id, 
				data_size_in_byte, 
				lpa, 
				ppa, 
				address, 
				related_user_IO_request),
		Content(content), 
		RelatedErase(related_erase),
		read_sectors_bitmap(read_sectors_bitmap)
	{
		DataTimeStamps.push_back (data_timestamp);
	}

	//FGM - LPA, PPA, No Adderss, No Related Write, No Related Erase
	NVM_Transaction_Flash_RD::NVM_Transaction_Flash_RD(
			Transaction_Source_Type source, 
			stream_id_type stream_id,
			unsigned int data_size_in_byte, 
			LPA_type lpa,
			PPA_type ppa,
			SSD_Components::User_Request* related_user_IO_request,
			NVM::memory_content_type content, 
			page_status_type read_sectors_bitmap, 
			data_timestamp_type data_timestamp) :
		NVM_Transaction_Flash(
				source, 
				Transaction_Type::READ, 
				stream_id, 
				data_size_in_byte, 
				lpa, 
				ppa, 
				related_user_IO_request),
		Content(content), 
		read_sectors_bitmap(read_sectors_bitmap)
	{
		RelatedWrites.push_back(NULL);
		DataTimeStamps.push_back (data_timestamp);
	}

	//FGM - Data timestamps
	unsigned int NVM_Transaction_Flash_RD ::num_data_timestamps ()
	{
		return DataTimeStamps.size ();
	}
	data_timestamp_type NVM_Transaction_Flash_RD ::get_data_timestamp ()
	{
		if (!DataTimeStamps.empty())
		return DataTimeStamps[0];
	}
	data_timestamp_type NVM_Transaction_Flash_RD ::get_data_timestamp (unsigned int idx)
	{
		assert(DataTimeStamps.size() >= idx);
		return DataTimeStamps[idx];
	}
	void NVM_Transaction_Flash_RD ::set_data_timestamp (data_timestamp_type data_timestamp)
	{
		if (!DataTimeStamps.empty()) DataTimeStamps.push_back (data_timestamp);
		else DataTimeStamps[0] = data_timestamp;
	}
	void NVM_Transaction_Flash_RD ::set_data_timestamp (data_timestamp_type data_timestamp, unsigned int idx)
	{
		assert (DataTimeStamps.size() >= idx);
		DataTimeStamps[idx] = data_timestamp;
	}

	//FGM - Related writes 
	unsigned int NVM_Transaction_Flash_RD:: num_related_writes()
	{
		return RelatedWrites.size();
	}	
	
	NVM_Transaction_Flash_WR* NVM_Transaction_Flash_RD:: get_related_write()
	{
		if (!RelatedWrites.empty())
		return RelatedWrites[0];
	}

	NVM_Transaction_Flash_WR* NVM_Transaction_Flash_RD:: get_related_write (unsigned int idx)
	{
		assert (RelatedWrites.size() >= idx);
		return RelatedWrites[idx];
	}

	void NVM_Transaction_Flash_RD:: set_related_write (NVM_Transaction_Flash_WR* write_tr)
	{

		RelatedWrites[0] = write_tr;
	}

	void NVM_Transaction_Flash_RD:: set_related_write (NVM_Transaction_Flash_WR* write_tr, unsigned int idx)
	{
		assert (RelatedWrites.size() >= idx);
		RelatedWrites[idx] = write_tr;
	}
				
	//FGM - Handle write sectors bitmap  
	page_status_type NVM_Transaction_Flash_RD:: get_sector_bitmap ()
	{
		return read_sectors_bitmap;
	}
	page_status_type NVM_Transaction_Flash_RD:: get_sector_bitmap (
			unsigned int idx, unsigned int sectors_per_mu)
	{	
		assert ((this->num_lpas() >= idx)); //not sure about the assertion 
		return (
				(read_sectors_bitmap >> ((this->num_lpas () - idx - 1) * sectors_per_mu)) 
			& (~(0xffffffffffffffff << sectors_per_mu)));
	}
	void NVM_Transaction_Flash_RD:: set_sector_bitmap (page_status_type new_bitmap)
	{
		read_sectors_bitmap = new_bitmap;
	}

	//FGM - Update transaction
	void NVM_Transaction_Flash_RD :: update_transaction (
		unsigned int data_size_in_byte,
		LPA_type lpa,
		PPA_type ppa,
		unsigned int sectors_per_mu,
		page_status_type sector_bitmap,
		data_timestamp_type data_timestamp,
		NVM_Transaction_Flash_WR* related_write)
		{	
			Data_and_metadata_size_in_byte =+ data_size_in_byte;
			LPAs.push_back (lpa);
			PPAs.push_back (ppa);
			read_sectors_bitmap <<= sectors_per_mu;
			read_sectors_bitmap |= sector_bitmap;
			DataTimeStamps.push_back (data_timestamp);	
			RelatedWrites.push_back (related_write);
		}
}
																																
