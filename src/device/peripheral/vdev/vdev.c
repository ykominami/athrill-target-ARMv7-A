#include "device.h"
#include "inc/vdev.h"
#include "device.h"
#include "std_errno.h"
#include "mpu_types.h"
#include "cpuemu_ops.h"
#include "athrill_mpthread.h"
#include "mpu_ops.h"
#include "udp/udp_comm.h"
#include "std_cpu_ops.h"
#include "mpu_ops.h"
#include "assert.h"
#include <string.h>
#include <stdio.h>

//#define VDEV_IO_MMAP
#define VDEV_IO_UDP

static Std_ReturnType vdev_get_data8(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint8 *data);
static Std_ReturnType vdev_get_data16(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint16 *data);
static Std_ReturnType vdev_get_data32(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint32 *data);
static Std_ReturnType vdev_put_data8(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint8 data);
static Std_ReturnType vdev_put_data16(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint16 data);
static Std_ReturnType vdev_put_data32(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint32 data);
static Std_ReturnType vdev_get_pointer(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint8 **data);

MpuAddressRegionOperationType	vdev_memory_operation = {
		.get_data8 		= 	vdev_get_data8,
		.get_data16		=	vdev_get_data16,
		.get_data32		=	vdev_get_data32,

		.put_data8 		= 	vdev_put_data8,
		.put_data16		=	vdev_put_data16,
		.put_data32		=	vdev_put_data32,

		.get_pointer	= vdev_get_pointer,
};


typedef struct {
	UdpCommConfigType config;
#ifdef VDEV_IO_UDP
	UdpCommType comm;
	const char *remote_ipaddr;
#else /* VDEV_IO_MMAP */
	uint32		tx_data_addr;
	uint8		*tx_data;
	uint32		rx_data_addr;
	uint8		*rx_data;
#endif
	MpuAddressRegionType *region;
	uint32		cpu_freq;
	uint64 		vdev_sim_time[VDEV_SIM_INX_NUM]; /* usec */
} VdevControlType;

#ifdef VDEV_IO_UDP
static MpthrIdType vdev_thrid;
Std_ReturnType mpthread_init(void);
extern Std_ReturnType mpthread_register(MpthrIdType *id, MpthrOperationType *op);

static Std_ReturnType vdev_thread_do_init(MpthrIdType id);
static Std_ReturnType vdev_thread_do_proc(MpthrIdType id);
static MpthrOperationType vdev_op = {
	.do_init = vdev_thread_do_init,
	.do_proc = vdev_thread_do_proc,
};

static char *remote_ipaddr = "127.0.0.1";
#endif

static VdevControlType vdev_control;

void device_init_vdev(MpuAddressRegionType *region)
{
	vdev_control.region = region;
	vdev_control.config.is_wait = TRUE;

	vdev_control.cpu_freq = DEFAULT_CPU_FREQ; /* 100MHz */
	(void)cpuemu_get_devcfg_value("DEVICE_CONFIG_TIMER_FD", &vdev_control.cpu_freq);

#ifdef VDEV_IO_UDP
	{
		Std_ReturnType err;
		uint32 portno;
		(void)cpuemu_get_devcfg_string("DEBUG_FUNC_VDEV_TX_IPADDR", &remote_ipaddr);
		err = cpuemu_get_devcfg_value("DEBUG_FUNC_VDEV_TX_PORTNO", &portno);
		if (err != STD_E_OK) {
			printf("ERROR: can not load param DEBUG_FUNC_VDEV_TX_PORTNO\n");
			ASSERT(err == STD_E_OK);
		}
		vdev_control.config.client_port = (uint16)portno;
		err = cpuemu_get_devcfg_value("DEBUG_FUNC_VDEV_RX_PORTNO", &portno);
		if (err != STD_E_OK) {
			printf("ERROR: can not load param DEBUG_FUNC_VDEV_RX_PORTNO\n");
			ASSERT(err == STD_E_OK);
		}
		vdev_control.config.server_port = (uint16)portno;

		err = udp_comm_create(&vdev_control.config, &vdev_control.comm);
		ASSERT(err == STD_E_OK);

		mpthread_init();

		err = mpthread_register(&vdev_thrid, &vdev_op);
		ASSERT(err == STD_E_OK);

		err = mpthread_start_proc(vdev_thrid);
		ASSERT(err == STD_E_OK);
	}
#else /* VDEV_IO_MMAP */
	{
		Std_ReturnType err;
		err = cpuemu_get_devcfg_value_hex("DEBUG_FUNC_VDEV_MMAP_TX", &vdev_control.tx_data_addr);
		if (err != STD_E_OK) {
			printf("ERROR: can not load param DEBUG_FUNC_VDEV_MMAP_TX\n");
			ASSERT(err == STD_E_OK);
		}
		vdev_control.tx_data = mpu_address_get_ram(vdev_control.tx_data_addr, 1U);
		ASSERT(vdev_control.tx_data != NULL);

		err = cpuemu_get_devcfg_value_hex("DEBUG_FUNC_VDEV_MMAP_RX", &vdev_control.rx_data_addr);
		if (err != STD_E_OK) {
			printf("ERROR: can not load param DEBUG_FUNC_VDEV_MMAP_RX\n");
			ASSERT(err == STD_E_OK);
		}
		vdev_control.rx_data = mpu_address_get_ram(vdev_control.rx_data_addr, 1U);
		ASSERT(vdev_control.rx_data != NULL);
	}
#endif

	return;
}

