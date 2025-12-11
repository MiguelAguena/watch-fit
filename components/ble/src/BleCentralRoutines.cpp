#include "BleCentralRoutines.hpp"

extern "C" void ble_store_config_init(void);

//Static variable definitions (ugly, I know)
    // Local variables
    uint8_t BleCentralRoutines::s_ble_multi_conn_num = 0;
    uint16_t BleCentralRoutines::messaging_characteristic_val_handle = 0;

/**
 * Called when service discovery of the specified peer has completed.
 */
void BleCentralRoutines::central_on_discovery_complete(const struct peer *peer, int status, void *arg) {
    if (status != 0) {
        /* Service discovery failed.  Terminate the connection. */
        ESP_LOGE(TAG, "Error: Service discovery failed; status=%d conn_handle=%d", status,
                 peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    ESP_LOGD(TAG, "Service discovery complete; status=%d conn_handle=%d\n", status,
             peer->conn_handle);
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The application associates a GAP
 * event callback with each connection that is established. This callback will be only used by the
 * central.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */

int BleCentralRoutines::central_client_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_EXT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->ext_disc.data, event->ext_disc.length_data);

        /* An advertisement report was received during GAP discovery. */
        if ((rc == 0) && fields.name && (fields.name_len >= strlen(BLE_PEER_NAME)) &&
            !strncmp((const char *)fields.name, BLE_PEER_NAME, strlen(BLE_PEER_NAME))) {
            BleCentralRoutines::central_connect(&event->ext_disc);
        }

        return 0;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connection established. Handle:%d, Total:%d", event->connect.conn_handle,
                     ++s_ble_multi_conn_num);
            /* Remember peer. */
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to add peer; rc=%d\n", rc);
            } else {
                /* Perform service discovery */
                rc = peer_disc_svc_by_uuid(event->connect.conn_handle, remote_service_uuid,
                                        BleCentralRoutines::central_on_discovery_complete, NULL);
                if(rc != 0) {
                    ESP_LOGE(TAG, "Failed to discover services; rc=%d\n", rc);
                }
            }
        } else {
            /* Connection attempt failed; resume scanning. */
            ESP_LOGE(TAG, "Central: Connection failed; status=0x%x\n", event->connect.status);
        }
        ble_cent_scan();
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        print_conn_desc(&event->disconnect.conn);

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        ESP_LOGI(TAG, "Central disconnected; Handle:%d, Reason=%d, Total:%d",
                 event->disconnect.conn.conn_handle, event->disconnect.reason,
                 --s_ble_multi_conn_num);

        /* Resume scanning. */
        ble_cent_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "discovery complete; reason=%d\n", event->disc_complete.reason);
        return 0;

#if MYNEWT_VAL(BLE_POWER_CONTROL)
    case BLE_GAP_EVENT_TRANSMIT_POWER:
        ESP_LOGD(TAG, "Transmit power event : status=%d conn_handle=%d reason=%d phy=%d "
                 "power_level=%x power_level_flag=%d delta=%d", event->transmit_power.status,
                 event->transmit_power.conn_handle, event->transmit_power.reason,
                 event->transmit_power.phy, event->transmit_power.transmit_power_level,
                 event->transmit_power.transmit_power_level_flag, event->transmit_power.delta);
        return 0;

    case BLE_GAP_EVENT_PATHLOSS_THRESHOLD:
        ESP_LOGD(TAG, "Pathloss threshold event : conn_handle=%d current path loss=%d "
                 "zone_entered =%d", event->pathloss_threshold.conn_handle,
                 event->pathloss_threshold.current_path_loss, event->pathloss_threshold.zone_entered);
        return 0;
#endif

    default:
        return 0;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The application associates a GAP
 * event callback with each connection that is established. This callback will be only used by the
 * peripheral.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
int BleCentralRoutines::central_server_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* The connectable adv has been established. We will act as the peripheral. */
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Peripheral connected to central. Handle:%d", event->connect.conn_handle);
        } else {
            ESP_LOGE(TAG, "Peripheral: Connection failed; status=0x%x\n", event->connect.status);
            ble_cent_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Peripheral disconnected; Handle:%d, Reason=%d",
                 event->disconnect.conn.conn_handle, event->disconnect.reason);
        ble_cent_advertise();
        return 0;

    default:
        return 0;
    }
}

/**
 * Connects to the sender of the specified advertisement.The advertisement must contain its full
 * name which we will compare with 'BLE_PEER_NAME'.
 */
void BleCentralRoutines::central_connect(void *disc) {
    ble_addr_t own_addr;
    ble_addr_t *peer_addr;
    struct ble_gap_multi_conn_params multi_conn_params;
    struct ble_gap_conn_params uncoded_conn_param;
    struct ble_gap_conn_params coded_conn_param;
    int rc;

    if (s_ble_multi_conn_num >= BLE_PEER_MAX_NUM) {
        return;
    }

#if !(MYNEWT_VAL(BLE_HOST_ALLOW_CONNECT_WITH_SCAN))
    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }
