#include "BleCentralRoutines.hpp"

extern "C" void ble_store_config_init(void);

//Static variable definitions (ugly, I know)
    // Local variables
    uint8_t BleCentralRoutines::s_ble_multi_conn_num = 0;
    uint16_t BleCentralRoutines::messaging_characteristic_val_handle = 0;

//
//*  Handle GATT attribute register events
//*      - Service register event
//*      - Characteristic register event
//*      - Descriptor register event

void BleCentralRoutines::gatt_server_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    // Local variables
    char buf[BLE_UUID_STR_LEN];

    // Handle GATT attributes register events
    switch (ctxt->op) {

    // Service register event
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    // Characteristic register event
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    // Descriptor register event
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    // Unknown event
    default:
        assert(0);
        break;
    }
}

int BleCentralRoutines::gatt_service_send_to_peers(const struct peer *peer, void *arg)
{
    int rc;
    const struct peer_chr *chr;
    struct os_mbuf *src_om = arg;
    struct os_mbuf *dst_om = NULL;

    chr = peer_chr_find_uuid(peer, (const ble_uuid_t *)&BleCentralRoutines::messaging_service_uuid,
                             (const ble_uuid_t *)&BleCentralRoutines::messaging_characteristic_uuid);
    if (!chr) {
        ESP_LOGE(TAG, "Didn't find the characteristic, handle:%d", peer->conn_handle);
        goto failed_to_send;
    }

    dst_om = ble_hs_mbuf_att_pkt();
    if (!dst_om) {
        ESP_LOGE(TAG, "No enough buffer, handle:%d", peer->conn_handle);
        goto failed_to_send;
    }

    rc = os_mbuf_appendfrom(dst_om, src_om, 0, os_mbuf_len(src_om));
    if (rc) {
        ESP_LOGE(TAG, "Failed to copy data, handle:%d, rc:%d", peer->conn_handle, rc);
        goto failed_to_send;
    }

    rc = ble_gattc_write(peer->conn_handle, chr->chr.val_handle, dst_om, NULL, NULL);
    if (rc) {
        ESP_LOGE(TAG, "Failed to write, handle:%d, rc:%d", peer->conn_handle, rc);
        /* The dst_om has already been freed in ble_gattc_write. */
        dst_om = NULL;
        goto failed_to_send;
    }

    return 0;

failed_to_send:
    if (dst_om) {
        os_mbuf_free_chain(dst_om);
    }

    return -1;
}

int BleCentralRoutines::gatt_service_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t data[10];
    uint8_t len;
    struct os_mbuf *om;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        ESP_LOGI(TAG, "Characteristic write; conn_handle=%d", conn_handle);
        if (attr_handle == messaging_characteristic_val_handle) {
            om = ctxt->om;
            len = os_mbuf_len(om);
            len = len < sizeof(data) ? len : sizeof(data);
            assert(os_mbuf_copydata(om, 0, len, data) == 0);
            ESP_LOG_BUFFER_HEX(TAG, data, len);
            /* Send the received data to all of peers. */
            peer_traverse_all(gatt_service_send_to_peers, om);
            return 0;
        }
        goto unknown;

    default:
        goto unknown;
    }

unknown:
    return BLE_ATT_ERR_UNLIKELY;
}

int BleCentralRoutines::gatt_service_init(void) {
    // Local variables
    int rc;

    // 1. GATT service initialization
    ble_svc_gatt_init();

    // 2. Update GATT services counter
    rc = ble_gatts_count_cfg(gatt_server_services);
    if (rc != 0) {
        return rc;
    }

    // 3. Add GATT services
    rc = ble_gatts_add_svcs(gatt_server_services);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int BleCentralRoutines::gap_init(void) {
    // Local variables
    int rc = 0;

    // Initialize GAP service
    ble_svc_gap_init();

    // Set GAP device name
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d",
                 DEVICE_NAME, rc);
        return rc;
    }

    // Set GAP device appearance
    rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device appearance, error code: %d", rc);
        return rc;
    }
    return rc;
}

void BleCentralRoutines::ble_main_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    //nimble_port_freertos_deinit();
    vTaskDelete(NULL);
}

BlePeripheralRoutines::on_stack_reset(int reason) {
    ESP_LOGE(TAG, "Resetting state; reason=%d\n", reason);
}

BlePeripheralRoutines::on_stack_sync(void) {
    int rc;

    /*
     * To improve both throughput and stability, it is recommended to set the connection interval
     * as an integer multiple of the `MINIMUM_CONN_INTERVAL`. This `MINIMUM_CONN_INTERVAL` should
     * be calculated based on the total number of connections and the Transmitter/Receiver phy.
     *
     * Note that the `MINIMUM_CONN_INTERVAL` value should meet the condition that:
     *      MINIMUM_CONN_INTERVAL > ((MAX_TIME_OF_PDU * 2) + 150us) * CONN_NUM.
     *
     * For example, if we have 10 connections, maxmum TX/RX length is 251 and the phy is 1M, then
     * the `MINIMUM_CONN_INTERVAL` should be greater than ((261 * 8us) * 2 + 150us) * 10 = 43260us.
     *
     */
    rc = ble_gap_common_factor_set(true, (BLE_PREF_CONN_ITVL_MS * 1000) / 625);
    assert(rc == 0);

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* We will function as both the central and peripheral device, connecting to all peripherals
     * with the name of BLE_PEER_NAME. Meanwhile, a connectable advertising will be enabled.
     * In this example, we register two gap callback functions.
     *  - ble_cent_client_gap_event: Used by the central.
     *  - ble_cent_server_gap_event: Used by the peripheral.
     */
    ble_cent_advertise(); //FIX
    ble_cent_scan(); //FIX
}

void BleCentralRoutines::nimble_host_config_init(void)
{
    // Set host callbacks
    ble_hs_cfg.reset_cb = BlePeripheralRoutines::on_stack_reset;
    ble_hs_cfg.sync_cb = BlePeripheralRoutines::on_stack_sync;
    ble_hs_cfg.gatts_register_cb = BlePeripheralRoutines::gatt_server_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /*
    // Security manager configuration
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    */

    // Store host configuration
    ble_store_config_init();
}

BleCentralRoutines::BleCentralRoutines() {
    BleCentralRoutines::s_ble_multi_conn_num = 0;
    BleCentralRoutines::messaging_characteristic_val_handle = 0;
    
    int rc;
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if  (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return;
    }

    /* Initialize data structures to track connected peers. */
#if MYNEWT_VAL(BLE_INCL_SVC_DISCOVERY) || MYNEWT_VAL(BLE_GATT_CACHING_INCLUDE_SERVICES)
    rc = peer_init(BLE_PEER_MAX_NUM, BLE_PEER_MAX_NUM, BLE_PEER_MAX_NUM, BLE_PEER_MAX_NUM, BLE_PEER_MAX_NUM);
    assert(rc == 0);
#else
    rc = peer_init(BLE_PEER_MAX_NUM, BLE_PEER_MAX_NUM, BLE_PEER_MAX_NUM, BLE_PEER_MAX_NUM);
    assert(rc == 0);
#endif
    /* Set the default device name. We will act as both central and peripheral. */
    rc = ble_svc_gap_device_name_set("esp-ble-role-coex");
    assert(rc == 0);

    // GAP service initialization
    rc = gap_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    // GATT server initialization
    rc = gatt_service_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", rc);
        return;
    }

    // NimBLE host configuration initialization
    nimble_host_config_init();
}