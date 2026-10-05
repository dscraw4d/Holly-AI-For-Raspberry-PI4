#ifndef HOLLY_DASHBOARD_H
#define HOLLY_DASHBOARD_H
#include "face.h"
/* HDMI face only. Legacy public status API is inert; web status is separate. */
void holly_dashboard_avatar(unsigned hilly);
void holly_dashboard_status(int network,unsigned lease,const uint8_t ip[4],
                            unsigned vault,unsigned documents);
unsigned holly_dashboard_revision(void);
int holly_dashboard_render(uint32_t *,unsigned,unsigned,unsigned,size_t,
                            enum holly_expression,unsigned);
unsigned holly_dashboard_frame(enum holly_expression,unsigned);
int holly_dashboard_render16(uint16_t *,unsigned,unsigned,unsigned,size_t,
                            enum holly_expression,unsigned,unsigned);
#endif