#endif

    /* We won't connect to the same device. Change our static random address to simulate
     * multi-connection with only one central and one peripheral.
     */
    rc = ble_hs_id_gen_rnd(0, &own_addr);
    assert(rc == 0);
    rc = ble_hs_id_set_rnd(own_addr.val);
    assert(rc == 0);

    peer_addr = &((struct ble_gap_ext_disc_desc *)disc)->addr;

    /* The connection and scan parameters for uncoded phy (1M & 2M). */
    uncoded_conn_param.scan_itvl = BLE_GAP_SCAN_ITVL_MS(300);
    uncoded_conn_param.scan_window = BLE_GAP_SCAN_WIN_MS(100);
    uncoded_conn_param.itvl_min = BLE_GAP_CONN_ITVL_MS(BLE_PREF_CONN_ITVL_MS);
    uncoded_conn_param.itvl_max = BLE_GAP_CONN_ITVL_MS(BLE_PREF_CONN_ITVL_MS);
    uncoded_conn_param.latency = 0;
    uncoded_conn_param.supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(BLE_PREF_CONN_ITVL_MS * 30);
    uncoded_conn_param.min_ce_len = 0;
    uncoded_conn_param.max_ce_len =  BLE_GAP_CONN_ITVL_MS(BLE_PREF_CONN_ITVL_MS);

    /* The connection and scan parameters for coded phy (125k & 500k) */
    coded_conn_param.scan_itvl = BLE_GAP_SCAN_ITVL_MS(300);
    coded_conn_param.scan_window = BLE_GAP_SCAN_WIN_MS(200);
    coded_conn_param.itvl_min = BLE_GAP_CONN_ITVL_MS(BLE_PREF_CONN_ITVL_MS);
    coded_conn_param.itvl_max = BLE_GAP_CONN_ITVL_MS(BLE_PREF_CONN_ITVL_MS);
    coded_conn_param.latency = 0;
    coded_conn_param.supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(BLE_PREF_CONN_ITVL_MS * 30);
    coded_conn_param.min_ce_len = 0;
    coded_conn_param.max_ce_len =  BLE_GAP_CONN_ITVL_MS(BLE_PREF_CONN_ITVL_MS);

    /* The parameters for multi-connect. We expect that this connection has at least
     * BLE_PREF_EVT_LEN_MS every interval to Rx and Tx.
     */
    multi_conn_params.scheduling_len_us = BLE_PREF_EVT_LEN_MS * 1000;
    multi_conn_params.own_addr_type = BLE_OWN_ADDR_RANDOM;
    multi_conn_params.peer_addr = peer_addr;
    multi_conn_params.duration_ms = 8000;
    multi_conn_params.phy_mask = BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK |
                                 BLE_GAP_LE_PHY_CODED_MASK;
    multi_conn_params.phy_1m_conn_params = &uncoded_conn_param;
    multi_conn_params.phy_2m_conn_params = &uncoded_conn_param;
    multi_conn_params.phy_coded_conn_params = &coded_conn_param;

    rc = ble_gap_multi_connect(&multi_conn_params, central_client_gap_event, NULL);

    if (rc) {
        ESP_LOGE(TAG, "Error: Failed to connect to device; addr_type=%d addr=%s; rc=%d\n",
                    peer_addr->type, addr_str(peer_addr->val), rc);
    } else {
        ESP_LOGI(TAG, "Create connection. -> peer addr %s",  addr_str(peer_addr->val));
    }
}

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

BleCentralRoutines::on_stack_reset(int reason) {
    ESP_LOGE(TAG, "Resetting state; reason=%d\n", reason);
}

BleCentralRoutines::on_stack_sync(void) {
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
    BleCentralRoutines::central_advertise();
    BleCentralRoutines::central_scan();
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
void BleCentralRoutines::central_advertise() {
    int rc;
    struct ble_gap_ext_adv_params params;
    struct os_mbuf *data;
    uint8_t instance = 0;

    /* First check if any instance is already active */
    if(ble_gap_ext_adv_active(instance)) {
        return;
    }

    memset (&params, 0, sizeof(params));

    /* Enable connectable advertising */
    params.connectable = 1;

    params.own_addr_type = BLE_OWN_ADDR_PUBLIC;
    params.primary_phy = BLE_HCI_LE_PHY_1M;
    params.secondary_phy = BLE_HCI_LE_PHY_1M;
    params.tx_power = 127;
    params.sid = 1;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(300);
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(300);

    rc = ble_gap_ext_adv_configure(instance, &params, NULL,
                                   BleCentralRoutines::central_server_gap_event, NULL);
    assert(rc == 0);

    /* Get mbuf for adv data */
    data = os_msys_get_pkthdr(sizeof(ext_advertise_pattern_1), 0);
    assert(data);
    rc = os_mbuf_append(data, ext_advertise_pattern_1, sizeof(ext_advertise_pattern_1));
    assert(rc == 0);

    rc = ble_gap_ext_adv_set_data(instance, data);
    assert(rc == 0);

    /* Start advertising */
    rc = ble_gap_ext_adv_start(instance, 0, 0);
    assert(rc == 0);

    if (rc) {
        ESP_LOGE(TAG, "Failed to enable advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * Initiates the GAP general discovery procedure.
 */
void BleCentralRoutines::central_scan() {
    int rc;

    if (ble_gap_disc_active()) {
        return;
    }

    struct ble_gap_ext_disc_params uncoded_disc_params;
    struct ble_gap_ext_disc_params coded_disc_params;

    /* Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    uncoded_disc_params.passive = 1;
    uncoded_disc_params.itvl = BLE_GAP_SCAN_ITVL_MS(500);
    uncoded_disc_params.window = BLE_GAP_SCAN_WIN_MS(200);

    coded_disc_params.passive = 1;
    coded_disc_params.itvl = BLE_GAP_SCAN_ITVL_MS(500);
    coded_disc_params.window = BLE_GAP_SCAN_WIN_MS(300);

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    rc = ble_gap_ext_disc(BLE_OWN_ADDR_PUBLIC, 0, 0, 1, 0, 0, &uncoded_disc_params,
                          &coded_disc_params, BleCentralRoutines::central_client_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d\n", rc);
    }
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