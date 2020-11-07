/*! \page missing_api A List of Missing API
  Here is a list of missing API:
  - Discussed:
    - LCI_error_t LCI_MT_size ( uint32_t length , uint32_t * size ); // don't change
      Related to the actual matching table implementation.
    - LCI_error_t LCI_CQ_size ( uint32_t length , uint32_t * size ); // make it user defined, single writer multiple reader, LCRQ
      Related to the actual Completion Queue implementation.
    - Synchronizer: manual (sync object, synchronizer) v.s. implementation (sync, syncl) // make users choose whether LCI allocate request; document the context
    - LCI_ivalue_t or LCI_idata_t? LCI_ivalue_t
    - The send functions pass a LCI_buffer_t* parameter? also has a length parameter? should be void*
    - memory registration: We don't expose corresponding API for users to register memory region.
        - LCI_register function. return a mr_desc
    - CQ dequeue: Do we want to return the LCI_request_t by pointers (implementation) or actual objects (manual)?
    - Whether to use opaque type:
        - uint16_t or LCI_tag_t?
        - uint32_t or LCI_rank_t?
  - Need to discuss:
    - packet free: when to free to the initial pool, when to free to the current pool? (size > 1024?)
    - server.h: what's the meaning of these interfaces?
    - LCI_error_t LCI_endpoint_free ( LCI_endpoint_t endpoint );

  - Others:
    - Property List: A large part of property list entries are not implemented
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
*/