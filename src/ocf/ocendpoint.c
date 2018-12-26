/* ****************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
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

#include "ocendpoint.h"

#include <errno.h>
#include <string.h>

#define TAG  "OIC_RI_ENDPOINT"

/**
 * Endpoint information for connectivities.
 * Must be identical to OCDevAddr.
 */
#if EXPORT_INTERFACE
#include <stdint.h>
#include <stdio.h>
/* src: cacommon.h */
typedef struct /* added: */ CAEndpoint_s
{
    CATransportAdapter_t    adapter;    // adapter type
    CATransportFlags_t      flags;      // transport modifiers
    uint16_t                port;       // for IP
    char                    addr[MAX_ADDR_STR_SIZE_CA]; // address for all
    uint32_t                ifindex;    // usually zero for default interface
    char                    remoteId[CA_MAX_IDENTITY_SIZE]; // device ID of remote device
#if defined (ROUTING_GATEWAY) || defined (ROUTING_EP)
    char                    routeData[MAX_ADDR_STR_SIZE_CA]; /**< GatewayId:ClientId of
                                                                    destination. **/
#endif
} CAEndpoint_t; // @rewrite -> OCLocalEndpoint

#define CA_SECURE_ENDPOINT_PUBLIC_KEY_MAX_LENGTH    (512)
#endif	/* EXPORT_INTERFACE */

/**
 * Endpoint information for secure messages.
 */
#if EXPORT_INTERFACE
typedef struct
{
    CAEndpoint_t endpoint;      /**< endpoint */
    // TODO change name to deviceId
    CARemoteId_t identity;      /**< endpoint device uuid */
    CARemoteId_t userId;        /**< endpoint user uuid */
    uint32_t attributes;
    uint8_t publicKey[CA_SECURE_ENDPOINT_PUBLIC_KEY_MAX_LENGTH]; /**< Peer's DER-encoded public key (if using certificate) */
    size_t publicKeyLength;     /**< Length of publicKey; zero if not using certificate */
} CASecureEndpoint_t;

/**
 * Endpoint used for security administration - a special type of identity that
 * bypasses Access Control Entry checks for SVR resources, while the device is
 * not ready for normal operation yet.
 */
#define CA_SECURE_ENDPOINT_ATTRIBUTE_ADMINISTRATOR  0x1

/**
 *Maximum length of the remoteEndpoint identity.
 */
#define CA_MAX_ENDPOINT_IDENTITY_LEN  CA_MAX_IDENTITY_SIZE

/**
 * Max identity size.
 */
#define CA_MAX_IDENTITY_SIZE (37)

/* src: ocendpoint.h */
#define OC_MAX_TPS_STR_SIZE          (12)
#define OC_MAX_ADDR_STR_SIZE         (46)
#define OC_MAX_PORT_STR_SIZE         (6)
#define OC_ENDPOINT_TPS_TOKEN        "://"
#define OC_ENDPOINT_ADDR_TOKEN       ':'
#define OC_ENDPOINT_BRACKET_START     '['
#define OC_ENDPOINT_BRACKET_END       ']'
#define OC_ENDPOINT_IPV4_NULL_TOKEN '\0'
#define OC_ENDPOINT_IPV4_SEGMENT_TOKEN '.'
#define OC_ENDPOINT_IPV4_MIN_VALUE '0'
#define OC_ENDPOINT_IPV4_MAX_VALUE '9'

#define DEFAULT_ENDPOINT_PRI         (1)

#define COAP_STR                     "coap"
#define COAPS_STR                    "coaps"
#ifdef TCP_ADAPTER
#define COAP_TCP_STR                 "coap+tcp"
#define COAPS_TCP_STR                "coaps+tcp"
#endif
#ifdef HTTP_ADAPTER
#define HTTP_STR                     "http"
#define HTTPS_STR                    "https"
#endif
#ifdef EDR_ADAPTER
#define COAP_RFCOMM_STR              "coap+rfcomm"
#endif
#endif	/* EXPORT_INTERFACE */

#define mkhrd_ep_rp OCResourceProperty /* help makeheaders */