#if 0
static uint64 unity_interval_vtime = 0; /* usec */
static struct timeval unity_notify_time = { 0, 0 };
static uint64 average_unity_notify_interval = 0;
static double average_unity_vtime_ratio = 0.0;
#define average_count	100

static uint64 vdev_get_unity_sim_time(DeviceClockType *dev_clock)
{
	if (vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU] == 0) {
		return 0;
	}
	if (cpu_is_halt_all() == FALSE) {
		return ( vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU] * ((uint64)vdev_control.cpu_freq) );
	}
#if 1
	struct timeval curr_time;
	struct timeval elaps;
	gettimeofday(&curr_time, NULL);

	/*
	 * adjust unity virtual time
	 */
	cpuemu_timeval_sub(&curr_time, &unity_notify_time, &elaps);
	uint64 real_time_elaps = (uint64)((double)cpuemu_timeval2usec(&elaps) * average_unity_vtime_ratio);
#if 0
	{
		static int count = 0;
		if (((count++) % 1000000) == 0) {
			printf("elaps=%llu\n", real_time_elaps);
		}
	}
#endif
	//return ( vdev_udp_control.vdev_sim_time[VDEV_SIM_INX_YOU] * ((uint64)vdev_udp_control.cpu_freq) );
	return ( ( vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU] + (real_time_elaps) )  * ((uint64)vdev_control.cpu_freq) );

#else
	return ( vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU] * ((uint64)vdev_control.cpu_freq) );
#endif
}
#endif

void device_supply_clock_vdev(DeviceClockType *dev_clock)
{
	uint64 interval;
	uint64 unity_sim_time;

#ifdef VDEV_IO_MMAP
	{
		uint64 curr_stime;
		memcpy((void*)&curr_stime, &vdev_control.rx_data[VDEV_SIM_TIME(VDEV_SIM_INX_ME)], 8U);
		vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU] = curr_stime;
	}
#endif

#if 1
	unity_sim_time = vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU] * ((uint64)vdev_control.cpu_freq);
#else
	unity_sim_time = vdev_get_unity_sim_time(dev_clock);
#endif

	vdev_control.vdev_sim_time[VDEV_SIM_INX_ME] = ( dev_clock->clock / ((uint64)vdev_control.cpu_freq) );

	if ((unity_sim_time != 0) && (dev_clock->min_intr_interval != DEVICE_CLOCK_MAX_INTERVAL)) {
		if ((unity_sim_time <= dev_clock->clock)) {
			interval = 2U;
			//printf("UNITY <= MICON:%llu %llu\n", vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU], vdev_control.vdev_sim_time[VDEV_SIM_INX_ME]);
		}
		else {
			//interval = (unity_sim_time - dev_clock->clock) + ((unity_interval_vtime  * ((uint64)vdev_udp_control.cpu_freq)) / 2);
			interval = (unity_sim_time - dev_clock->clock);
			//printf("UNITY > MICON:%llu %llu\n", vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU], vdev_control.vdev_sim_time[VDEV_SIM_INX_ME]);
		}
		if (interval < dev_clock->min_intr_interval) {
			dev_clock->min_intr_interval = interval;
		}
	}
