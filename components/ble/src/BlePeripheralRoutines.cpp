#include "BlePeripheralRoutines.hpp"

extern "C" void ble_store_config_init(void);

//Static variable definitions (ugly, I know)
    // Local variables
    uint8_t BlePeripheralRoutines::own_addr_type = 0;
    uint8_t BlePeripheralRoutines::addr_val[6] = {0};
    uint8_t BlePeripheralRoutines::messaging_characteristic_val[2] = {0};
    uint16_t BlePeripheralRoutines::messaging_characteristic_val_handle = 0;
    uint16_t BlePeripheralRoutines::messaging_characteristic_conn_handle = 0;
    bool BlePeripheralRoutines::messaging_characteristic_conn_handle_inited = false;
    bool BlePeripheralRoutines::messaging_indication_status = false;
    uint8_t BlePeripheralRoutines::s_ble_multi_conn_num = 0;

//Central
/**
 * Initiates the GAP general discovery procedure.
 */
void BlePeripheralRoutines::scan_init() {
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
                          &coded_disc_params, BlePeripheralRoutines::central_client_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d\n", rc);
    }
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

int BlePeripheralRoutines::central_client_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_EXT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->ext_disc.data, event->ext_disc.length_data);

        /* An advertisement report was received during GAP discovery. */
        if ((rc == 0) && fields.name && (fields.name_len >= strlen(BLE_PEER_NAME)) &&
            !strncmp((const char *)fields.name, BLE_PEER_NAME, strlen(BLE_PEER_NAME))) {
            BlePeripheralRoutines::central_connect(&event->ext_disc);
        }

        return 0;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connection established. Handle:%d, Total:%d", event->connect.conn_handle,
                     ++BlePeripheralRoutines::s_ble_multi_conn_num);
            /* Remember peer. */
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to add peer; rc=%d\n", rc);
            } else {
                if(rc != 0) {
                    ESP_LOGE(TAG, "Failed to discover services; rc=%d\n", rc);
                }
            }
        } else {
            /* Connection attempt failed; resume scanning. */
            ESP_LOGE(TAG, "Central: Connection failed; status=0x%x\n", event->connect.status);
        }
        BlePeripheralRoutines::scan_init();
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        BlePeripheralRoutines::print_conn_desc(&event->disconnect.conn);

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        ESP_LOGI(TAG, "Central disconnected; Handle:%d, Reason=%d, Total:%d",
                 event->disconnect.conn.conn_handle, event->disconnect.reason,
                 --BlePeripheralRoutines::s_ble_multi_conn_num);

        /* Resume scanning. */
        BlePeripheralRoutines::scan_init();
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
 * Connects to the sender of the specified advertisement. The advertisement must contain its full
 * name which we will compare with 'BLE_PEER_NAME'.
 */
void BlePeripheralRoutines::central_connect(void *disc) {
    ble_addr_t *peer_addr;
    struct ble_gap_multi_conn_params multi_conn_params;
    struct ble_gap_conn_params uncoded_conn_param;
    struct ble_gap_conn_params coded_conn_param;
    int rc;

    if (BlePeripheralRoutines::s_ble_multi_conn_num >= BLE_PEER_MAX_NUM) {
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
        ESP_LOGI(TAG, "Created connection. -> peer addr %s",  addr_str(peer_addr->val));
    }
}

//Peripheral

////GAP

//////GAP functions
void BlePeripheralRoutines::format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
            addr[2], addr[3], addr[4], addr[5]);
}

