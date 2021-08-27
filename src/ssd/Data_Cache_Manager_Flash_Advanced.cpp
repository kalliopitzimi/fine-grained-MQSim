#include <stdexcept>
#include "../nvm_chip/NVM_Types.h"
#include "Data_Cache_Manager_Flash_Advanced.h"
#include "NVM_Transaction_Flash_RD.h"
#include "NVM_Transaction_Flash_WR.h"
#include "FTL.h"

namespace SSD_Components
{
	Data_Cache_Manager_Flash_Advanced::Data_Cache_Manager_Flash_Advanced(const sim_object_id_type& id, 
		Host_Interface_Base* host_interface, 
		NVM_Firmware* firmware, 
		NVM_PHY_ONFI* flash_controller,
		unsigned int total_capacity_in_bytes,
		unsigned int dram_row_size,
 		unsigned int dram_data_rate, 
		unsigned int dram_busrt_size, 
		sim_time_type dram_tRCD, 
		sim_time_type dram_tCL, 
		sim_time_type dram_tRP,
		Caching_Mode* caching_mode_per_input_stream, 
		Cache_Sharing_Mode sharing_mode,
		unsigned int stream_count,
		unsigned int sector_no_per_page, 
		unsigned int sectors_per_mu,
		unsigned int mu_per_page,
		unsigned int back_pressure_buffer_max_depth)
		: Data_Cache_Manager_Base(id, 
			host_interface, 
			firmware, 
			dram_row_size, 
			dram_data_rate, 
			dram_busrt_size, 
			dram_tRCD, 
			dram_tCL, 
			dram_tRP, 
			caching_mode_per_input_stream, 
			sharing_mode, 
			stream_count),
				flash_controller(flash_controller), 
				capacity_in_bytes(total_capacity_in_bytes), 
				sector_no_per_page(sector_no_per_page),	
				sectors_per_mu(sectors_per_mu),
				mu_per_page(mu_per_page),
				memory_channel_is_busy(false),
				dram_execution_list_turn(0), 
				back_pressure_buffer_max_depth(back_pressure_buffer_max_depth)
	{

		capacity_in_pages = capacity_in_bytes / (SECTOR_SIZE_IN_BYTE * sectors_per_mu);

		switch (sharing_mode)
		{
		case SSD_Components::Cache_Sharing_Mode::SHARED:
		{
			Data_Cache_Flash* sharedCache = new Data_Cache_Flash(capacity_in_pages);
			per_stream_cache = new Data_Cache_Flash*[stream_count];
			for (unsigned int i = 0; i < stream_count; i++)
				per_stream_cache[i] = sharedCache;
			dram_execution_queue = new std::queue<Memory_Transfer_Info*>[1];
			this->back_pressure_buffer_depth = new unsigned int[1];
			this->back_pressure_buffer_depth[0] = 0;
			shared_dram_request_queue = true;
			break; 
		}
		case SSD_Components::Cache_Sharing_Mode::EQUAL_PARTITIONING:
			
			//FGM - User's write and GC buffers
			per_stream_cache = new Data_Cache_Flash*[stream_count];
			per_stream_page_buffer = new Data_Cache_Flash*[stream_count];
			per_stream_GC_buffer = new Data_Cache_Flash*[stream_count];

			for (unsigned int i = 0; i < stream_count; i++)
			{
				per_stream_cache[i] = new Data_Cache_Flash(capacity_in_pages / stream_count);
				per_stream_page_buffer[i] = new Data_Cache_Flash(mu_per_page);
				per_stream_GC_buffer[i] = new Data_Cache_Flash(mu_per_page);
			}	

			dram_execution_queue = new std::queue<Memory_Transfer_Info*>[stream_count];
			waiting_user_requests_queue_for_dram_free_slot = new std::list<User_Request*>[stream_count];
			this->back_pressure_buffer_depth = new unsigned int[stream_count];
			for (unsigned int i = 0; i < stream_count; i++)
				this->back_pressure_buffer_depth[i] = 0;
			shared_dram_request_queue = false;
			break;
		default:
			break;
		}

		bloom_filter = new std::set<LPA_type>[stream_count];
	}
	
