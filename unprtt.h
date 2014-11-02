#ifndef	__unp_rtt_h
#define	__unp_rtt_h

#include	"unp.h"
#include "dtghdr.h"

//TODO: remove later
typedef struct ServerBufferNode ServerBufferNode;//typeDef for the Clinet Info object
struct ServerBufferNode{
    int occupied;//Process ID used to remove from the DS when server is done serving client
    uint32_t  seq;
    uint32_t ackCount;
    uint32_t ts;
    int retransNumber;
    MsgHdr * dataPayload;
    ServerBufferNode *right;
    ServerBufferNode *left;
};


struct rtt_info {
  int		rtt_rtt;
  int		rtt_srtt;
  int		rtt_rttvar;	
  int		rtt_rto;	
  int		rtt_nrexmt;	
  long 		rtt_base;	
};

#define RTT_SCALE 		1000 /* We use the millisecond scale */

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in milliseconds */
#define	RTT_RXTMAX      3000	/* max retransmit timeout value, in milliseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

				/* function prototypes */
void	 rtt_debug(struct rtt_info *);
void	 rtt_debug2(struct rtt_info *, char* s);
void	 rtt_init(struct rtt_info *);
void	 rtt_newpack(struct rtt_info *);
int		 rtt_start(struct rtt_info *);
void	 rtt_stop(struct rtt_info *, int);
int		 rtt_timeout(struct rtt_info *, ServerBufferNode* );
int rtt_ts(struct rtt_info *);

extern int	rtt_d_flag;	/* can be set to nonzero for addl info */

#endif	/* __unp_rtt_h */