// default is to create resources with all TPS schemes
// FIXME: configure at compile time, so all enabled TPS schemes always used
OCStackResult OCGetSupportedEndpointFlags(const OCTpsSchemeFlags givenFlags, OCTpsSchemeFlags* out)
{
    if (!out)
    {
        return OC_STACK_INVALID_PARAM;
    }

#ifndef DISABLE_UDP
    //CATransportAdapter_t SelectedNetwork = CAGetSelectedNetwork();
    if (givenFlags & (OC_COAP | OC_COAPS)) // && (SelectedNetwork & CA_ADAPTER_IP))
    {
        *out = (OCTpsSchemeFlags)(*out | OC_COAP);

        if (OC_SECURE)
        {
            *out = (OCTpsSchemeFlags)(*out | OC_COAPS);
        }
    }
#endif
#ifdef TCP_ADAPTER
    if (givenFlags & (OC_COAP_TCP | OC_COAPS_TCP)) // && (SelectedNetwork & CA_ADAPTER_TCP))
    {

        *out = (OCTpsSchemeFlags)(*out | OC_COAP_TCP);

        if (OC_SECURE)
        {
            *out = (OCTpsSchemeFlags)(*out | OC_COAPS_TCP);
        }
    }
#endif
#ifdef EDR_ADAPTER
    if (givenFlags & OC_COAP_RFCOMM) //&& (SelectedNetwork & CA_ADAPTER_RFCOMM_BTEDR))
    {
        *out = (OCTpsSchemeFlags)(*out | OC_COAP_RFCOMM);
    }
#endif
#ifdef LE_ADAPTER
    if (givenFlags & OC_COAP_GATT) // && (SelectedNetwork & CA_ADAPTER_GATT_BTLE))
    {
        *out = (OCTpsSchemeFlags)(*out | OC_COAP_GATT);
    }
#endif
#ifdef NFC_ADAPTER
    if (givenFlags & OC_COAP_NFC) // && (SelectedNetwork & CA_ADAPTER_NFC))
    {
        *out = (OCTpsSchemeFlags)(*out | OC_COAP_NFC);
    }
#endif
#ifdef RA_ADAPTER
    if (givenFlags & OC_COAP_RA) // && (SelectedNetwork & CA_ADAPTER_REMOTE_ACCESS))
    {
        *out = (OCTpsSchemeFlags)(*out | OC_COAP_RA);
    }
#endif

    return OC_STACK_OK;
}

OCStackResult OCGetMatchedTpsFlags(const CATransportAdapter_t adapter,
                                   const CATransportFlags_t flags,
                                   OCTpsSchemeFlags* out)
{
    OIC_LOG_V(INFO, TAG, "%s ENTRY", __func__);
    // return matched TPS Flags with given CA flags.
    if (!out)
    {
        OIC_LOG(ERROR, TAG, "out param is NULL!!!");
        return OC_STACK_INVALID_PARAM;
    }

    if ((adapter & OC_ADAPTER_IP) && (flags & (OC_IP_USE_V4 | OC_IP_USE_V6)))
    {
        if (flags & OC_FLAG_SECURE)
        {
            // OC_COAPS
            // typecasting to support C90(arduino)
            *out = (OCTpsSchemeFlags)(*out | OC_COAPS);
        }
        else
        {   // OC_COAP
            // typecasting to support C90(arduino)
            *out = (OCTpsSchemeFlags)(*out | OC_COAP);
        }
    }

#ifdef TCP_ADAPTER
    if ((adapter & OC_ADAPTER_TCP) && (flags & (OC_IP_USE_V4 | OC_IP_USE_V6)))
    {
        if (flags & OC_FLAG_SECURE)
        {
            // OC_COAPS_TCP
            // typecasting to support C90(arduino)
            *out = (OCTpsSchemeFlags)(*out | OC_COAPS_TCP);
        }
        else
        {
            // OC_COAP_TCP
            // typecasting to support C90(arduino)
            *out = (OCTpsSchemeFlags)(*out | OC_COAP_TCP);
        }
    }
#endif
#ifdef HTTP_ADAPTER
    // OC_HTTP
    // @todo iotivity not support http transport yet...

    // OC_HTTPS
    // @todo iotivity not support https transport yet...
#endif
#ifdef EDR_ADAPTER
    // OC_COAP_RFCOMM
    if (adapter & OC_ADAPTER_RFCOMM_BTEDR)
    {
        // typecasting to support C90(arduino)
        *out = (OCTpsSchemeFlags)(*out | OC_COAP_RFCOMM);
    }
#endif
#ifdef LE_ADAPTER
    // OC_COAP_GATT
    if (adapter & CA_ADAPTER_GATT_BTLE)
    {
        // typecasting to support C90(arduino)
        *out = (OCTpsSchemeFlags)(*out | OC_COAP_GATT);
    }
#endif
#ifdef NFC_ADAPTER
    // OC_COAP_NFC
    if (adapter & CA_ADAPTER_NFC)
    {
        // typecasting to support C90(arduino)
        *out = (OCTpsSchemeFlags)(*out | OC_COAP_NFC);
    }
#endif
#ifdef RA_ADAPTER
    // OC_COAP_RA
    if (adapter & CA_ADAPTER_REMOTE_ACCESS)
    {
        // typecasting to support C90(arduino)
        *out = (OCTpsSchemeFlags)(*out | OC_COAP_RA);
    }
#endif
    return OC_STACK_OK;
}

