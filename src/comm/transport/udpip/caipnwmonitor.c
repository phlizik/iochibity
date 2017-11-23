/******************************************************************
*
* Copyright 2014 Samsung Electronics All Rights Reserved.
*
*
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
******************************************************************/
#include "caipnwmonitor.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/select.h>
/* #ifdef HAVE_SYS_IFADDRS_H */
#include <ifaddrs.h>
/* #else */
/* #include "ifaddrs.h" 		/\* netlink impl in portability layer *\/ */
/* #endif */
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <errno.h>

#if defined(__linux__) || defined(__ANDROID__) /* FIXM: use HAVE_NETLINK_H etc */
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

/* #include "../caipserver.h" */
/* #include "../caipnwmonitor.h" */
/* #include "octhread.h" */
/* #include "caadapterutils.h" */
/* #include "logger.h" */
/* #include "oic_malloc.h" */
/* #include "oic_string.h" */
/* #include <coap/utlist.h> */
#include "utlist.h"

#define TAG "OIC_CA_IP_MONITOR"

#if INTERFACE
#include <stdint.h>
#endif	/* INTERFACE */

#if EXPORT_INTERFACE
#define INTERFACE_NAME_MAX 16

#include <stddef.h>
/**
 * Callback to be notified when IP adapter network state changes.
 *
 * @param[in]  adapter      Transport adapter.
 * @param[in]  status       Connection status either ::CA_INTERFACE_UP or ::CA_INTERFACE_DOWN.
 * @see CAIPSetConnectionStateChangeCallback() for registration.
 */
typedef void (*CAIPAdapterStateChangeCallback)(CATransportAdapter_t adapter,
                                               CANetworkStatus_t status);

/*
 * Structure for IP address information, to be used to construct a CAEndpoint_t.  The
 * structure name is a misnomer, as there is one entry per address not one per interface.
 * An interface with 4 addresses should result in 4 instances of CAInterface_t.
 */
typedef struct
{
    char name[INTERFACE_NAME_MAX];
    uint32_t index;
    uint32_t flags;
    uint16_t family;
    char addr[MAX_ADDR_STR_SIZE_CA];
} CAInterface_t;		/* GAR: CAIfAddress_t */

typedef struct CAIPCBData_t
{
    struct CAIPCBData_t *next;
    CATransportAdapter_t adapter;
    CAIPAdapterStateChangeCallback callback;
} CAIPCBData_t;
#endif

/*
 * Enable or disable log for network changed event
 */
#define NETWORK_INTERFACE_CHANGED_LOGGING 1

/**
 * Mutex for synchronizing access to cached interface and IP address information.
 */
static oc_mutex g_networkMonitorContextMutex = NULL;

/**
 * Used to storing network interface.
 */
static u_arraylist_t *g_netInterfaceList = NULL;

/**
 * Used to storing adapter changes callback interface.
 */
static struct CAIPCBData_t *g_adapterCallbackList = NULL;

/* /\**
 *  * Remove network interface from list.
 *  *\/
 * static void CARemoveNetworkMonitorList(int ifiindex); */

/**
 * Initialize the network interface monitoring list.
 */
static CAResult_t CAIPInitializeNetworkMonitorList();

/**
 * Destroy the network interface monitoring list.
 */
static void CAIPDestroyNetworkMonitorList();

/**
 * Add new network interface in list.
 */
static CAResult_t CAAddNetworkMonitorList(CAInterface_t *ifitem);

/**
 * Create new interface item.
 */
static CAInterface_t *CANewInterfaceItem(int index, const char *name, int family,
                                         const char *addr, int flags);

static CAResult_t CAIPInitializeNetworkMonitorList()
{
    if (!g_networkMonitorContextMutex)
    {
        g_networkMonitorContextMutex = oc_mutex_new();
        if (!g_networkMonitorContextMutex)
        {
            OIC_LOG(ERROR, TAG, "oc_mutex_new has failed");
            return CA_STATUS_FAILED;
        }
    }

    if (!g_netInterfaceList)
    {
        g_netInterfaceList = u_arraylist_create();
        if (!g_netInterfaceList)
        {
            OIC_LOG(ERROR, TAG, "u_arraylist_create has failed");
            CAIPDestroyNetworkMonitorList();
            return CA_STATUS_FAILED;
        }
    }
    return CA_STATUS_OK;
}

