#ifndef PAGE_H
#define PAGE_H

#include "FlashTypes.h"

#include <assert.h>
#include <vector>

namespace NVM
{
	namespace FlashMemory
	{
		struct PageMetadata
		{
			
			//FGM - Dynamic LPAs
			std::vector<LPA_type> LPAs; 

		};
		
		class Page 
		{
			public:

				Page();
				~Page();

				PageMetadata Metadata;
				//FGM - Utility functions to support the multiple LPAs
				void set_metadata (const PageMetadata& metadata, unsigned int idx) ; 
				void get_metadata (PageMetadata& metadata, unsigned int idx) ;
				LPA_type return_metadata(int idx);			
		};
	}
}

#endif // !PAGE_H