static const char *ConvertTpsToString(const OCTpsSchemeFlags tps)
{
    switch (tps)
    {
        case OC_COAP:
            return COAP_STR;

        case OC_COAPS:
            return COAPS_STR;
#ifdef TCP_ADAPTER
        case OC_COAP_TCP:
            return COAP_TCP_STR;

        case OC_COAPS_TCP:
            return COAPS_TCP_STR;
#endif
#ifdef HTTP_ADAPTER
        case OC_HTTP:
            return HTTP_STR;

        case OC_HTTPS:
            return HTTPS_STR;
#endif
#ifdef EDR_ADAPTER
        case OC_COAP_RFCOMM:
            return COAP_RFCOMM_STR;
#endif
        default:
            return NULL;
    }
}

OCStackResult OCConvertTpsToString(const OCTpsSchemeFlags tps, char** out)
{
    // return given tps as string for payload
    // OC_COAP_IPV4 -> OC_COAP

    if (!out)
    {
        return OC_STACK_INVALID_PARAM;
    }

    *out = OICStrdup(ConvertTpsToString(tps));
    VERIFY_NON_NULL_1(*out);
    return OC_STACK_OK;

exit:
    return OC_STACK_NO_MEMORY;
}

char* OCCreateEndpointString(const OCEndpointPayload* endpoint)
{
    OIC_LOG_V(DEBUG, TAG, "%s ENTRY", __func__);
    if (!endpoint)
    {
	OIC_LOG_V(DEBUG, TAG, "%s ep is null", __func__);
        return NULL;
    }

    char* buf = (char*)OICCalloc(MAX_ADDR_STR_SIZE, sizeof(char));
    VERIFY_NON_NULL_1(buf);

    if ((strcmp(endpoint->tps, COAP_STR) == 0) || (strcmp(endpoint->tps, COAPS_STR) == 0)
#ifdef TCP_ADAPTER
        || (strcmp(endpoint->tps, COAP_TCP_STR) == 0) ||(strcmp(endpoint->tps, COAPS_TCP_STR) == 0)
#endif
#ifdef HTTP_ADAPTER
        || (strcmp(endpoint->tps, HTTP_STR) == 0) ||(strcmp(endpoint->tps, HTTPS_STR) == 0)
#endif
        )
    {
        // checking addr is ipv4 or not
        if (endpoint->family & OC_IP_USE_V4)
        {
            // ipv4
            int snRet = snprintf(buf, MAX_ADDR_STR_SIZE, "%s://%s:%d", endpoint->tps,
                                  endpoint->addr, endpoint->port);
            VERIFY_SNPRINTF_RET(snRet, MAX_ADDR_STR_SIZE);
        }
        else
        {
            // ipv6
            int snRet = snprintf(buf, MAX_ADDR_STR_SIZE, "%s://[%s]:%d", endpoint->tps,
                                  endpoint->addr, endpoint->port);
            VERIFY_SNPRINTF_RET(snRet, MAX_ADDR_STR_SIZE);
        }
    }
#ifdef EDR_ADAPTER
    else if ((strcmp(endpoint->tps, COAP_RFCOMM_STR) == 0))
    {
        // coap+rfcomm
        int snRet = snprintf(buf, MAX_ADDR_STR_SIZE, "%s://%s",
                              endpoint->tps, endpoint->addr);
        VERIFY_SNPRINTF_RET(snRet, MAX_ADDR_STR_SIZE);
    }
#endif
    else
    {
        OIC_LOG_V(ERROR, TAG, "Payload has invalid TPS!!! %s", endpoint->tps);
        goto exit;
    }
    OIC_LOG_V(DEBUG, TAG, "%s EXIT: %s", __func__, buf);
    return buf;

exit:
    OICFree(buf);
    return NULL;
}