static void CAIPDestroyNetworkMonitorList()
{
    if (g_netInterfaceList)
    {
        u_arraylist_destroy(g_netInterfaceList);
        g_netInterfaceList = NULL;
    }

    if (g_networkMonitorContextMutex)
    {
        oc_mutex_free(g_networkMonitorContextMutex);
        g_networkMonitorContextMutex = NULL;
    }
}

/**
 * Compare the interface with the already added interface in list.
 */
bool CACmpNetworkList(uint32_t ifiindex)
{
#if NETWORK_INTERFACE_CHANGED_LOGGING
    OIC_LOG_V(DEBUG, TAG, "IN %s: ifiindex = %ul", __func__, ifiindex);
#endif
    if (!g_netInterfaceList)
    {
        OIC_LOG(ERROR, TAG, "g_netInterfaceList is NULL");
        return false;
    }

    oc_mutex_lock(g_networkMonitorContextMutex);

    size_t list_length = u_arraylist_length(g_netInterfaceList);
    for (size_t list_index = 0; list_index < list_length; list_index++)
    {
        CAInterface_t *currItem = (CAInterface_t *) u_arraylist_get(g_netInterfaceList,
                                                                    list_index);
        if (currItem->index == ifiindex)
        {
            oc_mutex_unlock(g_networkMonitorContextMutex);
            return true;
        }
    }
    oc_mutex_unlock(g_networkMonitorContextMutex);
    OIC_LOG_V(DEBUG, TAG, "OUT %s", __func__);
    return false;
}

static CAResult_t CAAddNetworkMonitorList(CAInterface_t *ifitem)
{
    OIC_LOG_V(DEBUG, TAG, "IN %s", __func__);
    VERIFY_NON_NULL_MSG(g_netInterfaceList, TAG, "g_netInterfaceList is NULL");
    VERIFY_NON_NULL_MSG(ifitem, TAG, "ifitem is NULL");

    oc_mutex_lock(g_networkMonitorContextMutex);
    bool result = u_arraylist_add(g_netInterfaceList, (void *) ifitem);
    if (!result)
    {
        OIC_LOG(ERROR, TAG, "u_arraylist_add failed.");
        oc_mutex_unlock(g_networkMonitorContextMutex);
        return CA_STATUS_FAILED;
    }
    oc_mutex_unlock(g_networkMonitorContextMutex);
    OIC_LOG_V(DEBUG, TAG, "OUT %s", __func__);
    return CA_STATUS_OK;
}

CAResult_t CAIPStartNetworkMonitor(CAIPAdapterStateChangeCallback callback,
                                   CATransportAdapter_t adapter)
{
    CAResult_t res = CAIPInitializeNetworkMonitorList();
    if (CA_STATUS_OK == res)
    {
        return CAIPSetNetworkMonitorCallback(callback, adapter);
    }
    return res;
}

CAResult_t CAIPStopNetworkMonitor(CATransportAdapter_t adapter)
{
    CAIPDestroyNetworkMonitorList();
    return CAIPUnSetNetworkMonitorCallback(adapter);
}

int CAGetPollingInterval(int interval)
{
    return interval;
}

/**
 * Pass the changed network status through the stored callback.
 */
void CAIPPassNetworkChangesToAdapter(CANetworkStatus_t status)
{
    OIC_LOG_V(DEBUG, TAG, "IN %s: status = %d", __func__, status);
    CAIPCBData_t *cbitem = NULL;
    LL_FOREACH(g_adapterCallbackList, cbitem)
    {
        if (cbitem && cbitem->adapter)
        {
            cbitem->callback(cbitem->adapter, status);
            CALogAdapterStateInfo(cbitem->adapter, status);
        }
    }
    OIC_LOG_V(DEBUG, TAG, "OUT %s", __func__);
}

