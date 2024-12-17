#ifndef DMC_TEST_H_
#define DMC_TEST_H_

#include <cstring>

#include "dmc_utils.h"

#define GB (1024ll * 1024 * 1024)
#define MB (1024ll * 1024)
#define KB (1024ll)

class DMC {
public:
  void setup_server_conf() {
    server_conf_.role = SERVER;
    server_conf_.conn_type = ROCE;
    server_conf_.server_id = 0;
    server_conf_.udp_port = 2333;
    server_conf_.memory_num = 1;
    strcpy(server_conf_.memory_ip_list[0], "10.0.0.49");

    server_conf_.ib_dev_id = 0;
    server_conf_.ib_port_id = 1;
    server_conf_.ib_gid_idx = 3;

    server_conf_.server_base_addr = 0x10000000;
    server_conf_.server_data_len = 4ll * GB;
    server_conf_.segment_size = 16ll * MB;
    server_conf_.block_size = 256;

    server_conf_.client_local_size = 64ll * MB;
  }
  void setup_client_conf() {
    client_conf_.role = CLIENT;
    client_conf_.conn_type = ROCE;
    client_conf_.server_id = 1;
    client_conf_.udp_port = 2333;
    client_conf_.memory_num = 1;
    strcpy(client_conf_.memory_ip_list[0], "10.0.0.49");

    client_conf_.ib_dev_id = 1;
    client_conf_.ib_port_id = 1;
    client_conf_.ib_gid_idx = 3;

    client_conf_.server_base_addr = 0x10000000;
    client_conf_.server_data_len = 4ll * GB;
    client_conf_.segment_size = 16ll * MB;
    client_conf_.block_size = 256;

    client_conf_.client_local_size = 1ll * MB;
  }
  DMCConfig server_conf_;
  DMCConfig client_conf_;
};

#endif
