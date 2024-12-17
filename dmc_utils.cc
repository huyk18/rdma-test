#include <cstdint>

#include <arpa/inet.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "dmc_utils.h"

inline static uint64_t htonll(uint64_t val) {
  return (((uint64_t)htonl(val)) << 32) + htonl(val >> 32);
}

inline static uint64_t ntohll(uint64_t val) {
  return (((uint64_t)ntohl(val)) << 32) + ntohl(val >> 32);
}

void serialize_udpmsg(__OUT UDPMsg *msg) {
  switch (msg->type) {
  case UDPMSG_REQ_CONNECT:
  case UDPMSG_REP_CONNECT:
    serialize_conn_info(&msg->body.conn_info);
    break;
  case UDPMSG_REQ_ALLOC:
  case UDPMSG_REP_ALLOC:
    serialize_mr_info(&msg->body.mr_info);
    break;
  case UDPMSG_REQ_TS:
  case UDPMSG_REP_TS:
    msg->body.sys_start_ts = htonll(msg->body.sys_start_ts);
    break;
  default:
    break;
  }
  msg->type = htons(msg->type);
  msg->id = htons(msg->id);
}

void deserialize_udpmsg(__OUT UDPMsg *msg) {
  msg->type = ntohs(msg->type);
  msg->id = ntohs(msg->id);
  switch (msg->type) {
  case UDPMSG_REQ_CONNECT:
  case UDPMSG_REP_CONNECT:
    deserialize_conn_info(&msg->body.conn_info);
    break;
  case UDPMSG_REQ_ALLOC:
  case UDPMSG_REP_ALLOC:
    serialize_mr_info(&msg->body.mr_info);
    break;
  case UDPMSG_REQ_TS:
  case UDPMSG_REP_TS:
    msg->body.sys_start_ts = ntohll(msg->body.sys_start_ts);
    break;
  default:
    break;
  }
}

void serialize_qp_info(__OUT QPInfo *qp_info) {
  qp_info->qp_num = htonl(qp_info->qp_num);
  qp_info->lid = htons(qp_info->lid);
}

void deserialize_qp_info(__OUT QPInfo *qp_info) {
  qp_info->qp_num = ntohl(qp_info->qp_num);
  qp_info->lid = ntohs(qp_info->lid);
}

void serialize_mr_info(__OUT MrInfo *mr_info) {
  mr_info->addr = htonll(mr_info->addr);
  mr_info->rkey = htonl(mr_info->rkey);
}

void deserialize_mr_info(__OUT MrInfo *mr_info) {
  mr_info->addr = ntohll(mr_info->addr);
  mr_info->rkey = ntohl(mr_info->rkey);
}

void serialize_conn_info(__OUT ConnInfo *conn_info) {
  serialize_qp_info(&conn_info->qp_info);
  serialize_mr_info(&conn_info->mr_info);
}

void deserialize_conn_info(__OUT ConnInfo *conn_info) {
  deserialize_qp_info(&conn_info->qp_info);
  deserialize_mr_info(&conn_info->mr_info);
}

int stick_this_thread_to_core(int core_id) {
  int num_cores = sysconf(_SC_NPROCESSORS_CONF);
  if (core_id < 0 || core_id >= num_cores) {
    return -1;
  }

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

static uint64_t sys_start_ts = 0;

void set_sys_start_ts(uint64_t ts) { sys_start_ts = ts; }

uint32_t new_ts32() {
  uint64_t cur_ts = new_ts();
  return (uint32_t)(cur_ts - sys_start_ts);
}