CAResult_t CAIPSetNetworkMonitorCallback(CAIPAdapterStateChangeCallback callback,
                                         CATransportAdapter_t adapter)
{
    if (!callback)
    {
        OIC_LOG(ERROR, TAG, "callback is null");
        return CA_STATUS_INVALID_PARAM;
    }

    CAIPCBData_t *cbitem = NULL;
    LL_FOREACH(g_adapterCallbackList, cbitem)
    {
        if (cbitem && adapter == cbitem->adapter && callback == cbitem->callback)
        {
            OIC_LOG(DEBUG, TAG, "this callback is already added");
            return CA_STATUS_OK;
        }
    }

    cbitem = (CAIPCBData_t *)OICCalloc(1, sizeof(*cbitem));
    if (!cbitem)
    {
        OIC_LOG(ERROR, TAG, "Malloc failed");
        return CA_STATUS_FAILED;
    }

    cbitem->adapter = adapter;
    cbitem->callback = callback;
    LL_APPEND(g_adapterCallbackList, cbitem);

    return CA_STATUS_OK;
}

CAResult_t CAIPUnSetNetworkMonitorCallback(CATransportAdapter_t adapter)
{
    CAIPCBData_t *cbitem = NULL;
    CAIPCBData_t *tmpCbitem = NULL;
    LL_FOREACH_SAFE(g_adapterCallbackList, cbitem, tmpCbitem)
    {
        if (cbitem && adapter == cbitem->adapter)
        {
            OIC_LOG(DEBUG, TAG, "remove specific callback");
            LL_DELETE(g_adapterCallbackList, cbitem);
            OICFree(cbitem);
            return CA_STATUS_OK;
        }
    }
    return CA_STATUS_OK;
}

static CAInterface_t *CANewInterfaceItem(int index, const char *name, int family,
                                         const char *addr, int flags)
{
    OIC_LOG_V(DEBUG, TAG, "IN %s:"
              "index = %d, name = \"%s\", family = %d, addr = \"%s\", flags = %d",
              __func__, index, name, family, addr, flags);
    CAInterface_t *ifitem = (CAInterface_t *)OICCalloc(1, sizeof (CAInterface_t));
    if (!ifitem)
    {
        OIC_LOG(ERROR, TAG, "Malloc failed");
        return NULL;
    }

    OICStrcpy(ifitem->name, sizeof (ifitem->name), name);
    ifitem->index = index;
    ifitem->family = family;
    OICStrcpy(ifitem->addr, sizeof (ifitem->addr), addr);
    ifitem->flags = flags;

    OIC_LOG_V(DEBUG, TAG, "OUT %s", __func__);
    return ifitem;
}

/* GAR TODO: portable implementation */
/* GAR: called by CASelectReturned */
/* u_arraylist_t *CAFindInterfaceChange()
 * {
 *     u_arraylist_t *iflist = NULL;
 * #if defined(__linux__) || defined (__ANDROID__) /\* GAR:  Darwin support *\/
 *     char buf[4096] = { 0 };
 *     struct nlmsghdr *nh = NULL;
 *     struct sockaddr_nl sa = { .nl_family = 0 };
 *     struct iovec iov = { .iov_base = buf,
 *                          .iov_len = sizeof (buf) };
 *     struct msghdr msg = { .msg_name = (void *)&sa,
 *                           .msg_namelen = sizeof (sa),
 *                           .msg_iov = &iov,
 *                           .msg_iovlen = 1 };
 *
 *     ssize_t len = recvmsg(caglobals.ip.netlinkFd, &msg, 0);
 *
 *     for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len))
 *     {
 *         if (nh != NULL && (nh->nlmsg_type != RTM_DELADDR && nh->nlmsg_type != RTM_NEWADDR))
 *         {
 * 	    /\* GAR: what about RTM_NEWLINK, RTM_DELLINK? *\/
 *             continue;
 *         }
 *
 *         if (RTM_DELADDR == nh->nlmsg_type)
 *         {
 *             struct ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA (nh);
 *             if (ifa)
 *             {
 *                 int ifiIndex = ifa->ifa_index;
 *                 bool isFound = CACmpNetworkList(ifiIndex);
 *                 if (isFound)
 *                 {
 *                     CARemoveNetworkMonitorList(ifiIndex);
 *                     CAIPPassNetworkChangesToAdapter(CA_INTERFACE_DOWN);
 *                 }
 *             }
 *             continue;
 *         }
 *
 *         // Netlink message type is RTM_NEWADDR.
 *         struct ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA (nh);
 *         if (ifa)
 *         {
 *             int ifiIndex = ifa->ifa_index;
 * 	    /\* FIXME: BUG. what if > 1 new addrs? only last will be in iflist *\/
 *             iflist = CAIPGetInterfaceInformation(ifiIndex);
 *             if (!iflist)
 *             {
 *                 OIC_LOG_V(ERROR, TAG, "get interface info failed: %s", strerror(errno));
 *                 return NULL;
 *             }
 * 	    /\* GAR: CAProcessNewInterfaceItem? (android) *\/
 *         }
 *     }
 * #endif
 *     return iflist;
 * } */

