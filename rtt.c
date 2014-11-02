/* include rtt1 */
#include	"unprtt.h"

int		rtt_d_flag = 0;		/* debug flag; can be set by caller */

/*
 * Calculate the RTO value based on current estimators:
 *		smoothed RTT plus four times the deviation
 */
//#define	RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (4 * (ptr)->rtt_rttvar))
int calcRto(struct rtt_info * ptr) {
	return (ptr)->rtt_srtt + (4 * (ptr)->rtt_rttvar);
}
static int rtt_minmax(int rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return(rto);
}

void rtt_init(struct rtt_info *ptr)
{
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ptr->rtt_base = tv.tv_sec*RTT_SCALE;		/* # sec since 1/1/1970 at start */

	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 0.75 * RTT_SCALE;
	ptr->rtt_rto = rtt_minmax(calcRto(ptr));
	/* first RTO at (srtt + (4 * rttvar)) = 3 seconds */
}
/* end rtt1 */

/*
 * Return the current timestamp.
 * Our timestamps are 32-bit integers that count milliseconds since
 * rtt_init() was called.
 */

/* include rtt_ts */
int rtt_ts(struct rtt_info *ptr)
{
	int		ts;
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ts = (tv.tv_sec*RTT_SCALE - ptr->rtt_base) + (tv.tv_usec / 1000);
	return(ts);
}

void rtt_newpack(struct rtt_info *ptr)
{
	ptr->rtt_nrexmt = 0;
}

int rtt_start(struct rtt_info *ptr)
{
	//return((int) (ptr->rtt_rto + 0.5));		/* round float to int */
	return ptr->rtt_rto;
	/* 4return value can be used as: alarm(rtt_start(&foo)) */
}
/* end rtt_ts */

/*
 * A response was received.
 * Stop the timer and update the appropriate values in the structure
 * based on this packet's RTT.  We calculate the RTT, then update the
 * estimators of the RTT and its mean deviation.
 * This function should be called right after turning off the
 * timer with alarm(0), or right after a timeout occurs.
 */

/* include rtt_stop */
void rtt_stop(struct rtt_info *ptr, int ms)
{
	int delta;

	ptr->rtt_rtt = ms;		/* measured RTT in milliseconds */
	printf("rtt=%u\n",ptr->rtt_rtt);

	/*
	 * Update our estimators of RTT and mean deviation of RTT.
	 * See Jacobson's SIGCOMM '88 paper, Appendix A, for the details.
	 * We use floating point here for simplicity.
	 */

	delta = ptr->rtt_rtt - ptr->rtt_srtt;
	ptr->rtt_srtt = ptr->rtt_srtt + delta / 8;		/* g = 1/8 */
//printf("srtt=%d\n",ptr->rtt_srtt);
	if (delta < 0.0)
		delta = -delta;				/* |delta| */
//printf("delta=%d\n", delta);
//printf("rttvar=%d\n",ptr->rtt_rttvar);
	int diff = delta - ptr->rtt_rttvar;
	ptr->rtt_rttvar = ptr->rtt_rttvar + diff/4;	/* h = 1/4 */
	//ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4;	/* h = 1/4 */
//printf("rttvar=%d\n",ptr->rtt_rttvar);
	int x = calcRto(ptr);
	
	ptr->rtt_rto = rtt_minmax(x);
//	printf("RTT stop, calc rto=%d, limto=%d\n", x, ptr->rtt_rto);
}
/* end rtt_stop */

/*
 * A timeout has occurred.
 * Return -1 if it's time to give up, else return 0.
 */

/* include rtt_timeout */
int rtt_timeout(struct rtt_info *ptr, ServerBufferNode* sbn)
{
	ptr->rtt_rto = ptr->rtt_rto*2;		/* next RTO */
	ptr->rtt_rto = rtt_minmax(ptr->rtt_rto);
	if (++sbn->retransNumber > RTT_MAXNREXMT)
		return(-1);			/* time to give up for this packet */
	return(0);
}
/* end rtt_timeout */

/*
 * Print debugging information on stderr, if the "rtt_d_flag" is nonzero.
 */

void rtt_debug(struct rtt_info *ptr)
{
	if (rtt_d_flag == 0)
		return;

	/*fprintf(stderr, "rtt = %.3f, srtt = %.3f, rttvar = %.3f, rto = %.3f\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto); */
	fprintf(stderr, "rtt = %u, srtt = %u, rttvar = %u, rto = %u\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stderr);
}

void rtt_debug2(struct rtt_info *ptr, char* s)
{
	if (rtt_d_flag == 0)
		return;

	/*fprintf(stderr, "rtt = %.3f, srtt = %.3f, rttvar = %.3f, rto = %.3f\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto); */
	fprintf(stderr, "%s rtt = %u, srtt = %u, rttvar = %u, rto = %u\n",
			s, ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stderr);
}