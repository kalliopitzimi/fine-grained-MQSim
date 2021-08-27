# Fine-grained address mapping
# How we extended MQSim: What has been modified, why and where 

## 1) Device Parameter Set 
Found in src/exec/Device_Parameter_Set:
Added "Mapping Unit" as a new Device Parameter. With this addition the user can specify the mapping unit size in the configuration file.

## 2) SSD Device
Found in src/exec/SSD_Device:

We are defining the variables for printing in main.cpp and the key variables of this project, which are:
- sectors/mapping unit 
- mapping unit/page

The key idea of the FGM scheme is to make the FTL consious of the mapping unit existence. We do this by breaking the levels of abrstaction and adding a new value, the mapping unit.

During the creating of the SSD device FTL and non-FTL components have been impacted by the new FGM scheme.

Namely, the FTL components are the following:
* Address Mapping Unit 
* Flash Block Manager
* GC and WL 
* FTL

Namely, the non-FTL components are the following:
* Host Interface 
* Data Cache Manager
* Logical Address Partitioning Unit
* NVM PHY ONFI NVDDR2
* NVM Transaction Flash (RD, WR)



### Address Mapping Unit 
Found in src/ssd/Address_Mapping_Unit_Base/Hybrid:
Added the two key variables to constructors.

Found in src/ssd/Address_Mapping_Unit_Page_Level:

In this code, the FGM scheme has impacted 4 functionalities of MQSim:

#### Allocation of a page for a user/gc write transaction. 
When a new, merged transaction has been evicted from the cache, the Address Mapping Unit is responsible
for assigning a new physical address. We modify some parts of the code that allocate a page in plane for a user write. The idea here is to iterate through each LPA of the merged transaction, and allocate a different (however, consecutive) PPA.

#### The plane allocation algorithm in a Round Robin fashion
Traditionally, MQSim was allocating a plane based on the LPA value. However, with the fine-grained address mapping we are allocating one merged transaction to one page with multiple LPAs, i.e. we decouple the bonding between the plane and the logical address. A Round Robin algorithm was chosen to allocate a new plane for the logical addresses. The channel ID, the chip ID, the die ID and the plane ID are calculated based on the Plane Allocation Scheme (e.g. allocate CWDP which means Channel, Way (chip), Die, Plane) and the Round-Robin algorithm. This can be found on the new method called "calculate resource indices".

#### Methods that convert the physical address
The FGM-enabled FTL is considering a mapping unit as one individual page. This means that the number of pages that FTL considers is different from the actual pages in NAND Flash memory. There are three methods ( 1. void Convert ppa to address, 2. NVM::FlashMemory::Physical_Page_Address Convert_ppa_to_address, 3. PPA_type Convert_address_to_ppa ) that do the switch between these two concepts and we modified them to support the new FTL's concept. Note that, Physical Page Address is how the back end (NAND flash memory) interprets the address and PPA is how FTL interprets the address.

#### Set of a barrier for accessing a physical block during GC operations
When the victim block has been detected during GC operations, MQSim makes sure that during the copy of the valid pages to a new free block, no user request will interfere with this block. For that reason, we need to set a barrier to this block. MQSim's original code was iterating and locking the LPAs of each valid page, but we add the iteration of each valid mapping unit and locking multiple LPAs of a page. Also, when asking for flash controller to bring the metadata from the flash chip we also send the MUID (mapping unit ID), because we store multiple LPAs in the metadata of a physical page.

**Note: Fine-grained address mapping is supported with an ideal mapping table.** 


### Flash Block Manager 

Found in src/ssd/Flash_Block_Manager_Base:
Added mapping unit/page to constructor.

Found in src/ssd/Flash_Block_Manager:

#### Page invalidation mechanism 
The new page invalidation mechanism is focusing on invalidating mapping units, that will further invalidate a full page.
Originally, Flash Block Manager used a page status bitmap to decide if a page is valid or invalid. Instead of using page status bitmaps, we assign a boolean value. The main idea is : If there is at least one valid mapping unit within a page, then page is still valid. Therefore, a page is invalid if only all mapping units are invalid.


### GC and WL 
Found in src/ssd/GC_and_WL_Base:
Added the two key variables to constructor.

#### Implementation of the GC Page buffer 
This class and more specifically the method: " handle transaction serviced signal from PHY" is where we link the GC RD and GC WR. A GC RD has come from the flash controller and we need to search for the right lpa in the waiting lpas list. Only when we have confirmed that the lpa that came from the flash controller is the same with the one that has been locked in the address mapping unit (Get data mapping info for gc), we are able to insert the transaction to the GC Page buffer. The GC Page buffer is a new feature of MQSim and the idea behind it is to buffer some GC RD transactions and once the buffer is full, we issue the corresponding, merged GC WR. Both user writes buffer, which exists in the Data Cache Manager, and GC buffer serve the same purpose. Two new routines have been added to create the link between Data Cache - GC Base, which are "Insert to GC buffer" and "Merge and flush GC buffer".

Found in src/ssd/GC_and_WL_Page_Level:
#### GC activities 
When the victim block has been chosen, it is important for the GC routines to copy all the valid pages from the victim block. Originally, MQSim's GC mechanism strongly linked the GC RD and GC WR, meaning that every time a GC RD was issued, it had to be linked with its corresponding GC WR, which was a related write of the read. In the new GC mechanism this link is destroyed. The algorithm iterates through each valid page of the block and issues a GC RD. For each valid mapping unit it updates the transaction into a merged GC RD in order to read all the valid mapping units within a page. Once the GC RD and GC WR have been serviced, a GC Erase will erase the block. The new merged GC RD is linked with the erase operation through  the Related Erase argument. This field did not exist previously in MQSim. Now both GC RD and GC WR have a Related Erase.