CAResult_t CAGetLinkLocalZoneIdInternal(uint32_t ifindex, char **zoneId)
{
    if (!zoneId || (*zoneId != NULL))
    {
        return CA_STATUS_INVALID_PARAM;
    }

    *zoneId = (char *)OICCalloc(IF_NAMESIZE, sizeof(char));
    if (!(*zoneId))
    {
        OIC_LOG(ERROR, TAG, "OICCalloc failed in CAGetLinkLocalZoneIdInternal");
        return CA_MEMORY_ALLOC_FAILED;
    }

    if (!if_indextoname(ifindex, *zoneId))
    {
        OIC_LOG(ERROR, TAG, "if_indextoname failed in CAGetLinkLocalZoneIdInternal");
        OICFree(*zoneId);
        *zoneId = NULL;
        return CA_STATUS_FAILED;
    }

    OIC_LOG_V(DEBUG, TAG, "Given ifindex is %d parsed zoneId is %s", ifindex, *zoneId);
    return CA_STATUS_OK;
}

void CARemoveNetworkMonitorList(int ifiindex)
{
    VERIFY_NON_NULL_VOID(g_netInterfaceList, TAG, "g_netInterfaceList is NULL");

    oc_mutex_lock(g_networkMonitorContextMutex);

    size_t list_length = u_arraylist_length(g_netInterfaceList);
    for (size_t list_index = 0; list_index < list_length; list_index++)
    {
        CAInterface_t *removedifitem = (CAInterface_t *) u_arraylist_get(
                g_netInterfaceList, list_index);
        if (removedifitem && ((int)removedifitem->index) == ifiindex)
        {
            if (u_arraylist_remove(g_netInterfaceList, list_index))
            {
                OICFree(removedifitem);
                oc_mutex_unlock(g_networkMonitorContextMutex);
                return;
            }
            continue;
        }
    }
    oc_mutex_unlock(g_networkMonitorContextMutex);
    return;
}

