#include "NVM_Transaction_Flash.h"
#include "assert.h"


namespace SSD_Components
{

	NVM_Transaction_Flash::NVM_Transaction_Flash(
			Transaction_Source_Type source, 
			Transaction_Type type, 
			stream_id_type stream_id,
			unsigned int data_size_in_byte, 
			LPA_type lpa,
			PPA_type ppa,
			User_Request* user_request):
		NVM_Transaction(
				stream_id, 
				source, 
				type, 
				user_request),
		Data_and_metadata_size_in_byte(data_size_in_byte), 
		Physical_address_determined(false), 
		FLIN_Barrier(false)
	{
		LPAs.push_back (lpa);	
		PPAs.push_back (ppa);	
	}
	
	NVM_Transaction_Flash::NVM_Transaction_Flash(
			Transaction_Source_Type source, 
			Transaction_Type type, 
			stream_id_type stream_id,
			unsigned int data_size_in_byte, 
			LPA_type lpa,
			PPA_type ppa,
			const NVM::FlashMemory::Physical_Page_Address& address, 
			User_Request* user_request) :
		NVM_Transaction(
				stream_id, 
				source, 
				type, 
				user_request), 
		Data_and_metadata_size_in_byte(data_size_in_byte), 
		Address(address), 
		Physical_address_determined(false)
	{
		LPAs.push_back (lpa);
		PPAs.push_back (ppa);
	}
	


	//FGM - LPAs
	unsigned int NVM_Transaction_Flash::num_lpas ()
	{
		return LPAs.size ();
	}
	LPA_type NVM_Transaction_Flash::get_lpa ()
	{
		if (!LPAs.empty()) return LPAs[0];
	}
	LPA_type NVM_Transaction_Flash::get_lpa (int idx)
	{
		if (!LPAs.empty()) return LPAs[idx];
	}
	void NVM_Transaction_Flash::set_lpa (LPA_type lpa)
	{
		if (!LPAs.empty()) LPAs.push_back (lpa);
		else LPAs[0] = lpa;
	}
	void NVM_Transaction_Flash::set_lpa (LPA_type lpa, int idx)
	{
		assert(LPAs.size() >= idx);
		if (LPAs.empty()) LPAs.push_back(lpa);
		else LPAs[idx] = lpa;
	}

	//FGM - PPAs
	unsigned int NVM_Transaction_Flash::num_ppas ()
	{
		return PPAs.size ();
	}
	PPA_type NVM_Transaction_Flash::get_ppa ()
	{
		if (!PPAs.empty()) return PPAs[0];
	}
	PPA_type NVM_Transaction_Flash::get_ppa (int idx)
	{
		if (!PPAs.empty()) return PPAs[idx];
	}
	void NVM_Transaction_Flash::set_ppa (PPA_type ppa)
	{
		if (!PPAs.empty()) PPAs.push_back (ppa);
		else PPAs[0] = ppa;
		
	}
	void NVM_Transaction_Flash::set_ppa (PPA_type ppa, int idx)
	{
		assert(PPAs.size() >= idx);
		if (PPAs.empty()) PPAs.push_back (ppa); 
		else PPAs[idx] = ppa;
	}

	//FGM - LPAs for GC 
	void NVM_Transaction_Flash::replace_lpa(LPA_type lpa, int idx, int i)
	{
		assert(!LPAs.empty());
		LPAs.erase ( LPAs.begin() + idx  );
		LPAs.insert( LPAs.begin()+ idx, lpa);

		Waiting_LPAs.erase( Waiting_LPAs.begin() + i );
	}
	void NVM_Transaction_Flash:: set_waiting_lpas (LPA_type lpa)
	{
		Waiting_LPAs.push_back(lpa);
	}
	LPA_type NVM_Transaction_Flash:: get_waiting_lpas (int idx)
	{
		assert(!Waiting_LPAs.empty());
		return Waiting_LPAs[idx];
	}
}
	
