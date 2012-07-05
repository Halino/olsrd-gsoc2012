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

int ENTRYTTL = 120;
struct list_entity ROUTER_ID;
struct list_entity ROUTER_ID6;
#define ROUTER_ID_ENTRIES(n, iterator) list_for_each_element_safe(&ROUTER_ID, n, list, iterator)
#define ROUTER_ID6_ENTRIES(n6, iterator) list_for_each_element_safe(&ROUTER_ID6, n6, list, iterator)

//List for routers
struct list_entity ListOfRouter;
#define ROUTER_ELECTION_ENTRIES(nr, iterator) list_for_each_element_safe(&ListOfRouter, nr, list, iterator)

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

  struct RouterListEntry *tmp, *iterator;
  int exist = 0;

  if (olsr_cnf->ip_version == AF_INET6)		//mdns plugin is running in ipv4, discard ipv6
    return 0;

  ROUTER_ELECTION_ENTRIES(tmp, iterator) {
    if((memcmp(&(listEntry->router_id), &(tmp->router_id), sizeof(struct in_addr)) == 0) &&
		(memcmp(&(listEntry->network_id), &(tmp->network_id), sizeof(struct in_addr)) == 0)){
      exist = 1;
      tmp->ttl = listEntry->ttl;
    }
  }
    if (exist == 0)
      list_add_tail(&ListOfRouter, &(listEntry->list));
  return 0;
}

int UpdateRouterList6 (struct RouterListEntry6 *listEntry6){

  struct RouterListEntry6 *tmp, *iterator;
  int exist = 0;

  if (olsr_cnf->ip_version == AF_INET)		//mdns plugin is running in ipv6, discard ipv4
    return 0;
 
  ROUTER_ELECTION_ENTRIES(tmp, iterator) { 
    if((memcmp(&(listEntry6->router_id), &(tmp->router_id), sizeof(struct in6_addr)) == 0) &&
              (memcmp(&(listEntry6->network_id), &(tmp->network_id), sizeof(struct in6_addr)) == 0)){
      exist = 1;
      tmp->ttl = listEntry6->ttl;
    }
  }
    if (exist == 0)
      list_add_tail(&ListOfRouter, &(listEntry6->list));
  return 0;
}

void electTimer (void *x __attribute__ ((unused))){

  struct RouterListEntry *tmp, *iterator, *tmp2, *iterator2;
  struct RouterListEntry6 *tmp6, *iterator6, *tmp62, *iterator62 ;

  if (list_is_empty(&ListOfRouter)){
    ISMASTER = 1;
    return;
  }

  ISMASTER = 1;
  if (olsr_cnf->ip_version == AF_INET) {
    ROUTER_ELECTION_ENTRIES(tmp, iterator){
      ROUTER_ID_ENTRIES(tmp2, iterator2){
        if(memcmp(&tmp->network_id.s_addr, &tmp2->network_id.s:addr, sizeof(unsigned long)) == 0)
          if(memcmp(&tmp->router_id.s_addr, &tmp2->router_id.s_addr, sizeof(unsigned long)) < 0)
            ISMASTER = 0;
      }
    }
  }
  else{
    ROUTER_ELECTION_ENTRIES(tmp6, iterator6){
      ROUTER_ID_ENTRIES(tmp62, iterator62){
        if(memcmp(&tmp6->network_id, &tmp62->network_id, sizeof(struct in6_addr)) == 0)
          if(memcmp(&tmp6->router_id, &tmp62->router_id, sizeof(struct in6_addr)) < 0)
            ISMASTER = 0;
      }
    }
  }

}

int InitRouterList(){

  struct RouterListEntry *selfEntry;
  struct RouterListEntry6 *selfEntry6;
  struct ip_prefix_entry *h, *h_iterator;
  struct olsr_timer_entry *RouterElectionTimer;
  struct olsr_timer_info *RouterElectionTimer_cookie = NULL;

  list_init_head(&ListOfRouter);
  list_init_head(&ROUTER_ID);
  list_init_head(&ROUTER_ID6);

  OLSR_FOR_ALL_IPPREFIX_ENTRIES(&olsr_cnf->hna_entries, h, h_iterator) {
    if(olsr_cnf->ip_version == AF_INET){
      selfEntry = (ListEntry *) malloc(sizeof(struct RouterListEntry));
      memcpy(h->net.prefix.v4, selfEntry->network_id, sizeof(struct in_addr));
      memcpy(olsr_cnf->router_id.v4, selfEntry->router_id, sizeof(struct in_addr));
      list_add_tail(&ROUTER_ID, &selfEntry->list);
    }
    else{
    selfEntry6 = (ListEntry6 *) malloc(sizeof(struct RouterListEntry6));
    memcpy(h->net.prefix.v6, selfEntry6->network_id, sizeof(struct in6_addr));
    memcpy(olsr_cnf->router_id.v6, selfEntry6->router_id, sizeof(struct in6_addr));
    list_add_tail(&ROUTER_ID6, &selfEntry6->list);
    }
  }

  RouterElectionTimer_cookie =
        olsr_timer_add("Router Election Timer", &electTimer, true);
  RouterElectionTimer =
        olsr_timer_start(ELECTION_TIMER * MSEC_PER_SEC, ELECTION_JITTER, NULL,
                         RouterElectionTimer_cookie);

  return 0;
}