void BlePeripheralRoutines::print_conn_desc(struct ble_gap_conn_desc *desc) {
    // Local variables
    char addr_str[18] = {0};

    // Connection handle
    ESP_LOGI(TAG, "connection handle: %d", desc->conn_handle);

    // Local ID address
    BlePeripheralRoutines::format_addr(addr_str, desc->our_id_addr.val);
    ESP_LOGI(TAG, "device id address: type=%d, value=%s",
             desc->our_id_addr.type, addr_str);

    // Peer ID address
    BlePeripheralRoutines::format_addr(addr_str, desc->peer_id_addr.val);
    ESP_LOGI(TAG, "peer id address: type=%d, value=%s", desc->peer_id_addr.type,
             addr_str);

    // Connection info
    ESP_LOGI(TAG,
             "conn_itvl=%d, conn_latency=%d, supervision_timeout=%d, "
             "encrypted=%d, authenticated=%d, bonded=%d\n",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
             desc->sec_state.encrypted, desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

void BlePeripheralRoutines::set_random_addr(void) {
    // Local variables
    int rc = 0;
    ble_addr_t addr;

    // Generate new non-resolvable private address
    rc = ble_hs_id_gen_rnd(0, &addr);
    assert(rc == 0);

    // Set address
    rc = ble_hs_id_set_rnd(addr.val);
    assert(rc == 0);
}

int BlePeripheralRoutines::gap_event_handler(struct ble_gap_event *event, void *arg) {
    // Local variables
    int rc = 0;
    struct ble_gap_conn_desc desc;

    // Handle different GAP event
    ESP_LOGI(TAG, "Event received. Type: %d", event->type);

    switch (event->type) {

        // Connect event
        case BLE_GAP_EVENT_CONNECT:
            // A new connection was established or a connection attempt failed.
            ESP_LOGI(TAG, "connection %s; status=%d",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);

            // Connection succeeded
            if (event->connect.status == 0) {
                // Check connection handle
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (rc != 0) {
                    ESP_LOGE(TAG,
                            "failed to find connection by handle, error code: %d",
                            rc);
                    return rc;
                }

                // Print connection descriptor
                BlePeripheralRoutines::print_conn_desc(&desc);

                // Try to update connection parameters
                struct ble_gap_upd_params params = {.itvl_min = desc.conn_itvl,
                                                    .itvl_max = desc.conn_itvl,
                                                    .latency = 3,
                                                    .supervision_timeout =
                                                        desc.supervision_timeout};
                rc = ble_gap_update_params(event->connect.conn_handle, &params);
                if (rc != 0) {
                    ESP_LOGE(
                        TAG,
                        "failed to update connection parameters, error code: %d",
                        rc);
                    return rc;
                }
            }
            // Connection failed, restart advertising
            else {
                BlePeripheralRoutines::start_advertising();
            }
            return rc;

        // Disconnect event
        case BLE_GAP_EVENT_DISCONNECT:
            // A connection was terminated, print connection descriptor
            ESP_LOGI(TAG, "disconnected from peer; reason=%d",
                    event->disconnect.reason);

            // Restart advertising
            BlePeripheralRoutines::start_advertising();
            return rc;

        // Connection parameters update event
        case BLE_GAP_EVENT_CONN_UPDATE:
            // The central has updated the connection parameters.
            ESP_LOGI(TAG, "connection updated; status=%d",
                    event->conn_update.status);

            // Print connection descriptor
            rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            if (rc != 0) {
                ESP_LOGE(TAG, "failed to find connection by handle, error code: %d",
                        rc);
                return rc;
            }
            BlePeripheralRoutines::print_conn_desc(&desc);
            return rc;

        // Advertising complete event
        case BLE_GAP_EVENT_ADV_COMPLETE:
            // Advertising completed, restart advertising
            ESP_LOGI(TAG, "advertise complete; reason=%d",
                    event->adv_complete.reason);
            BlePeripheralRoutines::start_advertising();
            return rc;

        // Notification sent event
        case BLE_GAP_EVENT_NOTIFY_TX:
            if ((event->notify_tx.status != 0) &&
                (event->notify_tx.status != BLE_HS_EDONE)) {
                // Print notification info on error
                ESP_LOGI(TAG,
                        "notify event; conn_handle=%d attr_handle=%d "
                        "status=%d is_indication=%d",
                        event->notify_tx.conn_handle, event->notify_tx.attr_handle,
                        event->notify_tx.status, event->notify_tx.indication);
            }
            return rc;

        // Subscribe event
        case BLE_GAP_EVENT_SUBSCRIBE:
            /// Print subscription info to log
            ESP_LOGI(TAG,
                    "subscribe event; conn_handle=%d attr_handle=%d "
                    "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                    event->subscribe.conn_handle, event->subscribe.attr_handle,
                    event->subscribe.reason, event->subscribe.prev_notify,
                    event->subscribe.cur_notify, event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);

            // GATT subscribe event callback
            rc = BlePeripheralRoutines::gatt_server_subscribe_cb(event); 
            if (rc == BLE_ATT_ERR_INSUFFICIENT_AUTHEN) {
                // Request connection encryption
                return ble_gap_security_initiate(event->subscribe.conn_handle);
            }
            return rc;

        // MTU update event
        case BLE_GAP_EVENT_MTU:
            // Print MTU update info to log
            ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                    event->mtu.conn_handle, event->mtu.channel_id,
                    event->mtu.value);
            return rc;

        // Encryption change event 
        case BLE_GAP_EVENT_ENC_CHANGE:
            // Encryption has been enabled or disabled for this connection.
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG, "connection encrypted!");
            } else {
                ESP_LOGE(TAG, "connection encryption failed, status: %d",
                        event->enc_change.status);
            }
            return rc;

        // Repeat pairing event
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            // Delete the old bond
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            if (rc != 0) {
                ESP_LOGE(TAG, "failed to find connection, error code %d", rc);
                return rc;
            }
            ble_store_util_delete_peer(&desc.peer_id_addr);

            // Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
            // continue with pairing operation
            ESP_LOGI(TAG, "repairing...");
            return BLE_GAP_REPEAT_PAIRING_RETRY;

        // Passkey action event
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            // Display action
            if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
                // Generate passkey
                struct ble_sm_io pkey = {0};
                pkey.action = event->passkey.params.action;
                pkey.passkey = 100000 + esp_random() % 900000;
                ESP_LOGI(TAG, "enter passkey %" PRIu32 " on the peer side",
                        pkey.passkey);
                rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
                if (rc != 0) {
                    ESP_LOGE(TAG,
                            "failed to inject security manager io, error code: %d",
                            rc);
                    return rc;
                }
            }
        return rc;
    }
    return rc;
}

