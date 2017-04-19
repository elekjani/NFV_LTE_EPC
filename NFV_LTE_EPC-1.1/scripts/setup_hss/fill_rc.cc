#include "KVStore.h"
#define ldb_path "172.19.0.5:8090"

using namespace kvstore;
using namespace std;

class Authinfo {
  public:
    uint64_t key_id;
    uint64_t rand_num;

    template <class Archive>
      void serialize(Archive& ar, const unsigned int version);
};

template <class Archive>
void Authinfo::serialize(Archive& ar, const unsigned int version) {
  ar& key_id& rand_num;
}

int main(int argc, char* argv[]) {
  long value1 = 119000000000l;
  long value2 = 0;
  long value3 = 2;
  long p = 1e2;
  KVStore<uint64_t, Authinfo> k;
  k.bind(ldb_path, "ds_autn_info");
  bool allok = true;
  while (value2 < 1e5) {
    if(value2 > p) {
      cout << p << endl;
      p += p;
    }
    Authinfo obj;
    obj.key_id = value2;
    obj.rand_num = value3;

    if (argc > 1) {
      auto kvd = k.put(value1, obj);
      if (kvd.ierr < 0) {
        allok = false;
        cout << "Error in putting data" << endl << kvd.serr << endl;
        break;
      }
    } else {
      auto kvd = k.get(value1);
      if (kvd.ierr < 0) {
        allok = false;
        cout << "Error in getting data" << endl << kvd.serr << endl;
        break;
      } else {
        Authinfo obj2 = kvd.value;
        if (obj2.key_id != obj.key_id || obj2.rand_num != obj.rand_num) {
          allok = false;
          cout << "error" << endl;
          break;
        }
      }
    }

    value1++; value2++; value3++;
  }
  if( allok &&  argc == 1 ) cout<<"Data inserted and checked successfully"<<endl;
}