#ifdef VDEV_IO_MMAP
	{
		memcpy(&vdev_control.tx_data[VDEV_SIM_TIME(VDEV_SIM_INX_ME)],  (void*)&vdev_control.vdev_sim_time[VDEV_SIM_INX_ME], 8U);
		memcpy(&vdev_control.tx_data[VDEV_SIM_TIME(VDEV_SIM_INX_YOU)], (void*)&vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU], 8U);
	}
#endif
	return;
}

#ifdef VDEV_IO_UDP
static Std_ReturnType vdev_thread_do_init(MpthrIdType id)
{
	//nothing to do
	return STD_E_OK;
}
#endif

#if 0
static void vdev_calc_predicted_virtual_time(uint64 prev_stime, uint64 curr_stime)
{
	static struct timeval prev_time = { 0, 0 };
	//static uint64 prev_athrill_virtual_time = 0;
	//uint64 current_athrill_virtual_time = vdev_udp_control.vdev_sim_time[VDEV_SIM_INX_ME];
	struct timeval elaps;
	cpuemu_timeval_sub(&unity_notify_time, &prev_time, &elaps);

	uint64 virtual_time_elaps = curr_stime - prev_stime;
	uint64 real_time_elaps = cpuemu_timeval2usec(&elaps);

	{
		static int count = 0;
		static uint64 virtual_time_elaps_sum = 0;
		static uint64 real_time_elaps_sum = 0;
		virtual_time_elaps_sum += virtual_time_elaps;
		real_time_elaps_sum += real_time_elaps;
		if (((++count) % average_count) == 0) {
			average_unity_notify_interval = real_time_elaps_sum/average_count;
			average_unity_vtime_ratio = (double)virtual_time_elaps_sum/(double)real_time_elaps_sum;
#if 0
			printf("virtual_elapls = %llu usec real_elapls = %llu rate=%lf athrill_elaps= %llu\n",
					virtual_time_elaps_sum/average_count, average_unity_notify_interval,
					average_unity_vtime_ratio,
					current_athrill_virtual_time - prev_athrill_virtual_time);
#endif
			virtual_time_elaps_sum = 0;
			real_time_elaps_sum = 0;
		}
	}
	prev_time = unity_notify_time;
	//prev_athrill_virtual_time = current_athrill_virtual_time;
}
#endif

#ifdef VDEV_IO_UDP
static Std_ReturnType vdev_thread_do_proc(MpthrIdType id)
{
	Std_ReturnType err;
	uint32 off = VDEV_RX_DATA_BASE - VDEV_BASE;
	uint64 curr_stime;

	while (1) {
		err = udp_comm_read(&vdev_control.comm);
		if (err != STD_E_OK) {
			continue;
		}
		//gettimeofday(&unity_notify_time, NULL);
		memcpy(&vdev_control.region->data[off], &vdev_control.comm.read_data.buffer[0], vdev_control.comm.read_data.len);
		memcpy((void*)&curr_stime, &vdev_control.comm.read_data.buffer[VDEV_SIM_TIME(VDEV_SIM_INX_ME)], 8U);

		//unity_interval_vtime = curr_stime - vdev_udp_control.vdev_sim_time[VDEV_SIM_INX_YOU];
		//vdev_calc_predicted_virtual_time(vdev_udp_control.vdev_sim_time[VDEV_SIM_INX_YOU], curr_stime);
		vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU] = curr_stime;
#if 0
		{
			uint32 count = 0;
			if ((count % 1000) == 0) {
				printf("%llu, %llu\n", vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU], vdev_control.vdev_sim_time[VDEV_SIM_INX_ME]);
			}
			count++;
		}