void BlePeripheralRoutines::start_advertising(void) {
    // Local variables
    int rc = 0;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    // Set advertising flags
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Set device name
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    // Set device tx power
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;

    // Set device appearance
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;

    // Set device LE role
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    // Set advertisement fields
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }

    // Set device address
    rsp_fields.device_addr = BlePeripheralRoutines::addr_val;
    rsp_fields.device_addr_type = BlePeripheralRoutines::own_addr_type;
    rsp_fields.device_addr_is_present = 1;

    // Set URI
    rsp_fields.uri = BlePeripheralRoutines::esp_uri;
    rsp_fields.uri_len = sizeof(BlePeripheralRoutines::esp_uri);

    // Set advertising interval
    rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(500);
    rsp_fields.adv_itvl_is_present = 1;

    // Set scan response fields
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set scan response data, error code: %d", rc);
        return;
    }

    // Set connectable and general discoverable mode to be a beacon
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // Set advertising interval
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

    // Start advertising
    rc = ble_gap_adv_start(BlePeripheralRoutines::own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           BlePeripheralRoutines::gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}

void BlePeripheralRoutines::adv_init(void) {
    // Local variables
    int rc = 0;
    uint8_t instance = 0;
    char addr_str[18] = {0};

    // Make sure we have proper BT identity address set
    BlePeripheralRoutines::set_random_addr();
    rc = ble_hs_util_ensure_addr(1);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    // Figure out BT address to use while advertising
    rc = ble_hs_id_infer_auto(0, &BlePeripheralRoutines::own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    // Copy device address to addr_val
    rc = ble_hs_id_copy_addr(BlePeripheralRoutines::own_addr_type, BlePeripheralRoutines::addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }
    BlePeripheralRoutines::format_addr(addr_str, BlePeripheralRoutines::addr_val);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    // Start advertising.
    BlePeripheralRoutines::start_advertising();
}

bool BlePeripheralRoutines::is_connection_encrypted(uint16_t conn_handle) {
    // Local variables
    int rc = 0;
    struct ble_gap_conn_desc desc;

    // Print connection descriptor
    rc = ble_gap_conn_find(conn_handle, &desc);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to find connection by handle, error code: %d",
                 rc);
        return false;
    }

    return desc.sec_state.encrypted;
}

