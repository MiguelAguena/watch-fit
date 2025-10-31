//GAP includes and defines
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200
#define BLE_GAP_URI_PREFIX_HTTPS 0x17
#define BLE_GAP_LE_ROLE_PERIPHERAL 0x00

//GATT includes and defines
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

class BleUc {
    private:
        //Private attributes
        static constexpr ble_uuid16_t heart_rate_svc_uuid = BLE_UUID16_INIT(0x180D);
        static constexpr ble_uuid16_t heart_rate_chr_uuid = BLE_UUID16_INIT(0x2A37);
        static constexpr uint8_t esp_uri[] = {BLE_GAP_URI_PREFIX_HTTPS, '/', '/', 'e', 's', 'p', 'r', 'e', 's', 's', 'i', 'f', '.', 'c', 'o', 'm'};
        
        uint8_t own_addr_type;
        uint8_t addr_val[6];
        
        uint8_t heart_rate_chr_val[2];
        uint16_t heart_rate_chr_val_handle;
        uint16_t heart_rate_chr_conn_handle;
        bool heart_rate_chr_conn_handle_inited;
        bool heart_rate_ind_status;
        static const struct ble_gatt_svc_def gatt_svr_svcs[];

        //GAP private functions
        inline void format_addr(char *addr_str, uint8_t addr[]);
        void print_conn_desc(struct ble_gap_conn_desc *desc);
        void set_random_addr(void);
        int gap_event_handler(struct ble_gap_event *event, void *arg);
        void start_advertising(void);
        void adv_init(void);
        bool is_connection_encrypted(uint16_t conn_handle);
        int gap_init(void);

        //GATT private functions
        int heart_rate_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
        void send_heart_rate_indication(void);
        void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
        int gatt_svr_subscribe_cb(struct ble_gap_event *event);
        int gatt_svc_init(void);

        //BleUc private functions
        void on_stack_reset(int reason);
        void on_stack_sync(void);
        void nimble_host_config_init(void);
        
    public:
        BleUc();
        void nimble_host_task(void *param);
};