char* OC_CALL OCCreateEndpointStringFromCA(const CAEndpoint_t* endpoint)
{
    if (!endpoint || 0 == strlen(endpoint->addr))
    {
        return NULL;
    }

    int snRet = -1;
    char *buf = NULL;
    OCTpsSchemeFlags tps = OC_NO_TPS;
    OCStackResult result = OCGetMatchedTpsFlags(endpoint->adapter, endpoint->flags, &tps);
    if (OC_STACK_OK != result)
    {
        goto exit;
    }

    buf = (char*)OICCalloc(MAX_ADDR_STR_SIZE, sizeof(char));
    VERIFY_NON_NULL_1(buf);

    switch (tps)
    {
    case OC_COAP: case OC_COAPS:
#ifdef TCP_ADAPTER
    case OC_COAP_TCP: case OC_COAPS_TCP:
#endif
#ifdef HTTP_ADAPTER
    case OC_HTTP: case OC_HTTPS:
#endif
        if (!endpoint->port)
        {
            goto exit;
        }
        // checking addr is ipv4 or not
        if (endpoint->flags & CA_IPV4)
        {
            // ipv4
            snRet = snprintf(buf, MAX_ADDR_STR_SIZE, "%s://%s:%d", ConvertTpsToString(tps),
                                  endpoint->addr, endpoint->port);
            VERIFY_SNPRINTF_RET(snRet, MAX_ADDR_STR_SIZE);
        }
        else
        {
            // ipv6
            snRet = snprintf(buf, MAX_ADDR_STR_SIZE, "%s://[%s]:%d", ConvertTpsToString(tps),
                                  endpoint->addr, endpoint->port);
            VERIFY_SNPRINTF_RET(snRet, MAX_ADDR_STR_SIZE);
        }
        break;
#ifdef EDR_ADAPTER
    case OC_COAP_RFCOMM:
        // coap+rfcomm
        snRet = snprintf(buf, MAX_ADDR_STR_SIZE, "%s://%s", ConvertTpsToString(tps), endpoint->addr);
        VERIFY_SNPRINTF_RET(snRet, MAX_ADDR_STR_SIZE);
        break;
#endif
    default:
        OIC_LOG_V(ERROR, TAG, "Payload has invalid TPS!!! %d", tps);
        goto exit;
    }
    return buf;

exit:
    OICFree(buf);
    return NULL;
}

