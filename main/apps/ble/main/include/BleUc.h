#ifdef __cplusplus
extern "C" {
#endif

class BleUc {
    private:
        /* Private functions*/
        static void on_stack_reset(int reason);
        static void on_stack_sync(void);
        static void nimble_host_config_init(void);
        
    public:
        BleUc();
        static void nimble_host_task(void *param);

};

#ifdef __cplusplus
}
#endif