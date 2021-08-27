#include <vector>
#include "NVM_Transaction_Flash_WR.h"


namespace SSD_Components
{
	//FGM - Transaction format: LPA, PPA, No address
	NVM_Transaction_Flash_WR::NVM_Transaction_Flash_WR(Transaction_Source_Type source, 
		stream_id_type stream_id,
		unsigned int data_size_in_byte, 
		LPA_type lpa, 
		PPA_type ppa, 
		SSD_Components::User_Request* user_io_request, 
		NVM::memory_content_type content,
		NVM_Transaction_Flash_RD* related_read, 
		page_status_type written_sectors_bitmap, 
		data_timestamp_type data_timestamp) :
		NVM_Transaction_Flash(source, 
			Transaction_Type::WRITE, 
			stream_id, 
			data_size_in_byte, 
			lpa, 
			ppa, 
			user_io_request),
		Content(content), 
		write_sectors_bitmap(written_sectors_bitmap),
		ExecutionMode(WriteExecutionModeType::SIMPLE)
	{
		
		RelatedReads.push_back(related_read);
		DataTimeStamps.push_back(data_timestamp);
	}

	//FGM - Transaction format: LPA, PPA, Address
	NVM_Transaction_Flash_WR::NVM_Transaction_Flash_WR(
		Transaction_Source_Type source, 
		stream_id_type stream_id,
		unsigned int data_size_in_byte, 
		LPA_type lpa, 
		PPA_type ppa, 
		const NVM::FlashMemory::Physical_Page_Address& address, 
		SSD_Components::User_Request* user_io_request, 
		NVM::memory_content_type content,
		NVM_Transaction_Flash_RD* related_read, 
		page_status_type written_sectors_bitmap, 
		data_timestamp_type data_timestamp) :
		NVM_Transaction_Flash(source, 
			Transaction_Type::WRITE, stream_id, 
			data_size_in_byte, 
			lpa, 
			ppa, 
			address, 
			user_io_request),
		Content(content), 
		write_sectors_bitmap(written_sectors_bitmap),
		ExecutionMode(WriteExecutionModeType::SIMPLE)
	{
		
		RelatedReads.push_back(related_read);
		DataTimeStamps.push_back(data_timestamp);	
	}

	//FGM - Transaction format: LPA, No PPA, No address
	NVM_Transaction_Flash_WR::NVM_Transaction_Flash_WR(Transaction_Source_Type source, 
		stream_id_type stream_id,
		unsigned int data_size_in_byte, 
		LPA_type lpa, 
		SSD_Components::User_Request* user_io_request, 
		NVM::memory_content_type content,
		page_status_type written_sectors_bitmap, 
		data_timestamp_type data_timestamp) :
		NVM_Transaction_Flash(source, 
			Transaction_Type::WRITE, 
			stream_id, 
			data_size_in_byte, 
			lpa, 
			NO_PPA, 
			user_io_request),
		Content(content),  
		write_sectors_bitmap(written_sectors_bitmap),
		ExecutionMode(WriteExecutionModeType::SIMPLE)
	{

		RelatedReads.push_back(NULL);
		DataTimeStamps.push_back(data_timestamp);
	}
	
	//FGM - Data timestamps
	unsigned int NVM_Transaction_Flash_WR ::num_data_timestamps ()
	{
		return DataTimeStamps.size ();
	}
	data_timestamp_type NVM_Transaction_Flash_WR ::get_data_timestamp ()
	{
		if (!DataTimeStamps.empty())
		return DataTimeStamps[0];
	}
	data_timestamp_type NVM_Transaction_Flash_WR ::get_data_timestamp (unsigned int idx)
	{
		assert(DataTimeStamps.size() >= idx);
		return DataTimeStamps[idx];
	}
	void NVM_Transaction_Flash_WR ::set_data_timestamp (data_timestamp_type data_timestamp)
	{
		if (!DataTimeStamps.empty())DataTimeStamps.push_back (data_timestamp);
		else DataTimeStamps[0] = data_timestamp;
	}
	void NVM_Transaction_Flash_WR ::set_data_timestamp (data_timestamp_type data_timestamp, unsigned int idx)
	{
		assert(DataTimeStamps.size() >= idx);
		DataTimeStamps[idx] = data_timestamp;
	}

	//FGM - Related reads
	unsigned int NVM_Transaction_Flash_WR:: num_related_reads()
	{
		return RelatedReads.size();
	}
	NVM_Transaction_Flash_RD* NVM_Transaction_Flash_WR:: get_related_read()
	{
		if (!RelatedReads.empty()) return RelatedReads[0];
	}
	NVM_Transaction_Flash_RD* NVM_Transaction_Flash_WR:: get_related_read (unsigned int idx)
	{
		if (!RelatedReads.empty()) return RelatedReads[idx];
	}
	void NVM_Transaction_Flash_WR:: set_related_read (NVM_Transaction_Flash_RD* read_tr)
	{
		if (!RelatedReads.empty()) RelatedReads.push_back (read_tr);
		else RelatedReads[0] = read_tr;
		
	}
	void NVM_Transaction_Flash_WR:: set_related_read (NVM_Transaction_Flash_RD* read_tr, unsigned int idx)
	{
		assert (RelatedReads.size() >= idx);
		RelatedReads[idx] = read_tr;
	}

	//FGM - Handle write sectors bitmap
	page_status_type NVM_Transaction_Flash_WR:: get_sector_bitmap ()
	{
		return write_sectors_bitmap;
	}
	page_status_type NVM_Transaction_Flash_WR:: get_sector_bitmap (
			unsigned int idx, unsigned int sectors_per_mu)
	{	
		assert ((this->num_lpas() >= idx)); 
		
		return (
				(write_sectors_bitmap >> ((this->num_lpas () - idx - 1) * sectors_per_mu)) 
			& (~(0xffffffffffffffff << sectors_per_mu)));
	}
	void NVM_Transaction_Flash_WR:: set_sector_bitmap (page_status_type new_bitmap)
	{
		write_sectors_bitmap = new_bitmap;
	}
	void NVM_Transaction_Flash_WR:: set_sector_bitmap (
		page_status_type new_bitmap, 
		unsigned int idx, 
		unsigned int sectors_per_mu) 
	{
		assert ( num_lpas() >= idx);

		page_status_type mask = ~(0xffffffffffffffff << sectors_per_mu);
		unsigned int shift = ( num_lpas () -  idx - 1) * sectors_per_mu;

		mask = ~(mask << shift);
		write_sectors_bitmap &= mask;
		write_sectors_bitmap |= (new_bitmap << shift);
	}

	//FGM - Update transaction	
	void NVM_Transaction_Flash_WR :: update_transaction (
		unsigned int data_size_in_byte,
		LPA_type lpa,
		PPA_type ppa,
		unsigned int sectors_per_mu,
		page_status_type sector_bitmap,
		data_timestamp_type data_timestamp,
		NVM_Transaction_Flash_RD* related_read)
		{	
			Data_and_metadata_size_in_byte += data_size_in_byte;
			
			LPAs.push_back (lpa);
			PPAs.push_back (ppa);
			
			write_sectors_bitmap <<= sectors_per_mu;
			write_sectors_bitmap |= sector_bitmap;

			DataTimeStamps.push_back (data_timestamp);	
			RelatedReads.push_back (related_read);
		}	
}




