
/* NAT state table -- store socket mappings
 * state_table_init     initializes an empty state table with the external host IP
 * state_table_create   creates a new NAT mapping for the source socket and return it
 * state_table_get      gets a NAT mapping for source socket or NULL if it doesn't exists
 * state_table_remove   removes a NAT maping for source socket
 */

#ifndef STATE_TABLE_H
#define STATE_TABLE_H

#include <stdint.h>

typedef struct {
  uint32_t addr;
  uint16_t port;
} socket;

void state_table_init(uint32_t ip, uint16_t port_low, uint16_t, port_high);
socket state_table_create(socket source);
socket state_table_get(socket source);
void state_table_remove(socket source);

#endif
