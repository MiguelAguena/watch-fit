#pragma once
#include "common.h"
#define BLE_PEER_NAME           "esp-multi-conn"
#define BLE_PEER_MAX_NUM        (MYNEWT_VAL(BLE_MAX_CONNECTIONS) - 1)
#define BLE_PREF_EVT_LEN_MS     (5)
#define BLE_PREF_CONN_ITVL_MS   (BLE_PEER_MAX_NUM * BLE_PREF_EVT_LEN_MS)

class BleCentralRoutines {
    private:
        static uint8_t s_ble_multi_conn_num;
        static void gatt_server_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
        static int gatt_service_send_to_peers(const struct peer *peer, void *arg);
        int gap_init(void);
        int gatt_service_init(void);
        
        void static on_stack_reset(int reason);
        void static on_stack_sync(void);
        void nimble_host_config_init(void);

    public:
        static constexpr ble_uuid16_t messaging_service_uuid = BLE_UUID16_INIT(0x180D);
        static constexpr ble_uuid16_t messaging_characteristic_uuid = BLE_UUID16_INIT(0x2A37);
        static constexpr ble_uuid_t *remote_service_uuid =
            BLE_UUID128_DECLARE(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                                0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);


        static uint16_t messaging_characteristic_val_handle;

        static uint8_t ext_advertise_pattern_1[] = {
            0x02, 0x01, 0x06,
            0x14, 0X09, 'e', 's', 'p', '-', 'b', 'l', 'e', '-', 'r', 'o', 'l', 'e', '-', 'c', 'o', 'e', 'x', '-', 'e',
        };

        static int gatt_service_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
        BleCentralRoutines();
        void ble_main_task(void *param);

        inline static struct ble_gatt_svc_def gatt_server_services[] = {
            {
                /*** Service ***/
                .type = BLE_GATT_SVC_TYPE_PRIMARY,
                .uuid = &BleCentralRoutines::messaging_service_uuid.u,
                .characteristics = (struct ble_gatt_chr_def[])
                {
                    {
                        .uuid = &BleCentralRoutines::messaging_characteristic_uuid.u,
                        .access_cb = &BleCentralRoutines::gatt_service_access,
                        .flags = BLE_GATT_CHR_F_WRITE,
                        .val_handle = &BleCentralRoutines::messaging_characteristic_val_handle,
                    },
                    {
                        0, /* No more characteristics in this service. */
                    }
                },
            },

            {
                0, /* No more services. */
            },
        };
}