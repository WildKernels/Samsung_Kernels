/****************************************************************************
 *
 * Copyright (c) 2012 - 2026 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/component.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/bitops.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <pcie_scsc/scsc_mx.h>
#include "slsi_wondertap.h"
#include "debug.h"
#include "mlme.h"
#include "dev.h"
#include "mgt.h"
#include "tx_api.h"
#include "netif.h"
#include "txbp.h"
#include "hip.h"

/* ============================================================================
 * 2. Implementation of wondertap_ops (Callbacks)
 * ============================================================================
 * These functions are called by the WonderTap Core Driver.
 */
/**
 * vendor_wondertap_init - Initializes the vendor driver instance.
 * @handle:  Double pointer to be populated with the vendor context.
 * @params:  Initialization parameters from the core.
 *
 * Return: 0 on success, negative error code on failure.
 */
static struct slsi_dev               *sdev;
static struct net_device             *wondertap_dev;
static struct platform_device        *slsi_wondertap_pdev;
static bool                           slsi_wondertap_component_added;
/*
 * slsi_wondertap_op_mutex serializes all wondertap_ops invocations against
 * teardown (slsi_wondertap_unregister / component unbind). All ops must take
 * this mutex before touching `sdev` / `wondertap_dev` / the active ctx, and
 * teardown takes it after component_del to drain any in-flight callers
 * before clearing the static state.
 *
 * slsi_wondertap_active_ctx tracks the live ctx allocated by ->init(). It
 * lets the unbind callback free the ctx if WLAN service goes down before
 * userspace called ->deinit(), and it lets ops detect stale handles after
 * a previous teardown freed the ctx.
 */