OCStackResult OCParseEndpointString(const char* endpointStr, OCEndpointPayload* out)
{
    /* OIC_LOG_V(DEBUG, TAG, "%s ENTRY", __func__); */
    if (!endpointStr || !out)
    {
        return OC_STACK_INVALID_PARAM;
    }

    char* tmp = NULL;
    void* ret = NULL;
    char* tps = NULL;
    char* addr = NULL;
    char* origin = NULL;
    char* tokPos = NULL;
    size_t tpsCharsToWrite = 0;
    size_t addrCharsToWrite = 0;
    OCStackResult isEnabledAdapter = OC_STACK_ADAPTER_NOT_ENABLED;
    OCTransportAdapter parsedAdapter = OC_DEFAULT_ADAPTER;
    bool isSecure = false;

    tps = (char*)OICCalloc(OC_MAX_TPS_STR_SIZE, sizeof(char));
    VERIFY_NON_NULL_1(tps);

    addr = (char*)OICCalloc(OC_MAX_ADDR_STR_SIZE, sizeof(char));
    VERIFY_NON_NULL_1(addr);

    origin = OICStrdup(endpointStr);
    VERIFY_NON_NULL_1(origin);

    // token start pos
    tokPos = strstr(origin, OC_ENDPOINT_TPS_TOKEN);
    VERIFY_NON_NULL_1(tokPos);

    // copy tps
    tpsCharsToWrite = tokPos - origin;
    VERIFY_GT_ZERO(tpsCharsToWrite);
    VERIFY_GT((size_t)OC_MAX_TPS_STR_SIZE, tpsCharsToWrite);
    ret = memcpy(tps, origin, tpsCharsToWrite);
    VERIFY_NON_NULL_1(ret);
    OIC_LOG_V(INFO, TAG, "\ttps: %s", tps);

    // check tps type
    if (strcmp(tps, COAP_STR) == 0)
    {
        isEnabledAdapter = OC_STACK_OK;
        parsedAdapter = OC_ADAPTER_IP;
    }
    else if (strcmp(tps, COAPS_STR) == 0)
    {
        isEnabledAdapter = OC_STACK_OK;
        parsedAdapter = OC_ADAPTER_IP;
        isSecure = true;
    }
#ifdef TCP_ADAPTER
    else if (strcmp(tps, COAP_TCP_STR) == 0)
    {
        isEnabledAdapter = OC_STACK_OK;
        parsedAdapter = OC_ADAPTER_TCP;
    }
    else if (strcmp(tps, COAPS_TCP_STR) == 0)
    {
        isEnabledAdapter = OC_STACK_OK;
        parsedAdapter = OC_ADAPTER_TCP;
        isSecure = true;
    }
#endif
#ifdef HTTP_ADAPTER
    // @todo iotivity not support http, https transport yet...
#endif
#ifdef EDR_ADAPTER
    else if (strcmp(tps, COAP_RFCOMM_STR) == 0)
    {
        isEnabledAdapter = OC_STACK_OK;
        parsedAdapter = OC_ADAPTER_RFCOMM_BTEDR;
    }
#endif
    // ignore unrecognized tps type
    if (isEnabledAdapter == OC_STACK_ADAPTER_NOT_ENABLED
        && parsedAdapter == OC_DEFAULT_ADAPTER)
    {
        OICFree(tps);
        OICFree(addr);
        OICFree(origin);
        return OC_STACK_ADAPTER_NOT_ENABLED;
    }

#ifdef EDR_ADAPTER
    if (parsedAdapter == OC_ADAPTER_RFCOMM_BTEDR)
    {
        // copy addr
        tokPos = tokPos + 3;
        ret = OICStrcpy(addr, OC_MAX_ADDR_STR_SIZE, tokPos);
        VERIFY_NON_NULL_1(ret);
        out->tps = tps;
        out->addr = addr;
        out->family = OC_DEFAULT_FLAGS;
        out->port = 0;
    }
    else
#endif
    {
        // first check epl has square bracket
        tmp = strchr(origin, OC_ENDPOINT_BRACKET_START);
        if (tmp)
        {
            out->family = OC_IP_USE_V6;
            tokPos = tokPos + 4;
            tmp = strrchr(origin, OC_ENDPOINT_BRACKET_END);
        }
        else
        {
            out->family = OC_IP_USE_V4;
            tokPos = tokPos + 3;
            tmp = strrchr(origin, OC_ENDPOINT_ADDR_TOKEN);
        }
        VERIFY_NON_NULL_1(tmp);
        if (isSecure)
        {
            out->family = (OCTransportFlags)(out->family | OC_FLAG_SECURE);
        }

        // copy addr
        addrCharsToWrite = tmp - tokPos;
        VERIFY_GT_ZERO(addrCharsToWrite);
        VERIFY_GT((size_t)OC_MAX_ADDR_STR_SIZE, addrCharsToWrite);
        ret = memcpy(addr, tokPos, addrCharsToWrite);
        VERIFY_NON_NULL_1(ret);
        OIC_LOG_V(INFO, TAG, "\taddr: %s", addr);

        tmp = strrchr(origin, OC_ENDPOINT_ADDR_TOKEN);
        VERIFY_NON_NULL_1(tmp);

        // port start pos
        tokPos = tmp + 1;
        char* end = NULL;
        long port = strtol(tokPos, &end, 10);
        VERIFY_GT_ZERO(port);
        VERIFY_LT_OR_EQ(port, UINT16_MAX);
        OIC_LOG_V(INFO, TAG, "\tport: %s", tokPos);

        out->tps = tps;
        out->addr = addr;
        out->port = (uint16_t)port;
    }

    OICFree(origin);
    origin = NULL;
    tokPos = NULL;
    tmp = NULL;
    ret = NULL;

    /* OIC_LOG_V(DEBUG, TAG, "%s EXIT OK", __func__); */
    return OC_STACK_OK;
exit:
    OICFree(origin);
    OICFree(tps);
    OICFree(addr);
    origin = NULL;
    tokPos = NULL;
    tmp = NULL;
    ret = NULL;
    tps = NULL;
    addr = NULL;

    OIC_LOG_V(DEBUG, TAG, "%s EXIT ERROR", __func__);
    return OC_STACK_ERROR;
}