	Data_Cache_Manager_Flash_Advanced::~Data_Cache_Manager_Flash_Advanced()
	{
		
		switch (sharing_mode)
		{
		case SSD_Components::Cache_Sharing_Mode::SHARED:
		{
			delete per_stream_cache[0];
			while (dram_execution_queue[0].size())
			{
				delete dram_execution_queue[0].front();
				dram_execution_queue[0].pop();
			}
			for (auto &req : waiting_user_requests_queue_for_dram_free_slot[0])
				delete req;
			break;
		}
		case SSD_Components::Cache_Sharing_Mode::EQUAL_PARTITIONING:
			for (unsigned int i = 0; i < stream_count; i++)
			{
				//FGM - User's write and GC buffers
				delete per_stream_cache[i];
				delete per_stream_page_buffer[i]; 
				delete per_stream_GC_buffer[i]; 

				while (dram_execution_queue[i].size())
				{
					delete dram_execution_queue[i].front();
					dram_execution_queue[i].pop();
				}
				for (auto &req : waiting_user_requests_queue_for_dram_free_slot[i])
					delete req;
			}
			break;
		default:
			break;
		}

		//FGM - User's write and GC buffers
		delete per_stream_cache;
		delete per_stream_page_buffer;
		delete per_stream_GC_buffer;

		delete[] dram_execution_queue;
		delete[] waiting_user_requests_queue_for_dram_free_slot;
		delete[] bloom_filter;
			
	}

	void Data_Cache_Manager_Flash_Advanced::Setup_triggers()
	{
		Data_Cache_Manager_Base::Setup_triggers();
		flash_controller->ConnectToTransactionServicedSignal(handle_transaction_serviced_signal_from_PHY);
	}

	void Data_Cache_Manager_Flash_Advanced::Do_warmup(std::vector<Utils::Workload_Statistics*> workload_stats)
	{
		double total_write_arrival_rate = 0, total_read_arrival_rate = 0;
		switch (sharing_mode) {
			case Cache_Sharing_Mode::SHARED:
				//Estimate read arrival and write arrival rate
				//Estimate the queue length based on the arrival rate
				for (auto &stat : workload_stats) {
					switch (caching_mode_per_input_stream[stat->Stream_id]) {
						case Caching_Mode::TURNED_OFF:
							break;
						case Caching_Mode::READ_CACHE:
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
							} else {
							}
							break;
						case Caching_Mode::WRITE_CACHE:
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
								unsigned int total_pages_accessed = 1;
								switch (stat->Address_distribution_type)
								{
								case Utils::Address_Distribution_Type::STREAMING:
									break;
								case Utils::Address_Distribution_Type::RANDOM_HOTCOLD:
									break;
								case Utils::Address_Distribution_Type::RANDOM_UNIFORM:
									break;
								}
							} else {
							}
							break;
						case Caching_Mode::WRITE_READ_CACHE:
							//Put items on cache based on the accessed addresses
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
							} else {
							}
							break;
					}
				}
				break;
			case Cache_Sharing_Mode::EQUAL_PARTITIONING:
				for (auto &stat : workload_stats) {
					switch (caching_mode_per_input_stream[stat->Stream_id])
					{
						case Caching_Mode::TURNED_OFF:
							break;
						case Caching_Mode::READ_CACHE:
							//Put items on cache based on the accessed addresses
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
							} else {
							}
							break;
						case Caching_Mode::WRITE_CACHE:
							//Estimate the request arrival rate
							//Estimate the request service rate
							//Estimate the average size of requests in the cache
							//Fillup the cache space based on accessed adddresses to the estimated average cache size
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
								//Estimate average write service rate
								unsigned int total_pages_accessed = 1;
								/*double average_write_arrival_rate, stdev_write_arrival_rate;
								double average_read_arrival_rate, stdev_read_arrival_rate;
								double average_write_service_time, average_read_service_time;*/
								switch (stat->Address_distribution_type)
								{
									case Utils::Address_Distribution_Type::STREAMING:
										break;
									case Utils::Address_Distribution_Type::RANDOM_HOTCOLD:
										break;
									case Utils::Address_Distribution_Type::RANDOM_UNIFORM:
										break;
								}
							} else {
							}
							break;
						case Caching_Mode::WRITE_READ_CACHE:
							//Put items on cache based on the accessed addresses
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
							} else {
							}
							break;
					}
				}
				break;
		}
	}
