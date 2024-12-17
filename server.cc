#include "dmc_conf.h"
#include "dmc_utils.h"
#include "nm.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <infiniband/verbs.h>
#include <sys/socket.h>

class Server {
public:
  Server(DMCConfig conf) : conf_(conf) {
    nm_ = new UDPNetworkManager(&conf_);
    auto pd = nm_->get_ib_pd();
    recv_msg_buffer_ =
        nm_->rdma_malloc(recv_msg_buffer_mr_, MSG_BUF_SIZE * MSG_BUF_NUM,
                         IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    for (uint64_t i = 0; i < MSG_BUF_NUM; i++) {
      // initialize rr_list_
      rr_list_[i].wr_id = i;
      rr_list_[i].next = NULL;
      rr_list_[i].sg_list = &rr_sge_list_[i];
      rr_list_[i].num_sge = 1;
      rr_sge_list_[i].addr = (uint64_t)recv_msg_buffer_ + i * MSG_BUF_SIZE;
      rr_sge_list_[i].length = MSG_BUF_SIZE;
      rr_sge_list_[i].lkey = recv_msg_buffer_mr_->lkey;
    }
    data_ = aligned_alloc(4096, conf_.server_data_len);
    memset(data_, 0, conf_.server_data_len);
    int access_flag = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                      IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;
    mr_ = ibv_reg_mr(pd, data_, conf_.server_data_len, access_flag);
  }
  int server_on_connect(const UDPMsg *request, struct sockaddr_in *src_addr,
                        socklen_t src_addr_len);
  int server_on_alloc(const UDPMsg *request, struct sockaddr_in *src_addr,
                      socklen_t src_addr_len);
  void run();

private:
  UDPNetworkManager *nm_;
  DMCConfig conf_;
  struct ibv_recv_wr rr_list_[MSG_BUF_NUM];
  struct ibv_sge rr_sge_list_[MSG_BUF_NUM];
  void *recv_msg_buffer_;
  ibv_mr *recv_msg_buffer_mr_;
  void *data_;
  ibv_mr *mr_;
};

int Server::server_on_alloc(const UDPMsg *request, struct sockaddr_in *src_addr,
                            socklen_t src_addr_len) {
  UDPMsg reply;
  memset(&reply, 0, sizeof(UDPMsg));
  reply.type = UDPMSG_REP_ALLOC;
  reply.id = 0;
  reply.body.mr_info.addr = (uint64_t)data_;
  reply.body.mr_info.rkey = mr_->rkey;
  serialize_udpmsg(&reply);

  auto ret = nm_->send_udp_msg(&reply, src_addr, src_addr_len);
  assert(ret == 0);
  return 0;
}

int Server::server_on_connect(const UDPMsg *request,
                              struct sockaddr_in *src_addr,
                              socklen_t src_addr_len) {
  int rc = 0;
  UDPMsg reply;
  memset(&reply, 0, sizeof(UDPMsg));

  reply.id = 0;
  reply.type = UDPMSG_REP_CONNECT;
  reply.body.conn_info.mr_info.addr = (uint64_t)data_;
  reply.body.conn_info.mr_info.rkey = mr_->rkey;
  rc = nm_->nm_on_connect_new_qp(request, &reply.body.conn_info.qp_info);
  assert(rc == 0);

  serialize_udpmsg(&reply);

  rc = nm_->send_udp_msg(&reply, src_addr, src_addr_len);
  assert(rc == 0);

  deserialize_udpmsg(&reply);
  rc = nm_->nm_on_connect_connect_qp(request->id, &reply.body.conn_info.qp_info,
                                     &request->body.conn_info.qp_info);
  assert(rc == 0);

  // post recv if the server is active
  if (true) {
    assert(request->id < 512);
    rc = nm_->rdma_post_recv_sid_async(&rr_list_[request->id], request->id);
  }
  return 0;
}

void Server::run() {
  bool stop = false;
  int rc = 0;
  UDPMsg request;
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  while (!stop) {
    rc = nm_->recv_udp_msg(&request, &client_addr, &client_addr_len);
    if (rc) {
      continue;
    }
    deserialize_udpmsg(&request);

    if (request.type == UDPMSG_REQ_CONNECT) {
      rc = server_on_connect(&request, &client_addr, client_addr_len);
    }
    if (request.type == UDPMSG_REQ_ALLOC) {
      rc = server_on_alloc(&request, &client_addr, client_addr_len);
    }
    assert(rc == 0);
  }
  return;
}

int main() {
  DMC dmc;
  dmc.setup_server_conf();
  Server server(dmc.server_conf_);
  server.run();
}