OCTpsSchemeFlags OCGetSupportedTpsFlags(void)
{
    OCTpsSchemeFlags ret = OC_NO_TPS;

#ifndef DISABLE_UDP
    //CATransportAdapter_t SelectedNetwork = CAGetSelectedNetwork();

    /* if (SelectedNetwork & CA_ADAPTER_IP) */
    /* { */
        ret = (OCTpsSchemeFlags)(ret | OC_COAP);

        if (OC_SECURE)
        {
            ret = (OCTpsSchemeFlags)(ret | OC_COAPS);
        }
    /* } */
#endif
#ifdef TCP_ADAPTER
    /* if (SelectedNetwork & CA_ADAPTER_TCP) */
    /* { */
        ret = (OCTpsSchemeFlags)(ret | OC_COAP_TCP);

        if (OC_SECURE)
        {
            ret = (OCTpsSchemeFlags)(ret | OC_COAPS_TCP);
        }
    /* } */
#endif
#ifdef EDR_ADAPTER
    /* if (SelectedNetwork & CA_ADAPTER_RFCOMM_BTEDR) */
    /* { */
        ret = (OCTpsSchemeFlags)(ret | OC_COAP_RFCOMM);
    /* } */
#endif
#ifdef LE_ADAPTER
    /* if (SelectedNetwork & CA_ADAPTER_GATT_BTLE) */
    /* { */
        ret = (OCTpsSchemeFlags)(ret | OC_COAP_GATT);
    /* } */
#endif
#ifdef NFC_ADAPTER
    /* if (SelectedNetwork & CA_ADAPTER_NFC) */
    /* { */
        ret = (OCTpsSchemeFlags)(ret | OC_COAP_NFC);
    /* } */
#endif
#ifdef RA_ADAPTER
    /* if (SelectedNetwork & CA_ADAPTER_REMOTE_ACCESS) */
    /* { */
        ret = (OCTpsSchemeFlags)(ret | OC_COAP_RA);
    /* } */
#endif
    return ret;
}

/*
 * Get realtime list of one address per unique (IF, family) pair
 */
