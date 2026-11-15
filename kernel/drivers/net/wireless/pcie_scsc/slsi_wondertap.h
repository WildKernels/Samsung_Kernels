/****************************************************************************
 *
 * Copyright (c) 2012 - 2026 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef _WONDERTAP_H_
#define _WONDERTAP_H_
#include "dev.h"
#include <wonder/wondertap.h>
#include <linux/ieee80211.h>

struct vendor_hardware_ctx {
	/* Current hardware configuration */
	struct wondertap_set_freq_params freq;
	struct wondertap_fixed_tx_rate_params tx_rate;
	struct wondertap_tx_rate_mask_params rate_mask;
 	/* State flags */
	bool initialized;
	u8 mac_addr[ETH_ALEN];
};

int slsi_wondertap_register(struct slsi_dev *sdev);
void slsi_wondertap_unregister(struct slsi_dev *sdev);

/* Platform driver registration hooks for module init/exit. */
int slsi_wondertap_pdrv_register(void);
void slsi_wondertap_pdrv_unregister(void);

/**
 * @brief Extract source and destination addresses from 802.11 header
 * @param skb The socket buffer containing 802.11 frame
 * @param da Pointer to store destination address
 * @param sa Pointer to store source address
 * @return 0 on success, -EINVAL on error
 */
static inline int slsi_tx_wondertap_addrs_from_80211(struct sk_buff *skb, u8 *da, u8 *sa)
{
	struct ieee80211_hdr *hdr;
	u8 *dst;
	u8 *src;

	if (!skb || skb->len < sizeof(struct ieee80211_hdr))
		return -EINVAL;

	hdr = (struct ieee80211_hdr *)(skb->head + skb->mac_header);

	dst = ieee80211_get_DA(hdr);
	src = ieee80211_get_SA(hdr);

	memcpy(da, dst, ETH_ALEN);
	memcpy(sa, src, ETH_ALEN);

	return 0;
}

/**
 * @brief Create a pseudo-Ethernet header for WonderTap TX API integration
 * @param skb The socket buffer containing raw 802.11 frame
 * @return 0 on success, negative error code on failure
 *
 * For WonderTap, we need to create a temporary Ethernet header so the TX API
 * can process the packet. The addresses are extracted from the 802.11 header.
 */
static inline int slsi_tx_wondertap_push_eth_header(struct sk_buff *skb)
{
	u8 da[ETH_ALEN], sa[ETH_ALEN];
	struct ethhdr *ethhdr;
	int ret;

	if (!skb)
		return -EINVAL;

	/* Extract addresses from 802.11 header */
	ret = slsi_tx_wondertap_addrs_from_80211(skb, da, sa);
	if (ret < 0)
		return ret;

	/* keep tx_d */
	skb_push(skb, sizeof(struct wonder_txd));
	/* Push Ethernet header */
	ethhdr = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));

	/* Set Ethernet addresses */
	memcpy(ethhdr->h_dest, da, ETH_ALEN);
	memcpy(ethhdr->h_source, sa, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_802_2); /* Use 802.2 for raw frames */

	return 0;
}
#endif /* _WONDERTAP_H_ */
