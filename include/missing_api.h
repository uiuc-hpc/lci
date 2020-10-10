/*! \page missing_api A List of Missing API
  Here is a list of missing API:
  - Variable:
      - LCI_MAX_TAG
      - LCI_REGISTERED_SEGMENT_START
      - LCI_MAX_REGISTERED_SEGMENT_SIZE
      - LCI_MAX_REGISTERED_SEGMENT_NUMBER
      - LCI_DEFAULT_MT_LENGTH
      - LCI_MAX_MT_LENGTH
      - LCI_DEFAULT_CQ_LENGTH
      - LCI_MAX_CQ_LENGTH
  - LCI_error_t LCI_MT_size ( uint32_t length , uint32_t * size );
  - LCI_error_t LCI_CQ_size ( uint32_t length , uint32_t * size );
  - LCI_error_t LCI_wait_dequeue ( LCI_comp_t cq , LCI_request_t * request );
  - LCI_error_t LCI_mult_wait_dequeue( LCI_comp_t cq ,
                                       LCI_request_t requests [] ,
                                       uint8_t count );
  - Synchronizer:
      - LCI_error_t LCI_SO_init ( LCI_SO_t * syncobject ,
                                  LCI_comptype_t type ;
                                  uint8
                                  max_thread_count );
      - LCI_error_t LCI_SO_free ( LCI_SO_t * syncobject );
      - LCI_error_t LCI_SO_type ( LCI_SO_t * syncobject ,
                                  LCI_comptype_t * type );
      - LCI_error_t LCI_sync_free ( LCI_sync_t * sync );
      - LCI_error_t LCI_one2one_test_full ( LCI_sync_one2one_t *sync,
                                            __Bool * flag );
      - LCI_error_t LCI_one2one_get ( LCI_sync_one2one_t *sync,
                                      __Bool *flag );
      - many2one, one2many, many2many: set, wait, test, get
  - Property List:
      - LCI_error_t LCI_PL_get ( LCI_endpoint_t endpoint, LCI_PL_t *plist );
      - LCI_PL_set_*: the order of the parameter list is reversed
      - LCI_error_t LCI_PL_set_dynamic(LCI_PL_t plist, LCI_dynamic_t type);
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
  - LCI_error_t LCI_endpoint_free ( LCI_endpoint_t* endpoint );
  - Two-sided Communication:
      - all the functions have parameter order mismatches
  - One-sided Communication:
      - putda
      - all the LCI_get* API
*/