CAResult_t udp_get_local_endpoints(CAEndpoint_t **info, size_t *size) // @was CAGetIPInterfaceInformation
{
    OIC_LOG_V(DEBUG, TAG, "%s ENTRY", __func__);
    VERIFY_NON_NULL_MSG(info, TAG, "info is NULL");
    VERIFY_NON_NULL_MSG(size, TAG, "size is NULL");

    // GAR: get live list of CAInterface_t, each represents a unique (IF, family) pair
    u_arraylist_t *iflist = udp_get_all_nifs();
    if (!iflist) {
        OIC_LOG_V(ERROR, TAG, "udp_get_all_nifs failed: %s", strerror(errno));
        return CA_STATUS_FAILED;
    }

#ifdef __WITH_DTLS__
    const size_t endpointsPerInterface = 2;
#else
    const size_t endpointsPerInterface = 1;
#endif

    /* get a count of enabled IPv4/IPv6 IFs */
    size_t iface_count = u_arraylist_length(iflist);
    for (size_t i = 0; i < u_arraylist_length(iflist); i++)
    {
        CAInterface_t *ifitem = (CAInterface_t *)u_arraylist_get(iflist, i);
        if (!ifitem)
        {
            continue;
        }

        if ((ifitem->family == AF_INET6 && !udp_ipv6_is_enabled) ||
            (ifitem->family == AF_INET && !udp_ipv4_is_enabled))
        {
            iface_count--;
        }
    }

    if (!iface_count)
    {
        OIC_LOG(DEBUG, TAG, "No enabled IPv4/IPv6 interfaces found");
        return CA_STATUS_OK;
    }

    size_t totalEndpoints = iface_count * endpointsPerInterface;
    CAEndpoint_t *eps = (CAEndpoint_t *)OICCalloc(totalEndpoints, sizeof (CAEndpoint_t));
    if (!eps)
    {
        OIC_LOG(ERROR, TAG, "Malloc Failed");
        u_arraylist_destroy(iflist);
        return CA_MEMORY_ALLOC_FAILED;
    }

    OIC_LOG_V(INFO, TAG, "%s constructing %d eps for %d interfaces", __func__, totalEndpoints, iface_count);

    /* create one CAEndpoint_t per (IF, family) */
    for (size_t i = 0,		/* index into iflist to control iteration */
	     j = 0;		/* index into eps array */
	 i < u_arraylist_length(iflist); i++)
    {
        CAInterface_t *ifitem = (CAInterface_t *)u_arraylist_get(iflist, i);
        if (!ifitem)
        {
            continue;
        }
	OIC_LOG_V(DEBUG, TAG, "%s creating ep %d, ifindex %d", __func__, i*2, ifitem->index);

	/* skip disabled IFs */
        if ((ifitem->family == AF_INET6 && !udp_ipv6_is_enabled) ||
            (ifitem->family == AF_INET && !udp_ipv4_is_enabled))
        {
            continue;
        }

        eps[j].adapter = CA_ADAPTER_IP; /* meaning UDP transport, NOT ipv4/v6 */
        // eps[j].ifindex = ifitem->index;

        if (ifitem->family == AF_INET6)
        {
            eps[j].flags = CA_IPV6;
            eps[j].port = udp_u6.port; /* why not take port from iflist item? */
        }
        else
        {
            eps[j].flags = CA_IPV4;
            eps[j].port = udp_u4.port;
        }
        OICStrcpy(eps[j].addr, sizeof(eps[j].addr), ifitem->addr);

#ifdef __WITH_DTLS__
	/* add secured endpoint */
        j++;
	OIC_LOG_V(DEBUG, TAG, "%s creating ep %d (secure), ifindex %d, addr %s",
                  __func__, i*2+1, ifitem->index, ifitem->addr);

        eps[j].adapter = CA_ADAPTER_IP;
        // oeps[j].ifindex = ifitem->index;

        if (ifitem->family == AF_INET6)
        {
            eps[j].flags = CA_IPV6 | CA_SECURE;
            eps[j].port = udp_u6s.port;
        }
        else
        {
            eps[j].flags = CA_IPV4 | CA_SECURE;
            eps[j].port = udp_u4s.port;
        }
        OICStrcpy(eps[j].addr, sizeof(eps[j].addr), ifitem->addr);
#endif
        j++;
    }

    *info = eps;
    *size = totalEndpoints;

    u_arraylist_destroy(iflist);

    OIC_LOG_V(DEBUG, TAG, "%s EXIT", __func__);
    return CA_STATUS_OK;
}

u_arraylist_t *local_endpoints = NULL;

