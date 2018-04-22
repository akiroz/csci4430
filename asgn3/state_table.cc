#include <cstdio>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include "state_table.h"

using namespace std;

// pool of open ports -- min-heap
priority_queue< uint16_t, vector<uint16_t>, greater<uint16_t> > pool;

uint16_t port_alloc_get() {
  uint16_t port = pool.top();
  pool.pop();
  return port;
}

void port_alloc_free(uint16_t port) {
  pool.push(port);
}

// state table -- hash map
unordered_map<socket, socket> state;

// host ip -- for creating new mappings
uint32_t host_ip;

void print_state() {
  for(const auto& n : state) {
    printf("=== State Table ===============\n");
    // TODO: print states
  }
}

void state_table_init(
    uint32_t ip,
    uint16_t port_low,
    uint16_t port_high)
{
  // init port allocation pool
  for(uint16_t i = port_low; i <= port_high; i++) pool.push(i);
  host_ip = ip;
}

socket state_table_create(socket source) {
  socket dest = { .addr = host_ip, .port = port_alloc_get() };
  state[source] = dest;
  print_state();
  return dest;
}

socket state_table_get(socket source) {
  return state[source];
}

void state_table_remove(socket source) {
  port_alloc_free(state[source].port);
  state.erase(source);
  print_state();
}