int BlePeripheralRoutines::messaging_characteristic_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // Local variables
    int rc;

    // Handle access events
    // Note: Heart rate characteristic is read only
    switch (ctxt->op) {

    // Read characteristic event
    case BLE_GATT_ACCESS_OP_READ_CHR:
        // Verify connection handle
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        // Verify attribute handle
        if (attr_handle == BlePeripheralRoutines::messaging_characteristic_val_handle) {
            // Update access buffer value
            //BlePeripheralRoutines::messaging_characteristic_val[1] = 0; //CHANGE LATERRRRRR
            rc = os_mbuf_append(ctxt->om, &BlePeripheralRoutines::messaging_characteristic_val,
                                sizeof(BlePeripheralRoutines::messaging_characteristic_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    // Unknown event
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to heart rate characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

bool BlePeripheralRoutines::send_messaging_indication(uint8_t data) {
    // Check indication and security status
    if (BlePeripheralRoutines::messaging_characteristic_conn_handle_inited &&
    BlePeripheralRoutines::messaging_indication_status &&
    BlePeripheralRoutines::is_connection_encrypted(messaging_characteristic_conn_handle)) {
        BlePeripheralRoutines::messaging_characteristic_val[1] = data;
        ble_gatts_indicate(BlePeripheralRoutines::messaging_characteristic_conn_handle,
                           BlePeripheralRoutines::messaging_characteristic_val_handle);
        return true;
    }

    return false;
}

//
//*  Handle GATT attribute register events
//*      - Service register event
//*      - Characteristic register event
//*      - Descriptor register event

void BlePeripheralRoutines::gatt_server_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
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

//
//*  GATT server subscribe event callback
//*      1. Update messaging subscription status


int BlePeripheralRoutines::gatt_server_subscribe_cb(struct ble_gap_event *event) {
    // Check attribute handle
    if (event->subscribe.attr_handle == BlePeripheralRoutines::messaging_characteristic_val_handle) {
        // Update messaging subscription status
        BlePeripheralRoutines::messaging_characteristic_conn_handle = event->subscribe.conn_handle;
        BlePeripheralRoutines::messaging_characteristic_conn_handle_inited = true;
        BlePeripheralRoutines::messaging_indication_status = event->subscribe.cur_indicate;

        // Check security status
        if (!BlePeripheralRoutines::is_connection_encrypted(event->subscribe.conn_handle)) {
            ESP_LOGE(TAG, "failed to subscribe to heart rate measurement, "
                          "connection not encrypted!");
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }
    }
    return 0;
}

//
//*  GATT server initialization
//*      1. Initialize GATT service
//*      2. Update NimBLE host GATT services counter
//*      3. Add GATT services to server

int BlePeripheralRoutines::gatt_service_init(void) {
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


int BlePeripheralRoutines::gap_init(void) {
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

//
//*  Stack event callback functions
//*      - on_stack_reset is called when host resets BLE stack due to errors
//*      - on_stack_sync is called when host has synced with controller

void BlePeripheralRoutines::on_stack_reset(int reason)
{
    // On reset, print reset reason to console
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

void BlePeripheralRoutines::on_stack_sync(void)
{
    int rc = 0;
    rc = ble_gap_common_factor_set(true, (BLE_PREF_CONN_ITVL_MS * 1000) / 625);
    assert(rc == 0);

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    // On stack sync, do advertising initialization
    BlePeripheralRoutines::adv_init();
    BlePeripheralRoutines::scan_init();
}

void BlePeripheralRoutines::nimble_host_config_init(void)
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

void BlePeripheralRoutines::ble_main_task(void *param)
{
    // Task entry log
    ESP_LOGI(TAG, "nimble host task has been started!");

    // This function won't return until nimble_port_stop() is executed
    nimble_port_run();

    // Clean up at exit
    vTaskDelete(NULL);
}

BlePeripheralRoutines::BlePeripheralRoutines() {
    // Local variables
    BlePeripheralRoutines::addr_val[6] = {0};
    BlePeripheralRoutines::messaging_characteristic_val[2] = {0};
    BlePeripheralRoutines::messaging_characteristic_conn_handle = 0;
    BlePeripheralRoutines::messaging_characteristic_conn_handle_inited = false;
    BlePeripheralRoutines::messaging_indication_status = false;

    int rc = 0;
    esp_err_t ret = ESP_OK;

    // NVS flash initialization
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    // NimBLE host stack initialization
    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

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