**Note: Normally, Garbage Collection activities are enabled when preconditioning has also been enabled. We haven’t tested this combination. The Garbage Collection activities were tested with a small SSD and a high gc threshold. Also, the copyback execution mode is not supported.**

### FTL 
Found in src/ssd/FTL:
The preconditioning function now operates with the sectors/mapping unit value, instead with the sectors/page.

We highlight here that we have also added the RANDOM_UNIFORM as the address distribution when we precondition with a trace based workload. We copied this code as the default case. That did not exist in original MQSim's code and is not directly related with the FGM scheme.


### Host Interface NVMe 
Found in src/ssd/Host_Interface_NVMe:

When a request breaks into multiple transactions, the logical address space is divided based on sectors per mapping unit, instead of sectors per page.

### Data Cache Manager 
Found in src/ssd/Data_Cache_Manager_Base.h:
Prototyping of the GC functions for the GC Page buffer implementation	

Found in src/ssd/Data_Cache_Manager_Simple:
Added the two key variables to constructor.
Found in src/ssd/Data_Cache_Manager_Advanced:

#### The new Data Cache Manager
We reserve a number of slots from DRAM to create the Page buffer. That number is equal to the mapping units per page, i.e. the page buffer has as many slots as the transactions that are going to be merged.

The new Data Cache Manager works as follows:

- If transactions exists in Cache (cache hit): update it.
- If transaction exists in Page buffer (page buffer hit): update it, promote it to Cache.
    - If Cache is full, remove the hit slot from buffer, evict the LRU slot from Cache and insert it to Page buffer and add promote the hit slot to Cache (swap).
- If transaction doesn't exist neither in Cache nor in Page buffer (cache and page buffer miss):
    - If Cache has free space: insert transaction to Cache
    - If Cache is full and Page buffer has free space: insert transaction to Page buffer
    - If Cache is full and Page buffer is full: merge and flush page buffer

**Note: The data cache sharing mode that is supported by the Fine-grained mapping is Equal partitioning.**

### Logical Address Partitioning Unit
Found in utils/Logical_Address_Partitioning_Unit:
Added the sectors/mapping unit to constructor
	
This unit manipulates the Logical Address Space of the SSD. For the logical address partitioning, MQSim now divides the logical sector address based on the sectors/mapping unit, instead of sectors/page.

### NVM PHY ONFI NVDDR2
Found in src/ssd/NVM_PHY_ONFI_NVDDR2:
Added the mapping unit/page to constructor.

A flash controller makes the transition from a transaction (an FTL concept) to a flash command (a device model concept). Our contribution here is to support the MUID that was sent from the Address Mapping Unit (Set barrier to a physical block). Also, when a flash command is generated we make sure to copy from the transaction all the LPAs and create the metadata for this Active Command. When the flash controller will send the data to GC Base class the method "copy read to transaction" will copy the LPAs from the metadata.

### NVM Transaction Flash (RD, WR)

#### Dynamic structures
The core idea of the fine-grained address mapping and the biggest impact on the simulator are the data structures (i.e. NVM Transaction Flash/RD/WR). Originally the structures of MQSim were static, in a sense that there was a 1-1 match between an LPA and a NVM Transaction Flash. We replace this 1-1 match by supporting dynamic structures like vectors to LPAs and PPAs. We also add some get - set routines that are used by the majority of FTL components. The update transaction method is very important to our project because in this way we can create a merged transaction.

## 3) Flash memory
Found in src/nvm_chip/flash_memory:
#### Physical Page Address
Along with ChannelID, ChipID, DieID, PlaneID, BlockID, PageID we added the MUID.

#### Flash Command
The metadata used to be static (i.e. only one LPA per page). For this project we need dynamic structures (i.e. a vector) to support multiple LPAs within metadata.

#### Flash Chip
When a flash command finishes its execution (finish command execution), the data metadata are updated. For example, when we write to a flash page we also store the LPA as metadata. For that reason, we iterate through the metadata size (= number of LPAs) and set the metadata in the Out Of Bound area (OOB area) of a page.

#### Page
We added a new method to return the metadata. Based on the idx (= mapping unit), the Page returns the corresponding LPA of the metadata. We added the Page.cpp to support this dynamic functionality.


## How to run the fine-grained address mapping
There are four key parameters when running a workload on an SSD with the fine-grained address mapping:


#### 1)Sector size:
To change the sector size of the SSD go to src/ssd/SSD_Defs.h and define the SECTOR_SIZE_IN_BYTE variable. Then clean the object files (make clean) and make again. Note that MQSim considers a sector size of 512 bytes, but we increase this value to 4 KiB.
#### 2) Mapping Unit
To change the mapping unit open ssdconfig.xml and enter the value in bytes. Note that mapping unit should be equal or greater than sector size. Both the sector size and the mapping unit are software concepts. For example, if the controller wants to read a particular sector in a page, it has to read the whole page and return from the device back to controller only the meaningful data. The minimum read/write unit is a page. The mapping unit is how coarse of fine the FTL considers a page.
#### 3) Page size
To change the mapping unit open ssdconfig.xml and enter the value in bytes. Note that page size should be always equal or greater than mapping unit.

In other words: sector <= mapping unit <= page size

#### 4) Request size
To change the request size open workload.xml and enter the parameter "Average Request Size". Note that this value is calculated in sectors.

**Note: Search for the FGM tag to the modified parts of the code that relate with the FGM scheme.**



