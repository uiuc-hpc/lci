/*! \page missing_api A List of Missing API
  Here is a list of missing API:
  - Need to discuss:
    - LCI_error_t LCI_MT_size ( uint32_t length , uint32_t * size ); // don't change
      Related to the actual matching table implementation.
    - LCI_error_t LCI_CQ_size ( uint32_t length , uint32_t * size ); // make it user defined, single writer multiple reader, LCRQ
      Related to the actual Completion Queue implementation.
    - Synchronizer: manual (sync object, synchronizer) v.s. implementation (sync, syncl) // make users choose whether LCI allocate request; document the context
    - memory registration: We don't expose corresponding API to users to use these variables.
        - LCI_REGISTERED_SEGMENT_SIZE amount of memory pre-registered
	with a device. If the value is zero, then all memory is registered or
	registration is not needed. The registered memory is contiguous.
	- LCI_REGISTERED_SEGMENT_START  initial address of
	pre-registered segment. Valid only if LCI_REGISTERED_SEGMENT_SIZE is non-zero.
	- LCI_MAX_REGISTERED_SEGMENT_SIZE maximum length of a
	memory segment that can be registered with a device.
	- LCI_MAX_REGISTERED_SEGMENT_NUMBER maximum number of
	distinct memory segments that can be registered with a device.
    - CQ dequeue: Do we want to return the LCI_request_t by pointers (implementation) or actual objects (manual)?

  - Others:
    - Property List: A large part of property list entries are not implemented
    - LCI_error_t LCI_endpoint_free ( LCI_endpoint_t endpoint );
    - Two-sided Communication:
        - all the functions have parameter order mismatches
    - One-sided Communication:
        - putda
        - all the LCI_get* API
    - LCI_alloc: A default memory allocator, LCI_alloc, is created on initialization. This
                 allocator allocates registered memory.
    - All the group API is missing.
        - LCI_error_t LCI_group_size ( LCI_endpoint_t endpoint, uint32_t * size );
        - LCI_error_t LCI_group_ranks ( LCI_endpoint_t endpoint, char ranks []);
        - LCI_error_t LCI_group_split ( LCI _endpoint_t old_endpoint ,
                                        uint16_t color ,
                                        uint16_t device ,
                                        LCI_PL_t PL ,
                                        LCI_endpoint_t * new_endpoint );
        - LCI_error_t LCI_group_dup ( LCI_endpoint_t old_endpoint ,
                                      uint16_t device ,
                                      LCI_PL_t PL ,
                                      LCI_endpoint_t * new_endpoint );
    - add progress call to all blocking operation? how about the device number?
*/