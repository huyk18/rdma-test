#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <infiniband/verbs.h>
#include <pthread.h>
#include <unistd.h>

#include "dmc_conf.h"
#include "dmc_utils.h"
#include "ib.h"
#include "nm.h"

constexpr uint32_t kBucketSize = 128;
constexpr uint32_t kBucketNum = 1024 * 1024;
constexpr uint32_t kItemSize = 2000;
constexpr uint32_t kMaxItemNum = 1024 * 1024;

#define op_num (100 * 1024 * 1024)
#define read_ratio (0)

class Client {
public:
  Client(const DMCConfig *conf) {
    nm_ = new UDPNetworkManager(conf);
    local_buffer_ =
        nm_->rdma_malloc(local_buffer_mr_, 1024,
                         IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
                             IBV_ACCESS_REMOTE_READ);
    assert(nm_->client_connect_one_rc_qp(0, &remote_mr_info_) == 0);
  }

  void Get(uint64_t index) {
    // read bucket
    nm_->rdma_read_sid_sync(
        0, remote_mr_info_.addr + index % kBucketNum * kBucketSize,
        remote_mr_info_.rkey, (uint64_t)local_buffer_, local_buffer_mr_->rkey,
        kBucketSize);
    nm_->rdma_read_sid_sync(
        0, remote_mr_info_.addr + kBucketNum * kBucketSize + index * kItemSize,
        remote_mr_info_.rkey, (uint64_t)local_buffer_, local_buffer_mr_->rkey,
        kItemSize);
  }

  void Set(uint64_t index) {
    // read index and write kv
    struct ibv_send_wr *head_wr = NULL;
    struct ibv_send_wr write_sr;
    struct ibv_sge write_sge;
    memset(&write_sr, 0, sizeof(struct ibv_send_wr));
    ib_create_sge((uint64_t)local_buffer_, local_buffer_mr_->lkey, kItemSize,
                  &write_sge);
    write_sr.wr_id = 0;
    write_sr.next = head_wr;
    write_sr.sg_list = &write_sge;
    write_sr.num_sge = 1;
    write_sr.opcode = IBV_WR_RDMA_WRITE;
    write_sr.send_flags = IBV_SEND_SIGNALED;
    write_sr.wr.rdma.remote_addr =
        remote_mr_info_.addr + kBucketNum * kBucketSize + kItemSize * index;
    write_sr.wr.rdma.rkey = remote_mr_info_.rkey;
    head_wr = &write_sr;

    // read bucket request
    struct ibv_send_wr read_sr;
    struct ibv_sge read_sge;
    memset(&read_sr, 0, sizeof(struct ibv_send_wr));
    ib_create_sge((uint64_t)local_buffer_, local_buffer_mr_->lkey, kBucketSize,
                  &read_sge);
    read_sr.wr_id = 1;
    read_sr.next = head_wr;
    read_sr.sg_list = &read_sge;
    read_sr.num_sge = 1;
    read_sr.opcode = IBV_WR_RDMA_READ;
    read_sr.send_flags = 0;
    read_sr.wr.rdma.remote_addr = remote_mr_info_.addr + index * kBucketSize;
    read_sr.wr.rdma.rkey = remote_mr_info_.rkey;
    head_wr = &read_sr;

    assert(head_wr != NULL);
    nm_->rdma_post_send_sid_sync(head_wr, 0);

    // update index
    nm_->rdma_cas_sid_sync(
        0, remote_mr_info_.addr + index * kBucketSize + index % 13,
        remote_mr_info_.rkey, (uint64_t)local_buffer_, local_buffer_mr_->rkey,
        0, 0);
  }

  void Run() {
    assert(stick_this_thread_to_core(conf_.core_id) == 0);
    unsigned int rand_seed = time(NULL);
    uint32_t write_offset = 0;

    for (int i = 0; i < op_num; i++) {
      if (rand_r(&rand_seed) > RAND_MAX * read_ratio) {
        Set(write_offset);
        write_offset = (write_offset + 1) % kMaxItemNum;
      } else {
        Get(i % kMaxItemNum);
      }
    }
  }

  static void *thread_run(void *context) {
    ((Client *)context)->Run();
    return NULL;
  }

private:
  UDPNetworkManager *nm_;
  DMCConfig conf_;
  void *local_buffer_;
  ibv_mr *local_buffer_mr_;
  MrInfo remote_mr_info_;
};

int main() {
  DMC dmc;
  dmc.setup_client_conf();
  int client_num = 2;
  Client *clients[client_num];
  pthread_t client_tids[client_num];
  for (int i = 0; i < client_num; i++) {
    auto conf = dmc.client_conf_;
    conf.core_id = i;
    conf.server_id += i;
    clients[i] = new Client(&conf);
  }
  sleep(3);
  for (int i = 0; i < client_num; i++) {
    pthread_create(&client_tids[i], NULL, Client::thread_run, clients[i]);
  }
  for (int i = 0; i < client_num; i++) {
    pthread_join(client_tids[i], NULL);
  }
  return 0;
}
