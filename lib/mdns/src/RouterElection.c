/* System includes */
#include <stddef.h>             /* NULL */
#include <sys/types.h>          /* ssize_t */
#include <string.h>             /* strerror() */
#include <stdarg.h>             /* va_list, va_start, va_end */
#include <errno.h>              /* errno */
#include <assert.h>             /* assert() */
#include <linux/if_ether.h>     /* ETH_P_IP */
#include <linux/if_packet.h>    /* struct sockaddr_ll, PACKET_MULTICAST */
//#include <pthread.h> /* pthread_t, pthread_create() */
#include <signal.h>             /* sigset_t, sigfillset(), sigdelset(), SIGINT */
#include <netinet/ip.h>         /* struct ip */
#include <netinet/udp.h>        /* struct udphdr */
#include <unistd.h>             /* close() */

#include <netinet/in.h>
#include <netinet/ip6.h>

/* OLSRD includes */
#include "plugin_util.h"        /* set_plugin_int */
#include "defs.h"               /* olsr_cnf, //OLSR_PRINTF */
#include "ipcalc.h"
#include "olsr.h"               /* //OLSR_PRINTF */
#include "mid_set.h"            /* mid_lookup_main_addr() */
#include "link_set.h"           /* get_best_link_to_neighbor() */
#include "net_olsr.h"           /* ipequal */

/* plugin includes */
#include "NetworkInterfaces.h"  /* TBmfInterface, CreateBmfNetworkInterfaces(), CloseBmfNetworkInterfaces() */
#include "Address.h"            /* IsMulticast() */
#include "Packet.h"             /* ENCAP_HDR_LEN, BMF_ENCAP_TYPE, BMF_ENCAP_LEN etc. */
#include "list_backport.h"
#include "RouterElection.h"
#include "mdns.h"
#include "list_backport.h"

int ENTRYTTL = 120;
int ISMASTER = 1;
struct list_entity ROUTER_ID;
struct list_entity ROUTER_ID6;
#define ROUTER_ID_ENTRIES(n, iterator) list_for_each_element_safe(&ROUTER_ID, n, list, iterator)
#define ROUTER_ID6_ENTRIES(n, iterator) list_for_each_element_safe(&ROUTER_ID6, n, list, iterator)

//List for routers
struct list_entity ListOfRouter;

int ParseElectionPacket (struct RtElHelloPkt *rcvPkt, struct RouterListEntry *listEntry){

 (void) memset (&listEntry, 0, sizeof(struct RouterListEntry));
 if(inet_pton( AF_INET, &rcvPkt->router_id, &listEntry->router_id) && 
			inet_pton( AF_INET, &rcvPkt->network_id, &listEntry->network_id)){
    listEntry->ttl = ENTRYTTL;
    return 1;
 }
 else
   return 0;			//if packet is not valid return 0
}

int ParseElectionPacket6 (struct RtElHelloPkt *rcvPkt, struct RouterListEntry6 *listEntry6){

  (void) memset (&listEntry6, 0, sizeof(struct RouterListEntry6));
  if(inet_pton( AF_INET6, &rcvPkt->router_id, &listEntry6->router_id) && 
                         inet_pton( AF_INET6, &rcvPkt->network_id, &listEntry6->network_id)){
    listEntry6->ttl = ENTRYTTL;
    return 1;
  }
  else
    return 0;                    //if packet is not valid return 0
}

int UpdateRouterList (struct RouterListEntry *listEntry){

  struct ListEntry *tmp, *iterator;
  int exist = 0;

  if (olsr_cnf->ip_version == AF_INET6)		//mdns plugin is running in ipv4, discard ipv6
    return;

  ROUTER_ELECTION_ENTRIES(tmp, iterator)
    if((memcmp(&(listEntry->router_id), &(tmp->router_id), sizeof(struct in_addr)) != 0) &&
		(memcmp(&(listEntry->network_id), &(tmp->network_id), sizeof(struct in_addr)) != 0)){
      exist = 1;
      tmp->ttl = listEntry->ttl;
    }

    if (exist == 0)
      list_add_tail(&ListOfRouter, &(listEntry->list));
  return;
}

int UpdateRouterList6 (struct RouterListEntry6 *listEntry6){

  struct ListEntry6 *tmp, *iterator;
  int exist = 0;

  if (olsr_cnf->ip_version == AF_INET)		//mdns plugin is running in ipv6, discard ipv4
    return;
 
  ROUTER_ELECTION_ENTRIES(tmp, iterator) 
    if((memcmp(&(listEntry6->router_id), &(tmp->router_id), sizeof(struct in6_addr)) != 0) &&
              (memcmp(&(listEntry6->network_id), &(tmp->network_id), sizeof(struct in6_addr)) != 0)){
      exist = 1;
      tmp->ttl = listEntry6->ttl;
    }

    if (exist == 0)
      list_add_tail(&ListOfRouter, &(listEntry6->list));
  return;
}

void electTimer (void *x __attribute__ ((unused))){

  struct ListEntry *tmp, *iterator;

  if (list_is_empty(&ListOfRouter)){
  ISMASTER = 1;
  return;
  }

  if (olsr_cnf->ip_version == AF_INET) {
    ROUTER_ELECTION_ENTRIES(tmp, iterator){
    if(memcmp(
    }
  }
  else{
  
  }

}

int InitRouterList(){

  struct ListEntry *selfEntry;
  struct ListEntry6 *selfEntry6;
  struct sockaddr_in selfSock;
  struct sockaddr_in6 selSock6;
  struct TBmfInterface *walker;
  struct ip_prefix_entry *h, *h_iterator;
  walker = BmfInterfaces;
  h = &olsr_cnf->hna_entries;

  struct olsr_timer_entry *RouterElectionTimer;
  struct olsr_timer_info *RouterElectionTimer_cookie = NULL;

  list_init_head(&ListOfRouter);

  RouterElectionTimer_cookie =
	olsr_timer_add("Router Election Timer", &electTimer, true);
  RouterElectionTimer =
	olsr_timer_start(ELECTION_TIMER * MSEC_PER_SEC, ELECTION_JITTER, NULL,
			 RouterElectionTimer_cookie);

  if(olsr_cnf->ip_version == AF_INET){
    while(walker != NULL){
      selfEntry = (ListEntry *) malloc(sizeof(struct ListEntry));
      
  }

  return 0;
}