static DEFINE_MUTEX(slsi_wondertap_op_mutex);
static struct vendor_hardware_ctx    *slsi_wondertap_active_ctx;
static int slsi_wondertap_interface_add(void)
{
	struct netdev_vif *ndev_vif;
	int ret = 0;
	bool is_cfg80211 = false;
	if (sdev->netdev[SLSI_NET_INDEX_RELAY]) {
		SLSI_ERR(sdev, "[Wonder]Interface already exists : %s\n", sdev->netdev[SLSI_NET_INDEX_RELAY]->name);
		return ret;
	}
	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	ret = slsi_netif_add_locked(sdev, "wondertap0", SLSI_NET_INDEX_RELAY);
		if (ret) {
			SLSI_ERR(sdev, "[Wonder] fail net_if_add wondertap0, if_idx:%d\n", SLSI_NET_INDEX_RELAY);
			SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
			return ret;
	}
	wondertap_dev = slsi_get_netdev_locked(sdev, SLSI_NET_INDEX_RELAY);
	if (!wondertap_dev)
		SLSI_ERR(sdev, "[Wonder]slsi_get_netdev_locked fail");
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
	ret = slsi_netif_register_rtlnl_locked(sdev, wondertap_dev, is_cfg80211);
	if (ret) {
		SLSI_ERR(sdev, "[Wonder]wondertap0 interface registration fail:%d\n", ret);
		return ret;
	} else {
		SLSI_ERR(sdev, "[Wonder] wondertap0 interface registration success, name : %s\n", wondertap_dev->name);
		ndev_vif = netdev_priv(wondertap_dev);
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
		ndev_vif->vif_type = FAPI_VIFTYPE_RELAY;
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	}
	return ret;
}
static void slsi_wondertap_interface_remove(void)
{
	bool is_cfg80211 = false;
	if (!wondertap_dev)
		return;
	if (sdev)
		SLSI_INFO(sdev, "wonder_interface_remove: detach %s\n", wondertap_dev->name);
	slsi_netif_remove_rtlnl_locked(sdev, wondertap_dev, is_cfg80211);
	wondertap_dev = NULL;
}
static bool slsi_wondertap_nan_activated(void)
{
	struct net_device *dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_NAN);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	if (ndev_vif->activated && ndev_vif->vif_type == FAPI_VIFTYPE_NAN)
		return true;
	return false;
}
static bool slsi_wondertap_sta_connected(void)
{
	struct net_device *dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	if (ndev_vif->activated && ndev_vif->vif_type == FAPI_VIFTYPE_STATION &&
	    (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTING ||
	     ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED)) {
		return true;
	}
	return false;
}
static bool slsi_wondertap_softap_activated(void)
{
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_AP);
	if (dev) {
		ndev_vif = netdev_priv(dev);
		if (ndev_vif->activated)
			return true;
	}
	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_AP2);
	if (dev) {
		ndev_vif = netdev_priv(dev);
		if (ndev_vif->activated)
			return true;
	}
	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_AP_VLAN + sdev->num_ap_vlan);
	if (dev) {
		ndev_vif = netdev_priv(dev);
		if (ndev_vif->activated)
			return true;
	}
	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	if (dev) {
		ndev_vif = netdev_priv(dev);
		if (ndev_vif->activated)
			return true;
	}
	return false;
}
static int slsi_wondertap_init(void **handle, const struct wondertap_init_params *params)
{
	struct vendor_hardware_ctx *ctx = NULL;
	struct netdev_vif *ndev_vif = NULL;
	u8 device_address[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	char country_code[3];
	int ret = 0;
	mutex_lock(&slsi_wondertap_op_mutex);
	if (!sdev) {
		SLSI_ERR_NODEV("[Wonder] sdev is NULL\n");
		ret = -ENODEV;
		goto err_unlock;
	}
	ret = slsi_wondertap_interface_add();
	if (ret) {
		SLSI_ERR(sdev, "[Wonder] wonder_interface_add failed: %d\n", ret);
		sdev = NULL;
		goto err_free_ctx;
	}
	if (!wondertap_dev) {
		SLSI_ERR(sdev, "[Wonder]failed to dev. Cannot start Relay\n");
		ret = -ENODEV;
		goto err_unlock;
	}
	ndev_vif = netdev_priv(wondertap_dev);
	if (!ndev_vif) {
		SLSI_ERR(sdev, "[Wonder] ndev_vif is NULL\n");
		ret = -ENODEV;
		goto err_unlock;
	}
	if (!params) {
		SLSI_ERR(sdev, "[Wonder] init error: NULL params\n");
		ret = -EINVAL;
		goto err_unlock;
	}
	if (!sdev->wlan_service_on) {
		SLSI_ERR(sdev, "[Wonder]WLAN service is not on. Cannot start Relay\n");
		ret = -EBUSY;
		goto err_unlock;
	}
	if (sdev->mlme_blocked) {
		SLSI_ERR(sdev, "[Wonder]MLME is blocked. Cannot start Relay\n");
		ret = -EBUSY;
		goto err_unlock;
	}
	if (sdev->p2p_state == P2P_SCANNING || sdev->p2p_state == P2P_LISTENING
		|| sdev->p2p_state == P2P_GROUP_FORMED_CLI || sdev->p2p_state == P2P_GROUP_FORMED_GO) {
		SLSI_ERR(sdev, "[Wonder]P2P is running. Cannot start Relay\n");
		ret = -EBUSY;
		goto err_unlock;
	}
	if (slsi_wondertap_nan_activated()) {
		SLSI_ERR(sdev, "[Wonder]NAN is running. Cannot start Relay\n");
		ret = -EBUSY;
		goto err_unlock;
	}
	if (slsi_wondertap_sta_connected()) {
		SLSI_ERR(sdev, "[Wonder]STA connected. Cannot start Relay\n");
		ret = -EBUSY;
		goto err_unlock;
	}
	if (slsi_wondertap_softap_activated()) {
		SLSI_ERR(sdev, "MHS is activated. Cannot start Relay\n");
		ret = -EBUSY;
		goto err_unlock;
	}
	/* Allocate vendor context */
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto err_unlock;
	}
	/* Store initial parameters */
	ctx->freq = params->channel;
	ctx->tx_rate = params->tx_rate;
	memcpy(ctx->mac_addr, params->mac_addr, ETH_ALEN);
	ctx->initialized = true;
	*handle = ctx;
	/*	Bring the interface up if it is not already up	*/
	wondertap_dev->type = ARPHRD_IEEE80211_RADIOTAP;
	eth_hw_addr_set(wondertap_dev, params->mac_addr);
	ret = dev_change_flags(wondertap_dev, wondertap_dev->flags | IFF_UP, NULL);
	if (ret) {
		SLSI_ERR(sdev, "[Wonder]Failed to bring up dev(%s): %d\n", wondertap_dev->name, ret);
		slsi_wondertap_interface_remove();
		goto err_free_ctx;
	}
	/* ADD COUNTRY CODE */
	memcpy(country_code, params->country_code, 2);
	country_code[2] = '\0';
	ret = slsi_mlme_set_country(sdev, country_code);
	if (ret) {
		SLSI_ERR(sdev, "[Wonder]failed to set country. Cannot start Relay\n");
		goto err_free_ctx;
	}
	SLSI_INFO(sdev, "[Wonder]Config MAC: %pM, Country: %s\n", ctx->mac_addr, params->country_code);
	SLSI_INFO(sdev, "[Wonder]Retry Limits - Mgmt:%d, Data:%d\n", params->mgmt_retry_limit, params->data_retry_limit);
	/* MLME ADD VIF */
	if (ndev_vif->activated) {
		SLSI_INFO(sdev, "[Wonder]Already Enabled. Req Rejected\n");
		ret = -EBUSY;
		goto err_free_ctx;
	}
	ndev_vif->vif_type = FAPI_VIFTYPE_RELAY;
	ret = slsi_mlme_add_vif(sdev, wondertap_dev, ctx->mac_addr, device_address);
	if (ret) {
			SLSI_ERR(sdev, "[Wonder]failed to add relay vif. Cannot start Relay\n");
			goto err_free_ctx;
	}
	if (slsi_vif_activated(sdev, wondertap_dev) != 0)
		SLSI_ERR(sdev, "slsi_vif_activated failed\n");
	if (!sdev->wlan_service_on || !ndev_vif->tx_netdev_data) {
		SLSI_ERR(sdev, "[Wonder]cannot init tx_netdev_data: service_on=%d, tx_priv=%p\n",
				 sdev->wlan_service_on, ndev_vif->tx_netdev_data);
		ret = -EBUSY;
		goto err_del_vif;
	}
	SLSI_INFO(sdev, "[Wonder]ctx->tx_rate.preamble:%d ctx->tx_rate.bw:%d, ctx->tx_rate.nss:%d, ctx->tx_rate.gi:%d, ctx->tx_rate.mcs:%d ctx->rate_mask.legacy_rates:%d\n",
		ctx->tx_rate.preamble, ctx->tx_rate.bw, ctx->tx_rate.nss, ctx->tx_rate.gi, ctx->tx_rate.mcs, ctx->rate_mask.legacy_rates);
	/* MLME RELAY START */
	ret = slsi_mlme_relay_start_request(sdev, wondertap_dev, ctx);
	if (ret) {
		SLSI_ERR(sdev, "[Wonder]failed to relay start\n");
		goto err_del_vif;
	}
	SLSI_INFO(sdev, "[Wonder]Relay vif and start success\n");
	slsi_wondertap_active_ctx = ctx; /* track for leak prevention on unbind */
	mutex_unlock(&slsi_wondertap_op_mutex);
	return 0;
err_del_vif:
	if (ndev_vif) {
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
		if (ndev_vif->activated) {
			slsi_mlme_del_vif(sdev, wondertap_dev);
			ndev_vif->activated = false;
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	}
err_free_ctx:
	kfree(ctx);
	*handle = NULL;
err_unlock:
	mutex_unlock(&slsi_wondertap_op_mutex);
	return ret;
}
/**
 * vendor_wondertap_deinit - Deinitializes the vendor driver instance.
 * @handle: The opaque driver instance handle.
 *
 * Note: This operation signature does not include extra params in the ops struct.
 */
static void slsi_wondertap_deinit(void *handle, const struct wondertap_deinit_params *params)
{
	struct vendor_hardware_ctx *ctx = handle;
	struct netdev_vif *ndev_vif;
	if (!ctx) {
		SLSI_ERR_NODEV("[Wonder]deinit called with NULL handle\n");
		return;
	}
	mutex_lock(&slsi_wondertap_op_mutex);
	if (ctx != slsi_wondertap_active_ctx) {
		/*
		 * Stale handle: ctx was already freed (e.g., by unbind cleanup
		 * after WLAN service teardown). Don't double-free or deref.
		 */
		SLSI_WARN_NODEV("[Wonder]deinit called with stale handle, ignoring\n");
		mutex_unlock(&slsi_wondertap_op_mutex);
		return;
	}
	if (sdev && wondertap_dev) {
		SLSI_INFO(sdev, "[Wonder]deinit called\n");
		ndev_vif = netdev_priv(wondertap_dev);
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
		if (ndev_vif->activated) {
			if (slsi_mlme_del_vif(sdev, wondertap_dev) != 0)
				SLSI_ERR(sdev, "[Wonder]slsi_mlme_del_vif failed\n");
			slsi_vif_deactivated(sdev, wondertap_dev);
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	}
	slsi_wondertap_interface_remove();
	slsi_wondertap_active_ctx = NULL;
	kfree(ctx);
	mutex_unlock(&slsi_wondertap_op_mutex);
}
/**
 * vendor_wondertap_set_freq - Sets the operating channel and bandwidth.
 * @handle: The driver instance handle.
 * @params: Pointer to the channel configuration.
 */
static int slsi_wondertap_set_freq(void *handle, const struct wondertap_set_freq_params *params)
{
	struct vendor_hardware_ctx *ctx = handle;
	int ret;
	if (!ctx)
		return -EINVAL;
	mutex_lock(&slsi_wondertap_op_mutex);
	if (!sdev || ctx != slsi_wondertap_active_ctx || !ctx->initialized) {
		mutex_unlock(&slsi_wondertap_op_mutex);
		return -ENODEV;
	}
	SLSI_INFO(sdev, "[Wonder]Set Frequency - Center Freq: %d MHz, BW: %d\n", params->freq, params->bandwidth);
	ret = slsi_mlme_relay_set_channel_request(sdev, wondertap_dev, params);
	if (ret)
		SLSI_ERR(sdev, "[Wonder]Failed to set frequency\n");
	else
		SLSI_INFO(sdev, "[Wonder]Set frequency success\n");
	if (!ret)
		ctx->freq = *params;
	mutex_unlock(&slsi_wondertap_op_mutex);
	return ret;
}
/**
 * vendor_wondertap_set_filter - Configures a packet filter.
 * @handle:       The driver instance handle.
 * @filter_type:   Type of filter to configure.
 * @params:       Void pointer to filter-specific params.
 */
static int slsi_wondertap_set_filter(void *handle, enum wondertap_filter_type filter_type, const void *params)
{
	struct vendor_hardware_ctx *ctx = handle;
	int ret = 0;
	if (!ctx)
		return -EINVAL;
	mutex_lock(&slsi_wondertap_op_mutex);
	if (!sdev || ctx != slsi_wondertap_active_ctx || !ctx->initialized) {
		mutex_unlock(&slsi_wondertap_op_mutex);
		return -ENODEV;
	}
	if (filter_type == WONDERTAP_FILTER_TYPE_FRAME) {
		const struct wondertap_frame_filter_params *fp = params;
		SLSI_INFO(sdev, "[Wonder]Set Frame Filter - Enabled:%d, Type:0x%04x, Subtype:0x%04x\n",
				fp->enabled, fp->frame_type, fp->frame_subtype);
		if (fp->enabled)
			/* Enable hardware filter logic */
			SLSI_INFO(sdev, "[Wonder]Hardware filter configured for Type:0x%04x\n", fp->frame_type);
	} else {
		SLSI_ERR(sdev, "[Wonder]Unsupported filter type %d\n", filter_type);
		ret = -EINVAL;
	}
	mutex_unlock(&slsi_wondertap_op_mutex);
	return ret;
}
/**
 * vendor_wondertap_set_fixed_tx_rate - Configures a fixed transmission rate.
 * @handle: The driver instance handle.
 * @params: Pointer to the rate configuration.
 */
static int slsi_wondertap_set_fixed_tx_rate(void *handle, const struct wondertap_fixed_tx_rate_params *params)
{
	struct vendor_hardware_ctx *ctx = handle;
	int ret;
	if (!ctx)
		return -EINVAL;
	mutex_lock(&slsi_wondertap_op_mutex);
	if (!sdev || ctx != slsi_wondertap_active_ctx || !ctx->initialized) {
		mutex_unlock(&slsi_wondertap_op_mutex);
		return -ENODEV;
	}
	SLSI_INFO(sdev, "[Wonder]Set Fixed TX Rate - Preamble:%d, MCS:%d, NSS:%d, BW:%d, GI:%d\n",
			params->preamble, params->mcs, params->nss, params->bw, params->gi);
	ctx->tx_rate = *params;
	ret = slsi_mlme_relay_set_tx_rate_request(sdev, wondertap_dev, ctx);
	if (ret)
		SLSI_ERR(sdev, "[Wonder]failed to set fixed tx rate \n");
	else
		SLSI_INFO(sdev, "[Wonder]Set fixed tx rate success\n");
	mutex_unlock(&slsi_wondertap_op_mutex);
	return ret;
}
/**
 * vendor_wondertap_set_tx_rate_mask - Configures permitted TX rates.
 * @handle: The driver instance handle.
 * @params: Pointer to the rate mask configuration.
 */
static int slsi_wondertap_set_tx_rate_mask(void *handle, const struct wondertap_tx_rate_mask_params *params)
{
	struct vendor_hardware_ctx *ctx = handle;
	int ret;
	if (!ctx)
		return -EINVAL;
	mutex_lock(&slsi_wondertap_op_mutex);
	if (!sdev || ctx != slsi_wondertap_active_ctx || !ctx->initialized) {
		mutex_unlock(&slsi_wondertap_op_mutex);
		return -ENODEV;
	}
	ctx->rate_mask = *params;
	ret = slsi_mlme_relay_set_tx_rate_request(sdev, wondertap_dev, ctx);
	if (ret)
		SLSI_ERR(sdev, "[Wonder]Failed to set tx rate mask\n");
	else
		SLSI_INFO(sdev, "[Wonder]Set tx rate mask success\n");
	mutex_unlock(&slsi_wondertap_op_mutex);
	return ret;
}
/**
 * vendor_wondertap_get_capability - Retrieves capability bits.
 * @handle:       The driver instance handle.
 * @capabilities: Pointer to the struct to be filled.
 */
static int slsi_wondertap_get_capabilities(void *handle, struct wondertap_capability *features)
{
	SLSI_UNUSED_PARAMETER(handle);
	mutex_lock(&slsi_wondertap_op_mutex);
	/* Populate the structure with our cached capabilities */
	features->bits.rate_adaptation = 1;
	features->bits.sta_coexist = 0;
	features->bits.sap_coexist = 0;
	features->bits.p2p_coexist = 0;
	features->bits.nan_coexist = 0;
	features->bits.ranging_coexist = 0;
	features->bits.amsdu_aggregation = 0;
	features->bits.ampdu_aggregation = 0;
	features->bits.dynamic_freq = 1;
	features->bits.dynamic_fixed_tx_rate = 1;
	features->bits.custom_mgmt_retry_limit = 0;
	features->bits.custom_data_retry_limit = 0;
	features->bits.frame_type_filter = 1;
	SLSI_INFO_NODEV("[Wonder]Get Capability report\n");
	mutex_unlock(&slsi_wondertap_op_mutex);
	return 0;
}
/*
static int slsi_wondertap_channel_schedule_request(void *handle, struct channel_schedule_request *request)
{
	struct vendor_hardware_ctx *ctx = handle;
	if (!ctx || !ctx->initialized)
		return -EINVAL;
	//TBD
	return 0;
}
static int slsi_wondertap_get_mac_tsf(void *handle, u32 *mac_tsf)
{
	struct vendor_hardware_ctx *ctx = handle;
	if (!ctx || !ctx->initialized)
		return -EINVAL;
	//TBD
	return 0;
}
*/
/* ============================================================================
 * 3. Ops Table Registration
 * ============================================================================
 * This structure is exposed via component mechanism and used by the Core.
 */
static const struct wondertap_ops vendor_ops = {
	.init = slsi_wondertap_init,
	.deinit = slsi_wondertap_deinit,
	.set_freq = slsi_wondertap_set_freq,
	.set_filter = slsi_wondertap_set_filter,
	.set_fixed_tx_rate = slsi_wondertap_set_fixed_tx_rate,
	.set_tx_rate_mask = slsi_wondertap_set_tx_rate_mask,
	.get_capabilities = slsi_wondertap_get_capabilities,
//	.channel_schedule_request = slsi_wondertap_channel_schedule_request,
//	.get_mac_tsf = slsi_wondertap_get_mac_tsf,
};
/* ============================================================================
 * 4. Component Framework Binding
 * ============================================================================
 */
static int slsi_wondertap_component_bind(struct device *dev, struct device *master, void *data)
{
	SLSI_INFO(sdev, "[Wonder]Successfully bound to master '%s'\n", dev_name(master));
	return 0;
}
static void slsi_wondertap_component_unbind(struct device *dev, struct device *master, void *data)
{
	/*
	 * Runs synchronously inside component_del()/wonder_master_unbind().
	 * Take the op mutex to (a) drain any in-flight wondertap_ops calls
	 * still holding it on another CPU, and (b) safely free a ctx that
	 * userspace never deinit'd before WLAN service teardown.
	 */
	mutex_lock(&slsi_wondertap_op_mutex);
	if (slsi_wondertap_active_ctx) {
		SLSI_WARN_NODEV("[Wonder] freeing leaked vendor_handle on unbind\n");
		kfree(slsi_wondertap_active_ctx);
		slsi_wondertap_active_ctx = NULL;
	}
	mutex_unlock(&slsi_wondertap_op_mutex);
	SLSI_INFO_NODEV("[Wonder]Unbound from master\n");
}
static const struct component_ops vendor_component_ops = {
	.bind = slsi_wondertap_component_bind,
	.unbind = slsi_wondertap_component_unbind,
};
/* ============================================================================
 * 5. Platform Driver Implementation
 * ============================================================================
 *
 * The platform_driver below is bound to the DTSI node with
 * compatible = "samsung,scsc-wondertap-driver". On probe it allocates the
 * wondertap_priv (carrying the version and ops table) and publishes it via
 * platform_set_drvdata(); the wonder driver retrieves it through
 * of_find_device_by_node() + platform_get_drvdata().
 *
 * The component_add() that signals the wonder-side master to bind happens
 * later, inside slsi_wondertap_register(sdev), which is called when the WLAN
 * service comes up. component_del() in slsi_wondertap_unregister() tears
 * the binding down when the service goes away.
 */
static int slsi_wondertap_pdev_probe(struct platform_device *pdev)
{
	struct wondertap_priv *priv;
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->ver = WONDER_VERSION_1_5;
	priv->wonder_ops = &vendor_ops;
	platform_set_drvdata(pdev, priv);
	slsi_wondertap_pdev = pdev;
	dev_info(&pdev->dev, "WonderTap provider probed (ver %d)\n", priv->ver);
	return 0;
}
static void slsi_wondertap_pdev_remove(struct platform_device *pdev)
{
	/*
	 * Safety net: if WLAN service teardown didn't run before module
	 * unload, the wonder master may still hold our vendor_ops pointer.
	 * Force component_del here so wonder unbinds before our .text vanishes.
	 */
	if (slsi_wondertap_component_added) {
		component_del(&pdev->dev, &vendor_component_ops);
		slsi_wondertap_component_added = false;
	}
	platform_set_drvdata(pdev, NULL);
	slsi_wondertap_pdev = NULL;
}
/*
 * platform_driver::remove() ABI shim
 * ----------------------------------
 * Starting with kernel 6.11, the return type of the platform_driver
 * .remove callback changed:
 *   int  (*remove)(struct platform_device *)   // pre-6.11
 *     ↓
 *   void (*remove)(struct platform_device *)   // 6.11+
 * (commit 0edb555a65d1).
 *
 * This driver implements the core function (slsi_wondertap_pdev_remove)
 * using the new (void) signature, but uses the following shim structure
 * to remain build-compatible with older kernels:
 *
 *   - 6.11+: Macros are inactive. The .remove = slsi_wondertap_pdev_remove
 *     in the platform_driver struct below directly binds to the void function.
 *   - Pre-6.11: An int-returning wrapper (_wrapper) is additionally defined,
 *     and token pasting redirects the .remove registration to the wrapper.
 *     The wrapper calls the core function and returns 0, satisfying the old signature.
 *
 * Important macro placement: These macros must appear after the core function
 * definition (slsi_wondertap_pdev_remove) but before the .remove field
 * initialization (slsi_wondertap_pdrv). Otherwise the core function itself
 * gets replaced by the wrapper, causing self-recursion.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int slsi_wondertap_pdev_remove_wrapper(struct platform_device *pdev)
{
	slsi_wondertap_pdev_remove(pdev);
	return 0;
}
#define slsi_wondertap_pdev_remove slsi_wondertap_pdev_remove_wrapper
#endif
static const struct of_device_id slsi_wondertap_of_match[] = {
	{ .compatible = "samsung,scsc-wondertap-driver" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, slsi_wondertap_of_match);
static struct platform_driver slsi_wondertap_pdrv = {
	.probe = slsi_wondertap_pdev_probe,
	.remove = slsi_wondertap_pdev_remove,
	.driver = {
		.name = "scsc-wondertap",
		.of_match_table = slsi_wondertap_of_match,
	},
};
int slsi_wondertap_pdrv_register(void)
{
	return platform_driver_register(&slsi_wondertap_pdrv);
}
void slsi_wondertap_pdrv_unregister(void)
{
	platform_driver_unregister(&slsi_wondertap_pdrv);
}
int slsi_wondertap_register(struct slsi_dev *sdev_param)
{
	struct wondertap_priv *priv;
	int ret;
	if (!sdev_param)
		return -EINVAL;
	if (!slsi_wondertap_pdev) {
		SLSI_ERR(sdev_param,
			 "[Wonder] platform device not bound; check DT for 'samsung,scsc-wondertap-driver' node\n");
		return -ENODEV;
	}
	sdev = sdev_param;
	priv = platform_get_drvdata(slsi_wondertap_pdev);
	if (!priv) {
		SLSI_ERR(sdev, "[Wonder] wondertap_priv missing on pdev\n");
		ret = -ENODEV;
		goto err;
	}
	sdev->wondertap_priv = priv;
	ret = component_add(&slsi_wondertap_pdev->dev, &vendor_component_ops);
	if (ret) {
		SLSI_ERR(sdev, "Failed to add wondertap component: %d\n", ret);
		sdev->wondertap_priv = NULL;
		goto err;
	}
	slsi_wondertap_component_added = true;
	SLSI_INFO(sdev, "WonderTap provider registered (ver %d, of_node=%pOF)\n",
		  priv->ver, slsi_wondertap_pdev->dev.of_node);
	return 0;
err:
	sdev = NULL;
	return ret;
}
void slsi_wondertap_unregister(struct slsi_dev *sdev_param)
{
	if (!sdev_param)
		return;
	/*
	 * component_del() synchronously triggers wonder_master_unbind, which
	 * (a) clears wondertap_ops on the wonder side so no NEW dispatches
	 *     can reach our vendor_ops, and
	 * (b) calls our slsi_wondertap_component_unbind, which takes
	 *     slsi_wondertap_op_mutex -- this drains any in-flight ops
	 *     before component_del() returns.
	 */
	if (slsi_wondertap_pdev && slsi_wondertap_component_added) {
		component_del(&slsi_wondertap_pdev->dev, &vendor_component_ops);
		slsi_wondertap_component_added = false;
	}
	sdev_param->wondertap_priv = NULL;
	/*
	 * Re-take the mutex to publish sdev=NULL atomically w.r.t. ops.
	 * After component_del returns, no new ops can start (ops table is
	 * NULL on wonder side), so this is mostly a memory-ordering fence,
	 * but it costs nothing and keeps the invariant explicit.
	 */
	mutex_lock(&slsi_wondertap_op_mutex);
	sdev = NULL;
	mutex_unlock(&slsi_wondertap_op_mutex);
	SLSI_INFO_NODEV("WonderTap provider unregistered\n");
}

