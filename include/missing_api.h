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
    - packet free: when to free to the initial pool, when to free to the current pool? (size > 1024?)
    - How the handle the LCI_request_t and the buffer returned by the completion queue?
        - if LCI_request_t is returned by pointer, we need a LCI_request_free.
        - need the ability to distinguish between user-managed buffers and LCI-managed packets in a completion queue entrie, which is essentially LCI_request_t.
          Such that after we pass the LCI_request_t into the LCI_request_free function, it knows whether to return the packet to the pool/do nothing.
          We could use a LCI_packet_free and let user optionally call it.
    - LCI_error_t LCI_endpoint_free ( LCI_endpoint_t endpoint );
    - what is dreg_init()
    - what is LC_PROTO_LONG?

  - Need to discuss:
    - remote data of RDMA：
        - manual: remote_completion is the address of a synchronizer in remote memory or NULL
        - implementation: 16 bits data treated as a tag
    - what is the LCI_provided default allocator?
        - LCI_alloc: A default memory allocator, LCI_alloc, is created on initialization. This
                     allocator allocates registered memory.
    - packet_data::buffer: why char[0] instead of void*
    - packet_context::hwctx: what is it used for?

  - Others:
    - Property List: A large part of property list entries are not implemented
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