void Data_Cache_Manager_Flash_Advanced::process_new_user_request(User_Request* user_request)
	{
		if (user_request->Transaction_list.size() == 0)//This condition shouldn't happen, but we check it
			return;

		if (user_request->Type == UserRequestType::READ)
		{
			switch (caching_mode_per_input_stream[user_request->Stream_id])
			{
			case Caching_Mode::TURNED_OFF:
				static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(user_request->Transaction_list);
				return;
			case Caching_Mode::WRITE_CACHE:
			case Caching_Mode::READ_CACHE:
			case Caching_Mode::WRITE_READ_CACHE:
			{	
				//FGM - Initialization of the page buffer
				Data_Cache_Flash* cache = per_stream_cache[user_request->Stream_id];
				Data_Cache_Flash* page_buffer = per_stream_page_buffer[user_request->Stream_id];
				Data_Cache_Slot_Type slot;

				bool hit = false;
				std::list<NVM_Transaction*>::iterator it = user_request->Transaction_list.begin();
				while (it != user_request->Transaction_list.end())
				{
					NVM_Transaction_Flash_RD* transaction = (NVM_Transaction_Flash_RD*)(*it);
					if (per_stream_cache[transaction->Stream_id]->Exists(transaction->Stream_id, transaction->get_lpa() ))
					{
						//FGM - User's read request - Cache hit 
						slot = cache-> Get_slot(transaction->Stream_id, transaction->get_lpa());
						hit= true;
					}
					else if ( per_stream_page_buffer[transaction->Stream_id] -> Exists(transaction->Stream_id, transaction->get_lpa()) )
					{
						//FGM - User's read request - Page buffer hit 
						slot = page_buffer-> Get_slot(transaction->Stream_id, transaction->get_lpa());
						hit = true;
					} 
					if (hit)
					{
						page_status_type available_sectors_bitmap = slot.State_bitmap_of_existing_sectors & transaction->read_sectors_bitmap;
						if (available_sectors_bitmap == transaction->read_sectors_bitmap)
						{
							user_request->Sectors_serviced_from_cache += count_sector_no_from_status_bitmap(transaction->read_sectors_bitmap);
							user_request->Transaction_list.erase(it++);//the ++ operation should happen here, otherwise the iterator will be part of the list after erasing it from the list
						}
						else if (available_sectors_bitmap != 0)
						{
							user_request->Sectors_serviced_from_cache += count_sector_no_from_status_bitmap(available_sectors_bitmap);
							transaction->read_sectors_bitmap = (transaction->read_sectors_bitmap & ~available_sectors_bitmap);
							transaction->Data_and_metadata_size_in_byte -= count_sector_no_from_status_bitmap(available_sectors_bitmap) * SECTOR_SIZE_IN_BYTE;
							it++;
						}
						else it++;
					}
					else 
					{
						it++;	
					}
					
	
				}
					
				
				if (user_request->Sectors_serviced_from_cache > 0)
				{
					Memory_Transfer_Info* transfer_info = new Memory_Transfer_Info;
					transfer_info->Size_in_bytes = user_request->Sectors_serviced_from_cache * SECTOR_SIZE_IN_BYTE;
					transfer_info->Related_request = user_request;
					transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_USERIO_FINISHED;
					transfer_info->Stream_id = user_request->Stream_id;
					service_dram_access_request(transfer_info);
				}
				if (user_request->Transaction_list.size() > 0)
					static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(user_request->Transaction_list);

				return;
			}
			}
		}
		else//This is a write request
		{
			switch (caching_mode_per_input_stream[user_request->Stream_id])
			{
				case Caching_Mode::TURNED_OFF:
				case Caching_Mode::READ_CACHE:
					static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(user_request->Transaction_list);
					return;
				case Caching_Mode::WRITE_CACHE://The data cache manger unit performs like a destage buffer
				case Caching_Mode::WRITE_READ_CACHE:
				{
					write_to_destage_buffer(user_request);

					int queue_id = user_request->Stream_id;
					if (shared_dram_request_queue)
						queue_id = 0;
					if (user_request->Transaction_list.size() > 0)
						waiting_user_requests_queue_for_dram_free_slot[queue_id].push_back(user_request);
					return;
				}
			}
		}
	}

	void Data_Cache_Manager_Flash_Advanced::write_to_destage_buffer(User_Request* user_request)
	{
		//To eliminate race condition, MQSim assumes the management information and user data are stored in separate DRAM modules
		unsigned int cache_eviction_read_size_in_sectors = 0;//The size of data evicted from cache
		unsigned int flash_written_back_write_size_in_sectors = 0;//The size of data that is both written back to flash and written to DRAM
		unsigned int dram_write_size_in_sectors = 0;//The size of data written to DRAM (must be >= flash_written_back_write_size_in_sectors)
		std::list<NVM_Transaction*>* evicted_cache_slots = new std::list<NVM_Transaction*>;
		std::list<NVM_Transaction*> writeback_transactions;
		auto it = user_request->Transaction_list.begin();

		int queue_id = user_request->Stream_id;
		if (shared_dram_request_queue) queue_id = 0;


		while (it != user_request->Transaction_list.end() 
			&& (back_pressure_buffer_depth[queue_id] + cache_eviction_read_size_in_sectors + flash_written_back_write_size_in_sectors) < back_pressure_buffer_max_depth)
		{		
		
			NVM_Transaction_Flash_WR* transaction = (NVM_Transaction_Flash_WR*)(*it);

			Data_Cache_Flash* cache = per_stream_cache[transaction->Stream_id];			
			
			Data_Cache_Flash* page_buffer = per_stream_page_buffer[transaction->Stream_id];
		
			//FGM - If the logical address already exists in cache, cache hit
			if (cache ->Exists(transaction->Stream_id, transaction->get_lpa()))
			{
				
				/*MQSim should get rid of writting stale data to the cache.
				* This situation may result from out-of-order transaction execution*/
				Data_Cache_Slot_Type slot = cache ->Get_slot(transaction->Stream_id, transaction->get_lpa());

				sim_time_type timestamp = slot.Timestamp;
				NVM::memory_content_type content = slot.Content;
				if (transaction->get_data_timestamp() > timestamp) {
					timestamp = transaction->get_data_timestamp();
					content = transaction->Content;
				}
				cache->Update_data(transaction->Stream_id, 
							transaction->get_lpa(), 
							content, 
							timestamp, 
							transaction->write_sectors_bitmap | slot.State_bitmap_of_existing_sectors,
							transaction->UserIORequest);
				
			} 
			// FGM - If the logical address already exists in page buffer, page buffer hit
			else if (page_buffer -> Exists (transaction->Stream_id , transaction->get_lpa()) )
			{
				
				//FGM - Update page buffer 
				Data_Cache_Slot_Type slot = page_buffer-> Get_slot(transaction->Stream_id, transaction->get_lpa());

				sim_time_type timestamp = slot.Timestamp;
				NVM::memory_content_type content = slot.Content;
				if (transaction->get_data_timestamp() > timestamp) {
					timestamp = transaction->get_data_timestamp();
					content = transaction->Content;
				}
				
				page_buffer->Update_data(transaction->Stream_id, 
							transaction->get_lpa(), 
							content, 
							timestamp, 
							transaction->write_sectors_bitmap | slot.State_bitmap_of_existing_sectors,
							transaction->UserIORequest);

				//FGM - Cache isn't full, then remove the HIT slot and insert data to cache
				page_buffer-> Remove_slot (transaction->Stream_id, transaction->get_lpa());
				
				//FGM - Cache is full, then evict an LRU slot from cache, promote the HIT slot to cache and insert the evicted slot from the cache to page buffer (swap)
				if (!cache->Check_free_slot_availability())
				{	
					Data_Cache_Slot_Type evicted_slot = cache-> Evict_one_slot_lru();
					if (evicted_slot.Status == Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK)
					{
						//FGM - Assert is triggered when there is NO free space in pb, by default page buffer must have a free slot
						assert(page_buffer->Check_free_slot_availability());
						page_buffer-> Insert_write_data( transaction->Stream_id,
									evicted_slot.LPA,
									evicted_slot.Content,
									evicted_slot.Timestamp,
									evicted_slot.State_bitmap_of_existing_sectors,
									evicted_slot.User_request);
					}

				}
				cache->Insert_write_data( transaction->Stream_id,
							slot.LPA,
							slot.Content,
							slot.Timestamp,
							slot.State_bitmap_of_existing_sectors,
							slot.User_request);	
		
			} 
			//FGM -  If the logical address doesn't exist neither in cache nor in page buffer, cache and page buffer miss
			else 
			{
				if (!cache ->Check_free_slot_availability())
				{
					
					Data_Cache_Slot_Type evicted_slot = cache -> Evict_one_slot_lru();
					if (evicted_slot.Status == Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK)
					{
						//FGM - Cache and page buffer are full, flush and merge page buffer
						if (!page_buffer->Check_free_slot_availability())
						{
							NVM_Transaction_Flash_WR* merged_transaction = merge_and_flush_page_buffer(transaction-> Stream_id);	
							evicted_cache_slots->push_back(merged_transaction);
							
							cache_eviction_read_size_in_sectors += count_sector_no_from_status_bitmap(merged_transaction->write_sectors_bitmap);
							DEBUG2("Evicting page" << evicted_slot.LPA << " from write buffer ")
						}
						page_buffer->Insert_write_data ( transaction->Stream_id,
										evicted_slot.LPA,
										evicted_slot.Content,
										evicted_slot.Timestamp,
										evicted_slot.State_bitmap_of_existing_sectors,
										evicted_slot.User_request);

					}
				}
				cache ->Insert_write_data(transaction->Stream_id, 
										transaction->get_lpa(), 
										transaction->Content, 
										transaction->get_data_timestamp(), 
										transaction->write_sectors_bitmap,
										transaction->UserIORequest);
			}
			dram_write_size_in_sectors += count_sector_no_from_status_bitmap(transaction->write_sectors_bitmap);
			user_request->Transaction_list.erase(it++);
		}
		
		user_request->Sectors_serviced_from_cache += dram_write_size_in_sectors;//This is very important update. It is used to decide when all data sectors of a user request are serviced
		back_pressure_buffer_depth[queue_id] += cache_eviction_read_size_in_sectors + flash_written_back_write_size_in_sectors;

		if (evicted_cache_slots->size() > 0)//Issue memory read for cache evictions
		{
			Memory_Transfer_Info* read_transfer_info = new Memory_Transfer_Info;
			read_transfer_info->Size_in_bytes = cache_eviction_read_size_in_sectors * SECTOR_SIZE_IN_BYTE;
			read_transfer_info->Related_request = evicted_cache_slots;
			read_transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_CACHE_EVICTION_FINISHED;
			read_transfer_info->Stream_id = user_request->Stream_id;
			service_dram_access_request(read_transfer_info);
		}

		if (dram_write_size_in_sectors)//Issue memory write to write data to DRAM
		{
			Memory_Transfer_Info* write_transfer_info = new Memory_Transfer_Info;
			write_transfer_info->Size_in_bytes = dram_write_size_in_sectors * SECTOR_SIZE_IN_BYTE;
			write_transfer_info->Related_request = user_request;
			write_transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_USERIO_FINISHED;
			write_transfer_info->Stream_id = user_request->Stream_id;
			service_dram_access_request(write_transfer_info);
		}
		return;
	}
	//FGM - Implementation of the page buffer 
	NVM_Transaction_Flash_WR* Data_Cache_Manager_Flash_Advanced :: merge_and_flush_page_buffer (stream_id_type stream_id)
	{
		Data_Cache_Flash* cache = per_stream_cache[stream_id];
		Data_Cache_Flash* page_buffer = per_stream_page_buffer[stream_id];
		Data_Cache_Slot_Type slot = page_buffer->Evict_one_slot_lru ();

		NVM_Transaction_Flash_WR* return_transaction = new NVM_Transaction_Flash_WR ( 
					Transaction_Source_Type::CACHE,
					stream_id,
					(count_sector_no_from_status_bitmap (slot.State_bitmap_of_existing_sectors) * SECTOR_SIZE_IN_BYTE),
					slot.LPA,
					slot.User_request, 
					slot.Content,
					slot.State_bitmap_of_existing_sectors,
					slot.Timestamp);

		for (unsigned int i=0; i<mu_per_page-1 ; i++)
		{
			Data_Cache_Slot_Type new_slot = page_buffer->Evict_one_slot_lru();
			return_transaction->update_transaction (
					(count_sector_no_from_status_bitmap (slot.State_bitmap_of_existing_sectors) * SECTOR_SIZE_IN_BYTE),
					new_slot.LPA,
					NO_PPA,
					sector_no_per_page,
					new_slot.State_bitmap_of_existing_sectors,
					new_slot.Timestamp, 
					NULL);
		}
		return return_transaction;	
	}
	
	//FGM - Implementation of the GC Page buffer
	bool Data_Cache_Manager_Flash_Advanced :: Insert_to_GC_buffer ( stream_id_type stream_id, LPA_type lpa, page_status_type page_status_bitmap )
	{

		Data_Cache_Flash* GC_buffer = per_stream_GC_buffer[stream_id];	
		GC_buffer->Insert_write_data( stream_id,
							lpa,
							0,
							INVALID_TIME_STAMP,
							page_status_bitmap,
							NULL);

		//FGM - Inform GC if buffer is full or not
		if (GC_buffer->Check_free_slot_availability()) return false;
		else return true; 
	}
	//FGM - Implementation of the GC Page buffer
	NVM_Transaction_Flash_WR* Data_Cache_Manager_Flash_Advanced:: Merge_and_flush_GC_buffer (stream_id_type stream_id)
	{
		
		Data_Cache_Flash* GC_buffer = per_stream_GC_buffer[stream_id];
		//FGM - If buffer has free space, abort
		assert(!GC_buffer->Check_free_slot_availability());

		Data_Cache_Slot_Type slot = GC_buffer->Evict_one_slot_lru ();
		NVM_Transaction_Flash_WR* return_GC_transaction = new NVM_Transaction_Flash_WR ( 
					Transaction_Source_Type::GC_WL,
					stream_id,
					sectors_per_mu * SECTOR_SIZE_IN_BYTE,
					slot.LPA, 
					NO_PPA, 
					NULL, 
					0, 
					NULL, 
					slot.State_bitmap_of_existing_sectors, 
					INVALID_TIME_STAMP); 

		for (unsigned int i=0; i<mu_per_page-1 ; i++)
		{
			Data_Cache_Slot_Type new_slot = GC_buffer->Evict_one_slot_lru ();
		
			return_GC_transaction->update_transaction (
					sectors_per_mu * SECTOR_SIZE_IN_BYTE,
					new_slot.LPA,
					NO_PPA,
					sectors_per_mu,
					new_slot.State_bitmap_of_existing_sectors,
					new_slot.Timestamp, 
					NULL);
			
		}
		return return_GC_transaction;	
	}

	void Data_Cache_Manager_Flash_Advanced::handle_transaction_serviced_signal_from_PHY(NVM_Transaction_Flash* transaction)
	{

		//FGM - dc_manager handles the RMW operations
		Data_Cache_Manager_Flash_Advanced* dc_manager;
		dc_manager = (Data_Cache_Manager_Flash_Advanced*)_my_instance;
		
		//First check if the transaction source is a user request or the cache itself
		if (transaction->Source != Transaction_Source_Type::USERIO && transaction->Source != Transaction_Source_Type::CACHE)
			return;

		if (transaction->Source == Transaction_Source_Type::USERIO || transaction->Source == Transaction_Source_Type::CACHE)
		{
			dc_manager->broadcast_user_memory_transaction_serviced_signal(transaction);
		}
		/* This is an update read (a read that is generated for a write request that partially updates page data).
		*  An update read transaction is issued in Address Mapping Unit, but is consumed in data cache manager.*/

		if (transaction->Type == Transaction_Type::READ)
		{
			if (((NVM_Transaction_Flash_RD*)transaction)->get_related_write() != NULL)
			{
				((NVM_Transaction_Flash_RD*)transaction)->get_related_write()->set_related_read(NULL);
				return;
			}
			switch (Data_Cache_Manager_Flash_Advanced::caching_mode_per_input_stream[transaction->Stream_id])
			{
			case Caching_Mode::TURNED_OFF:
			case Caching_Mode::WRITE_CACHE:
				transaction->UserIORequest->Transaction_list.remove(transaction);
				if (dc_manager->is_user_request_finished(transaction->UserIORequest))
					dc_manager->broadcast_user_request_serviced_signal(transaction->UserIORequest);
				break;
			case Caching_Mode::READ_CACHE:
			case Caching_Mode::WRITE_READ_CACHE:
			{
				Memory_Transfer_Info* transfer_info = new Memory_Transfer_Info;
				transfer_info->Size_in_bytes = count_sector_no_from_status_bitmap(((NVM_Transaction_Flash_RD*)transaction)->read_sectors_bitmap) * SECTOR_SIZE_IN_BYTE;
				transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_CACHE_FINISHED;
				transfer_info->Stream_id = transaction->Stream_id;
				dc_manager->service_dram_access_request(transfer_info);

				Data_Cache_Flash* cache = dc_manager->per_stream_cache[transaction->Stream_id];
				Data_Cache_Flash* page_buffer = dc_manager->per_stream_page_buffer[transaction->Stream_id];

				
				if (cache->Exists(transaction->Stream_id, transaction->get_lpa()))
				{
				
					/*MQSim should get rid of writting stale data to the cache.
					* This situation may result from out-of-order transaction execution*/
					Data_Cache_Slot_Type slot = cache->Get_slot(transaction->Stream_id, transaction->get_lpa() );
					sim_time_type timestamp = slot.Timestamp;
					NVM::memory_content_type content = slot.Content;
					if (((NVM_Transaction_Flash_RD*)transaction)->get_data_timestamp() > timestamp)
					{
						timestamp = ((NVM_Transaction_Flash_RD*)transaction)->get_data_timestamp();
						content = ((NVM_Transaction_Flash_RD*)transaction)->Content;
					}

					cache->Update_data(transaction->Stream_id, 
							transaction->get_lpa(), 
							content,
							timestamp, 
							((NVM_Transaction_Flash_RD*)transaction)->read_sectors_bitmap | slot.State_bitmap_of_existing_sectors,
							transaction->UserIORequest);
				}
				else if (page_buffer-> Exists(transaction->Stream_id, transaction->get_lpa() ))
				{
					
					Data_Cache_Slot_Type slot = page_buffer->Get_slot(transaction->Stream_id, transaction->get_lpa() );
					sim_time_type timestamp = slot.Timestamp;
					NVM::memory_content_type content = slot.Content;
					if (((NVM_Transaction_Flash_RD*)transaction)->get_data_timestamp() > timestamp)
					{
						timestamp = ((NVM_Transaction_Flash_RD*)transaction)->get_data_timestamp();
						content = ((NVM_Transaction_Flash_RD*)transaction)->Content;
					}

					page_buffer->Update_data(transaction->Stream_id, 
						transaction->get_lpa(), 
						content,
						timestamp, ((NVM_Transaction_Flash_RD*)transaction)->read_sectors_bitmap | slot.State_bitmap_of_existing_sectors,
						transaction->UserIORequest);
				}
				else 
				{
				
					if (!cache->Check_free_slot_availability())
					{
						Data_Cache_Slot_Type evicted_slot =  cache ->Evict_one_slot_lru();
						if (evicted_slot.Status == Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK)
						{
							if (!page_buffer->Check_free_slot_availability())
							{
								std::list<NVM_Transaction*>* evicted_cache_slots = new std::list<NVM_Transaction*>;
								NVM_Transaction_Flash_WR* merged_transaction = dc_manager-> merge_and_flush_page_buffer(transaction->Stream_id);
								evicted_cache_slots->push_back(merged_transaction);

								Memory_Transfer_Info* transfer_info = new Memory_Transfer_Info;
								transfer_info->Size_in_bytes = count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors) * SECTOR_SIZE_IN_BYTE;
								transfer_info->Related_request = evicted_cache_slots;
								transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_CACHE_EVICTION_FINISHED;
								transfer_info->Stream_id = transaction->Stream_id;
								dc_manager ->service_dram_access_request(transfer_info);
							}
							page_buffer-> Insert_write_data( transaction->Stream_id, 
											evicted_slot.LPA,
											evicted_slot.Content,
											evicted_slot.Timestamp,
											evicted_slot.State_bitmap_of_existing_sectors,
											evicted_slot.User_request);
						}
					}

					cache ->Insert_write_data(transaction->Stream_id, 
							transaction->get_lpa(),
							((NVM_Transaction_Flash_RD*)transaction)->Content, 
							((NVM_Transaction_Flash_RD*)transaction)->get_data_timestamp(), 
							((NVM_Transaction_Flash_RD*)transaction)->read_sectors_bitmap,
							transaction->UserIORequest);

					Memory_Transfer_Info* transfer_info = new Memory_Transfer_Info;
					transfer_info->Size_in_bytes = count_sector_no_from_status_bitmap(((NVM_Transaction_Flash_RD*)transaction)->read_sectors_bitmap) * SECTOR_SIZE_IN_BYTE;
					transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_CACHE_FINISHED;
					transfer_info->Stream_id = transaction->Stream_id;
					dc_manager->service_dram_access_request(transfer_info);
				}

				transaction->UserIORequest->Transaction_list.remove(transaction);
				if (dc_manager->is_user_request_finished(transaction->UserIORequest))
					dc_manager->broadcast_user_request_serviced_signal(transaction->UserIORequest);
				break;
			}
			}
		}
		else//This is a write request
		{
	
			switch (Data_Cache_Manager_Flash_Advanced::caching_mode_per_input_stream[transaction->Stream_id])
			{
				case Caching_Mode::TURNED_OFF:
				case Caching_Mode::READ_CACHE:
					transaction->UserIORequest->Transaction_list.remove(transaction);
					if (dc_manager->is_user_request_finished(transaction->UserIORequest))
						dc_manager->broadcast_user_request_serviced_signal(transaction->UserIORequest);
					break;
				case Caching_Mode::WRITE_CACHE:
				case Caching_Mode::WRITE_READ_CACHE:
				{
					int sharing_id = transaction->Stream_id;
					if (dc_manager->shared_dram_request_queue) sharing_id = 0;
		
					dc_manager->back_pressure_buffer_depth[sharing_id] -= transaction->Data_and_metadata_size_in_byte / SECTOR_SIZE_IN_BYTE + (transaction->Data_and_metadata_size_in_byte % SECTOR_SIZE_IN_BYTE == 0 ? 0 : 1);
					
					Data_Cache_Flash *cache = dc_manager->per_stream_cache[transaction->Stream_id];
					Data_Cache_Flash *page_buffer = dc_manager->per_stream_page_buffer[transaction->Stream_id];
					Data_Cache_Flash *cache_to_evict = NULL;

					if (cache->Exists(transaction->Stream_id, ((NVM_Transaction_Flash_WR*)transaction)->get_lpa()))
					{	
						cache_to_evict = cache;
					}
					
					if (page_buffer ->Exists(transaction->Stream_id, ((NVM_Transaction_Flash_WR*)transaction)->get_lpa()))
					{	
						cache_to_evict = page_buffer;
					}
					if (cache_to_evict!= NULL)
					{
						Data_Cache_Slot_Type slot = cache_to_evict ->Get_slot(transaction->Stream_id, ((NVM_Transaction_Flash_WR*)transaction)->get_lpa() );
						sim_time_type timestamp = slot.Timestamp;
						NVM::memory_content_type content = slot.Content;
						if (((NVM_Transaction_Flash_WR*)transaction)->get_data_timestamp() >= timestamp) {
							cache_to_evict->Remove_slot(transaction->Stream_id, ((NVM_Transaction_Flash_WR*)transaction)->get_lpa() );
						}
					}

					auto user_request = dc_manager ->waiting_user_requests_queue_for_dram_free_slot[sharing_id].begin();
					while (user_request != dc_manager->waiting_user_requests_queue_for_dram_free_slot[sharing_id].end())
					{
						dc_manager ->write_to_destage_buffer(*user_request);
						if ((*user_request)->Transaction_list.size() == 0)
							dc_manager->waiting_user_requests_queue_for_dram_free_slot[sharing_id].erase(user_request++);
						else user_request++;
						if ( dc_manager->back_pressure_buffer_depth[sharing_id] > dc_manager->back_pressure_buffer_max_depth)//The traffic load on the backend is high and the waiting requests cannot be serviced
							break;
					}

					break;
				}
			}
		}
	}

	void Data_Cache_Manager_Flash_Advanced::service_dram_access_request(Memory_Transfer_Info* request_info)
	{
		if (memory_channel_is_busy)
		{
			if(shared_dram_request_queue)
				dram_execution_queue[0].push(request_info);
			else
				dram_execution_queue[request_info->Stream_id].push(request_info);
		}
		else
		{
			Simulator->Register_sim_event(Simulator->Time() + estimate_dram_access_time(request_info->Size_in_bytes, dram_row_size,
				dram_busrt_size, dram_burst_transfer_time_ddr, dram_tRCD, dram_tCL, dram_tRP),
				this, request_info, static_cast<int>(request_info->next_event_type));
			memory_channel_is_busy = true;
			dram_execution_list_turn = request_info->Stream_id;
		}
	
	}

	void Data_Cache_Manager_Flash_Advanced::Execute_simulator_event(MQSimEngine::Sim_Event* ev)
	{
		
		

		Data_Cache_Simulation_Event_Type eventType = (Data_Cache_Simulation_Event_Type)ev->Type;
		Memory_Transfer_Info* transfer_info = (Memory_Transfer_Info*)ev->Parameters;

		switch (eventType)
		{
		case Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_USERIO_FINISHED://A user read is service from DRAM cache
		case Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_USERIO_FINISHED:
			((User_Request*)(transfer_info)->Related_request)->Sectors_serviced_from_cache -= transfer_info->Size_in_bytes / SECTOR_SIZE_IN_BYTE;
			if (is_user_request_finished((User_Request*)(transfer_info)->Related_request))
				broadcast_user_request_serviced_signal(((User_Request*)(transfer_info)->Related_request));
			break;
		case Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_CACHE_EVICTION_FINISHED://Reading data from DRAM and writing it back to the flash storage
			static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(*((std::list<NVM_Transaction*>*)(transfer_info->Related_request)));
			delete (std::list<NVM_Transaction*>*)transfer_info->Related_request;
			break;
		case Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_CACHE_FINISHED://The recently read data from flash is written back to memory to support future user read requests
			break;
		}
		delete transfer_info;

		memory_channel_is_busy = false;
		if (shared_dram_request_queue)
		{
			if (dram_execution_queue[0].size() > 0)
			{
				Memory_Transfer_Info* transfer_info = dram_execution_queue[0].front();
				dram_execution_queue[0].pop();
				Simulator->Register_sim_event(Simulator->Time() + estimate_dram_access_time(transfer_info->Size_in_bytes, dram_row_size, dram_busrt_size,
					dram_burst_transfer_time_ddr, dram_tRCD, dram_tCL, dram_tRP),
					this, transfer_info, static_cast<int>(transfer_info->next_event_type));
				memory_channel_is_busy = true;
			}
		}
		else
		{
			for (unsigned int i = 0; i < stream_count; i++)
			{
				dram_execution_list_turn++;
				dram_execution_list_turn %= stream_count;
				if (dram_execution_queue[dram_execution_list_turn].size() > 0)
				{
					Memory_Transfer_Info* transfer_info = dram_execution_queue[dram_execution_list_turn].front();
					dram_execution_queue[dram_execution_list_turn].pop();
					Simulator->Register_sim_event(Simulator->Time() + estimate_dram_access_time(transfer_info->Size_in_bytes, dram_row_size, dram_busrt_size,
						dram_burst_transfer_time_ddr, dram_tRCD, dram_tCL, dram_tRP),
						this, transfer_info, static_cast<int>(transfer_info->next_event_type));
					memory_channel_is_busy = true;
					break;
				}
			}
		}
	}
}