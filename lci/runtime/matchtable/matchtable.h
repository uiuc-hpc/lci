#ifndef LCI_MATCHTABLE_H
#define LCI_MATCHTABLE_H

enum LCII_matchtable_insert_type { LCII_MATCHTABLE_SEND, LCII_MATCHTABLE_RECV };

void LCII_matchtable_create(LCI_matchtable_t* mt_p);
void LCII_matchtable_free(LCI_matchtable_t* mt_p);
LCI_error_t LCII_matchtable_insert(LCI_matchtable_t mt, uint64_t key,
                                   uint64_t* value,
                                   enum LCII_matchtable_insert_type type);

struct LCII_matchtable_ops_t {
  void (*create)(LCI_matchtable_t* mt_p);
  LCI_error_t (*insert)(LCI_matchtable_t mt, uint64_t key, uint64_t* value,
                        enum LCII_matchtable_insert_type type);
  void (*free)(LCI_matchtable_t* mt_p);
};

#endif  // LCI_MATCHTABLE_H
