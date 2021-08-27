
#include "Page.h"

namespace NVM
{
    namespace FlashMemory
    {

        Page::Page()
        {
            
        }

        Page::~Page()
        {
            
        }
        void Page::set_metadata (const PageMetadata& metadata, unsigned int idx)
		{
            this->Metadata.LPAs.push_back(metadata.LPAs[idx]) ;
		}
				
		void Page:: get_metadata (PageMetadata& metadata, unsigned int idx) 
		{
           metadata = this->Metadata;
        }
        //FGM - For GC operations
        LPA_type Page::return_metadata(int idx)
        {
            return Metadata.LPAs[idx];
        } 
    }
    
} 