u_arraylist_t *oocf_get_local_endpoints(void) EXPORT
{
    OIC_LOG_V(DEBUG, TAG, "%s ENTRY", __func__);

    /* size_t len = u_arraylist_length(g_local_endpoint_cache); */
    /* for (size_t i = 0; i < len; i++) */
    /* { */
    /*     CAEndpoint_t *ownIpEp = u_arraylist_get(g_local_endpoint_cache, i); */
    /* 	OIC_LOG_V(DEBUG, TAG, "local ptr: %p", ownIpEp); */
    /* } */

    local_endpoints = u_arraylist_create();

    if (!local_endpoints)
    {
        OIC_LOG(ERROR, TAG, "Memory allocation failed! (local_endpoints)");
        return NULL;
    }

        CAEndpoint_t *eps = NULL;
        size_t numOfEps = 0;

        CAResult_t res = udp_get_local_endpoints(&eps, &numOfEps); /* @was CAGetIPInterfaceInformation */
        if (CA_STATUS_OK != res)
        {
            OIC_LOG(ERROR, TAG, "CAGetIPInterfaceInformation failed");
            return NULL;
        }
	OIC_LOG_V(DEBUG, TAG, "Found %d local endpoints", numOfEps);

        for (size_t i = 0; i < numOfEps; i++)
        {
	    OIC_LOG_V(DEBUG, TAG, "Caching %d: %s [%d]", i, eps[i].addr); //, eps[i].ifindex);
            u_arraylist_add(local_endpoints, (void *)&eps[i]);
        }
	return local_endpoints;
	//return g_local_endpoint_cache;
}

// FIXMME: move this to udp_status_manager?
// @rewrite udp_update_local_endpoint_cache @was CAUpdateStoredIPAddressInfo
// called by udp_if_change_handler (@was CAIPPassNetworkChangesToAdapter)
void udp_refresh_local_endpoint_cache(CANetworkStatus_t status) // @was CAUpdateStoredIPAddressInfo
{
    OIC_LOG_V(DEBUG, TAG, "%s ENTRY", __func__);
    if (CA_INTERFACE_UP == status)
    {
        OIC_LOG(DEBUG, TAG, "UDP status is UP. Caching local endpoints");

        CAEndpoint_t *eps = NULL;
        size_t numOfEps = 0;

        CAResult_t res = udp_get_local_endpoints(&eps, &numOfEps); /* @was CAGetIPInterfaceInformation */
        if (CA_STATUS_OK != res)
        {
            OIC_LOG(ERROR, TAG, "CAGetIPInterfaceInformation failed");
            return;
        }
	OIC_LOG_V(DEBUG, TAG, "Found %d local endpoints", numOfEps);

        for (size_t i = 0; i < numOfEps; i++)
        {
	    // FIXME: what about duplicates?
	    OIC_LOG_V(DEBUG, TAG, "Caching %d: %s [%d]", i, eps[i].addr); // , eps[i].ifindex);
            u_arraylist_add(g_local_endpoint_cache, (void *)&eps[i]);
        }
    }
    else // CA_INTERFACE_DOWN
    {
        OIC_LOG(DEBUG, TAG, "UDP status is off. Clearing the local endpoint cache");

        CAEndpoint_t *headEp = u_arraylist_get(g_local_endpoint_cache, 0);
        OICFree(headEp);
        headEp = NULL;

        size_t len = u_arraylist_length(g_local_endpoint_cache);
        for (size_t i = len; i > 0; i--)
        {
            u_arraylist_remove(g_local_endpoint_cache, i - 1);
        }
    }
    OIC_LOG_V(DEBUG, TAG, "%s EXIT", __func__);
}

bool CAIPIsLocalEndpoint(const CAEndpoint_t *ep)
{
    char addr[MAX_ADDR_STR_SIZE_CA];
    OICStrcpy(addr, MAX_ADDR_STR_SIZE_CA, ep->addr);

    // drop the zone ID if the address of endpoint is IPv6. ifindex will be checked instead.
    if ((ep->flags & CA_IPV6) && strchr(addr, '%'))
    {
        strtok(addr, "%");
    }

    size_t len = u_arraylist_length(g_local_endpoint_cache);
    for (size_t i = 0; i < len; i++)
    {
        CAEndpoint_t *ownIpEp = u_arraylist_get(g_local_endpoint_cache, i);
        if (!strcmp(addr, ownIpEp->addr) && ep->port == ownIpEp->port)
            // && ep->ifindex == ownIpEp->ifindex)
        {
            return true;
        }
    }

    return false;
}