#endif
	}
	return STD_E_OK;
}
#endif


static Std_ReturnType vdev_get_data8(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint8 *data)
{
	uint32 off = (addr - region->start);
#ifdef VDEV_IO_UDP
	*data = *((uint8*)(&region->data[off]));
#else /* VDEV_IO_MMAP */
	*data = *((uint8*)(&vdev_control.rx_data[off]));
#endif
	return STD_E_OK;
}
static Std_ReturnType vdev_get_data16(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint16 *data)
{
	uint32 off = (addr - region->start);
#ifdef VDEV_IO_UDP
	*data = *((uint16*)(&region->data[off]));
#else /* VDEV_IO_MMAP */
	*data = *((uint16*)(&vdev_control.rx_data[off]));
#endif
	return STD_E_OK;
}
static Std_ReturnType vdev_get_data32(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint32 *data)
{
	uint32 off = (addr - region->start);
#ifdef VDEV_IO_UDP
	*data = *((uint32*)(&region->data[off]));
#else /* VDEV_IO_MMAP */
	*data = *((uint32*)(&vdev_control.rx_data[off]));
#endif
	return STD_E_OK;
}
static Std_ReturnType vdev_put_data8(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint8 data)
{
#ifdef VDEV_IO_UDP
	uint32 off = (addr - region->start);
	*((uint8*)(&region->data[off])) = data;
#endif

	if (addr == VDEV_TX_FLAG(0)) {
#ifdef VDEV_IO_UDP
		uint32 tx_off = VDEV_TX_DATA_BASE - region->start;
		Std_ReturnType err;
		memcpy(&vdev_control.comm.write_data.buffer[0], &region->data[tx_off], UDP_BUFFER_LEN);
		memcpy(&vdev_control.comm.write_data.buffer[VDEV_SIM_TIME(VDEV_SIM_INX_ME)],  (void*)&vdev_control.vdev_sim_time[VDEV_SIM_INX_ME], 8U);
		memcpy(&vdev_control.comm.write_data.buffer[VDEV_SIM_TIME(VDEV_SIM_INX_YOU)], (void*)&vdev_control.vdev_sim_time[VDEV_SIM_INX_YOU], 8U);
		vdev_control.comm.write_data.len = UDP_BUFFER_LEN;
		//printf("sim_time=%llu\n", vdev_udp_control.vdev_sim_time[VDEV_SIM_INX_ME]);
		err = udp_comm_remote_write(&vdev_control.comm, remote_ipaddr);
		if (err != STD_E_OK) {
			printf("WARNING: vdevput_data8: udp send error=%d\n", err);
		}
#endif
	}
	else {
#ifdef VDEV_IO_MMAP
		uint32 off = addr - VDEV_TX_DATA_BASE;
		*((uint8*)(&vdev_control.tx_data[off])) = data;
#endif
	}

	return STD_E_OK;
}
static Std_ReturnType vdev_put_data16(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint16 data)
{
#ifdef VDEV_IO_UDP
	uint32 off = (addr - region->start);
	*((uint16*)(&region->data[off])) = data;
#else /* VDEV_IO_MMAP */
	uint32 off = addr - VDEV_TX_DATA_BASE;
	*((uint16*)(&vdev_control.tx_data[off])) = data;
#endif
	return STD_E_OK;
}
static Std_ReturnType vdev_put_data32(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint32 data)
{
#ifdef VDEV_IO_UDP
	uint32 off = (addr - region->start);
	*((uint32*)(&region->data[off])) = data;
#else /* VDEV_IO_MMAP */
	uint32 off = addr - VDEV_TX_DATA_BASE;
	*((uint32*)(&vdev_control.tx_data[off])) = data;
#endif
	return STD_E_OK;
}
static Std_ReturnType vdev_get_pointer(MpuAddressRegionType *region, CoreIdType core_id, uint32 addr, uint8 **data)
{
	uint32 off = (addr - region->start);
	*data = &region->data[off];
	return STD_E_OK;
}