/* GAR FIXME: CAIPGetAllInterfaceInformation(), not CAIPGetInterfaceInformation(0) */
u_arraylist_t *CAIPGetInterfaceInformation(int desiredIndex)
{
#if NETWORK_INTERFACE_CHANGED_LOGGING
    OIC_LOG_V(DEBUG, TAG, "IN %s: desiredIndex = %d", __func__, desiredIndex);
#endif
    if (desiredIndex < 0)
    {
        OIC_LOG_V(ERROR, TAG, "invalid index : %d", desiredIndex);
        return NULL;
    }

    u_arraylist_t *iflist = u_arraylist_create();
    if (!iflist)
    {
        OIC_LOG_V(ERROR, TAG, "Failed to create iflist: %s", strerror(errno));
        return NULL;
    }

    struct ifaddrs *ifp = NULL;
    errno = 0;
     int r = 0;
    r = getifaddrs(&ifp);
     if (-1 == r)
    {
        OIC_LOG_V(ERROR, TAG, "Failed to get ifaddrs: %s", strerror(errno));
        u_arraylist_destroy(iflist);
        return NULL;
    }

    struct ifaddrs *ifa = NULL;
#if NETWORK_INTERFACE_CHANGED_LOGGING
    OIC_LOG(DEBUG, TAG, "Iterating over interface addresses.");
#endif
    for (ifa = ifp; ifa; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
        {
            continue;
        }
        int family = ifa->ifa_addr->sa_family;
        if ((ifa->ifa_flags & IFF_LOOPBACK) || (AF_INET != family && AF_INET6 != family))
        {
            continue;
        }

	/* GAR: hidden semantics: if desiredIndex == 0, then every if will be added to list */
        int ifindex = if_nametoindex(ifa->ifa_name);
        if (desiredIndex && (ifindex != desiredIndex))
        {
            continue;
        }

        size_t length = u_arraylist_length(iflist);
        int already = false;
#if NETWORK_INTERFACE_CHANGED_LOGGING
        OIC_LOG_V(DEBUG, TAG, "Iterating over %d interfaces.", length); /*  " PRIuPTR " */
#endif
        for (size_t i = 0; i < length; i++)
        {
#if NETWORK_INTERFACE_CHANGED_LOGGING
            OIC_LOG_V(DEBUG, TAG, "Checking interface %d.", i); /* " PRIuPTR " */
#endif
            CAInterface_t *ifitem = (CAInterface_t *)u_arraylist_get(iflist, i);

            if (ifitem
                && (int)ifitem->index == ifindex
                && ifitem->family == (uint16_t)family)
            {
                already = true;
                break;
            }
        }
        if (already)
        {
            continue;
        }
	/* FIXME: CAInterface_t => CAIfAddress_t */
        CAInterface_t *ifitem = (CAInterface_t *)OICCalloc(1, sizeof(CAInterface_t));
        if (!ifitem)
        {
            OIC_LOG(ERROR, TAG, "Malloc failed");
            goto exit;
        }

        OICStrcpy(ifitem->name, INTERFACE_NAME_MAX, ifa->ifa_name);
        ifitem->index = ifindex;
        ifitem->family = family;
        ifitem->flags = ifa->ifa_flags;

        if (ifitem->family == AF_INET6)
        {
            struct sockaddr_in6 *in6 = (struct sockaddr_in6*) ifa->ifa_addr;
            inet_ntop(ifitem->family, (void *)&(in6->sin6_addr), ifitem->addr,
                      sizeof(ifitem->addr));
        }
        else if (ifitem->family == AF_INET)
        {
            struct sockaddr_in *in = (struct sockaddr_in*) ifa->ifa_addr;
            inet_ntop(ifitem->family, (void *)&(in->sin_addr), ifitem->addr,
                      sizeof(ifitem->addr));
        }

        bool result = u_arraylist_add(iflist, ifitem);
        if (!result)
        {
            OIC_LOG(ERROR, TAG, "u_arraylist_add failed.");
	    /* FIXME: free ifitem? */
            goto exit;
        }

        bool isFound = CACmpNetworkList(ifitem->index);
        if (!isFound)
        {
	    /* GAR: use DupIfItem(ifitem) instead of NewIfItem, to clarify sense */
            CAInterface_t *newifitem = CANewInterfaceItem(ifitem->index, ifitem->name, ifitem->family,
                                                          ifitem->addr, ifitem->flags);
            CAResult_t ret = CAAddNetworkMonitorList(newifitem);
            if (CA_STATUS_OK != ret)
            {
                OICFree(newifitem);
                goto exit;
            }
            CAIPPassNetworkChangesToAdapter(CA_INTERFACE_UP);
            OIC_LOG_V(DEBUG, TAG, "Added interface: %s (%d)", ifitem->name, ifitem->family);
        }
    }
    freeifaddrs(ifp);
#if NETWORK_INTERFACE_CHANGED_LOGGING
    OIC_LOG_V(DEBUG, TAG, "OUT %s", __func__);
#endif
    return iflist;

exit:
    freeifaddrs(ifp);
    u_arraylist_destroy(iflist);
#if NETWORK_INTERFACE_CHANGED_LOGGING
    OIC_LOG_V(DEBUG, TAG, "OUT %s", __func__);
#endif
    return NULL;
}