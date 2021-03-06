/**
 * Copyright 2019 The Gamma Authors.
 *
 * This source code is licensed under the Apache License, Version 2.0 license
 * found in the LICENSE file in the root directory of this source tree.
 */

#ifndef RAW_VECTOR_H_
#define RAW_VECTOR_H_

#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "concurrentqueue/concurrentqueue.h"
#include "gamma_api.h"
#include "raw_vector_common.h"
#include "utils.h"
#include "log.h"

namespace tig_gamma {

template <typename DataType>
class RawVectorIO;

template <typename DataType>
class RawVector {
 public:
  RawVector(const std::string &name, int dimension, int max_vector_size,
            const std::string &root_path);
  virtual ~RawVector();

  /** initialize resource
   *
   * @return 0 if successed
   */
  int Init(bool has_source, bool multi_vids);

  /** get the header of all vectors, so it can access all vecotors through the
   * header if dimension is known
   *
   * @param start start vector id(include)
   * @param end end vector id(exclude)
   * @return vectors header address if successed, null if failed
   */
  virtual int GetVectorHeader(int start, int end,
                              ScopeVector<DataType> &vec) = 0;

  /** dump vectors and sources to disk file
   *
   * @param path the disk directory path
   * @return 0 if successed
   */
  int Dump(const std::string &path, int dump_docid, int max_docid);

  /** load vectors and sources from disk file
   *
   * @param path the disk directory path
   * @return 0 if successed
   */
  int Load(const std::vector<std::string> &path, int doc_num);

  /** get vector by id
   *
   * @param id vector id
   * @return vector if successed, null if failed
   */
  int GetVector(long vid, ScopeVector<DataType> &vec);

  /** get vectors by vecotor id list
   *
   * @param k the length of vector id list
   * @param ids_list vector id list
   * @param resultss(output) vectors, Warning: the vectors must be destroyed by
   * Destroy()
   * @return 0 if successed
   */
  int Gets(int k, long *ids_list, ScopeVectors<DataType> &vecs) const;

  /** get source of one vector, source is a string, for example the image url of
   * vector
   *
   * @param vid vector id
   * @param str(output) the pointer of source string
   * @param len(output) the len of source string
   * @return 0 if successed
   */
  int GetSource(int vid, char *&str, int &len);

  /** add one vector field
   *
   * @param docid doc id, one doc may has multiple vectors
   * @param field vector field, it contains vector(DataType array) and
   * source(string)
   * @return 0 if successed
   */
  int Add(int docid, Field *&field);

  int Update(int docid, Field *&field);

  virtual size_t GetStoreMemUsage() { return 0; }

  long GetTotalMemBytes() {
    GetStoreMemUsage();
    return total_mem_bytes_;
  };
  int GetVectorNum() const { return ntotal_; };
  int GetMaxVectorSize() const { return max_vector_size_; }
  std::string GetName() { return vector_name_; }

  /** add vector to the specific implementation of RawVector(memory or disk)
   *it is called by next common function Add()
   */
  virtual int AddToStore(DataType *v, int len) = 0;

  virtual int UpdateToStore(int vid, DataType *v, int len) = 0;

  int GetDimension() { return dimension_; };

  VIDMgr *vid_mgr_;
  moodycamel::ConcurrentQueue<int> *updated_vids_;

 protected:
  /** get vector by id
   *
   * @param id vector id
   * @return vector if successed, null if failed
   */
  virtual int GetVector(long vid, const DataType *&vec,
                        bool &deletable) const = 0;
  virtual int DumpVectors(int dump_vid, int n) { return 0; }
  virtual int LoadVectors(int vec_num) { return 0; }
  virtual int InitStore() = 0;

 protected:
  friend RawVectorIO<DataType>;
  std::string vector_name_;  // vector name
  int dimension_;            // vector dimension
  int max_vector_size_;
  std::string root_path_;
  int vector_byte_size_;
  int ntotal_;                        // vector num
  long total_mem_bytes_;              // total used memory bytes
  char *str_mem_ptr_;                 // source memory
  std::vector<long> source_mem_pos_;  // position of each source
  bool has_source_;
};

template <typename DataType>
class RawVectorIO {
 public:
  RawVectorIO(RawVector<DataType> *raw_vector);
  ~RawVectorIO();
  int Init();
  int Dump(int start, int n);
  int Load(int doc_num);

 private:
  RawVector<DataType> *raw_vector_;
  int docid_fd_;
  int src_fd_;
  int src_pos_fd_;
};

class AsyncFlusher {
 public:
  AsyncFlusher(std::string name);
  ~AsyncFlusher();
  void Start();
  void Stop();
  void Until(int nexpect);

 protected:
  static void Handler(AsyncFlusher *flusher);
  int Flush();
  virtual int FlushOnce() = 0;

 protected:
  std::string name_;
  std::thread *runner_;
  bool stopped_;
  long nflushed_;
  long last_nflushed_;
  int interval_;
};

template <typename DataType>
void StartFlushingIfNeed(RawVector<DataType> *vec) {
  AsyncFlusher *flusher = nullptr;
  if ((flusher = dynamic_cast<AsyncFlusher *>(vec))) {
    flusher->Start();
    LOG(INFO) << "start flushing, raw vector=" << vec->GetName();
  }
}

template <typename DataType>
void StopFlushingIfNeed(RawVector<DataType> *vec) {
  AsyncFlusher *flusher = nullptr;
  if ((flusher = dynamic_cast<AsyncFlusher *>(vec))) {
    flusher->Until(vec->GetVectorNum());
    flusher->Stop();
    LOG(INFO) << "stop flushing, raw vector=" << vec->GetName();
  }
}

struct StoreParams {
  long cache_size_;  // bytes

  StoreParams() { cache_size_ = -1; }
  StoreParams(const StoreParams &other) {
    this->cache_size_ = other.cache_size_;
  }
  int Parse(const char *str);
  std::string ToString() {
    std::stringstream ss;
    ss << "{cache size=" << cache_size_ << "}";
    return ss.str();
  }
};

}  // namespace tig_gamma
#endif /* RAW_VECTOR_H_ */
