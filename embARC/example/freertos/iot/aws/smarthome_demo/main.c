/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * Modified by Qiang Gu(Qiang.Gu@synopsys.com) for embARC
 * \version 2016.01
 * \date 2016-01-26
 */

/**
 * \defgroup	EMBARC_APP_FREERTOS_IOT_AWS_SMARTHOME_DEMO	embARC IoT Amazon Web Services(AWS) Smart Home Demo
 * \ingroup	EMBARC_APPS_TOTAL
 * \ingroup	EMBARC_APPS_OS_FREERTOS
 * \ingroup	EMBARC_APPS_MID_LWIP
 * \ingroup	EMBARC_APPS_MID_FATFS
 * \ingroup	EMBARC_APPS_MID_MBEDTLS
 * \ingroup	EMBARC_APPS_MID_AWS
 * \brief	embARC IoT Amazon Web Services(AWS) Smart Home Demo
 *
 * \details
 * ### Extra Required Tools
 *
 * ### Extra Required Peripherals
 *     * [Digilent PMOD WIFI(MRF24WG0MA)](http://www.digilentinc.com/Products/Detail.cfm?NavPath=2,401,884&Prod=PMOD-WIFI)
 *     * [Digilent PMOD TMP2](https://www.digilentinc.com/Products/Detail.cfm?NavPath=2,401,961&Prod=PMOD-TMP2)
 *
 * ### Design Concept
 *     The Pmod modules should be connected to \ref EMBARC_BOARD_CONNECTION "EMSK".
 *     This example is designed to show how to connect EMSK and [AWS IoT](https://aws.amazon.com/iot/?nc1=h_ls) with using embARC.
 *
 * ### Usage Manual
 *     Before compiling this example, you need to change macro **AWS_IOT_MQTT_HOST** in aws_iot_config.h to your own aws iot cloud mqtt host.
 *     And create an aws iot thing named **SmartHome**, you can refer to [Gettting Started with AWS IoT](https://docs.aws.amazon.com/iot/latest/developerguide/iot-gs.html)
 *     for details, and generate certificates and download them and rename them to the ones under aws/smarthome_demo/cert.
 *
 *     Copy aws/smarthome_demo/cert to root folder of the EMSK SD card and run this example.
 *     It can report the following parameters to the AWS IoT Shadow.
 *     - the Room Temperature (float) - PMOD TMP2 connected to J2
 *     - the FrontDoor Lock (locked or unlocked) - Button X <-> LED 4-5
 *     - the LivingRoom Light (on or off) - Button L <-> Upper LED 2-3
 *     - the Kitchen Light (on or off) - Button R <-> Upper LED 0-1
 *
 *     EMSK can send the status of the Room Temperature, FrontDoor Lock status, LivingRoom Light status and Kitchen Light status to the AWS IoT and interact
 *     with AWS IoT with using embARC.
 *
 *     Open [dashboard website](http://foss-for-synopsys-dwc-arc-processors.github.io/freeboard/), and load dashboard-smarthome-singlething.json
 *     dashboard configuration in current folder, and then you can control and monitor the single node.
 *
 *     After load the configuration, you also need to click at the setting icon, and then click on the aws datasource, and then
 *     change the AWS IOT ENDPOINT, REGION, ACCESS KEY, SECRET KEY to your own aws ones.
 *     ![embARC AWS Smart Home Dashboard Setting - Step1](emsk_freertos_iot_aws_smarthome_webapp_setting1.jpg)
 *     ![embARC AWS Smart Home Dashboard Setting - Step2](emsk_freertos_iot_aws_smarthome_webapp_setting2.jpg)
 *
 *     The JSON Document in the AWS IoT cloud is shown below.
 *
 *     \code{.unparsed}
 *     {
 *       "reported": {
 *         "temperature": 0,
 *         "DoorLocked": false,
 *         "KitchenLights": false,
 *         "LivingRoomLights": false
 *       },
 *       "desired": {
 *         "DoorLocked": false,
 *         "KitchenLights": false,
 *         "LivingRoomLights": false
 *       }
 *     }
 *     \endcode
 *
 *     ![embARC AWS Smart Home Demo using EMSK](/doc/documents/pic/images/example/emsk/emsk_freertos_iot_aws_smarthome_demo.jpg)
 *     ![ScreenShot of embARC AWS Smart Home Demo using EMSK](/doc/documents/pic/images/example/emsk/emsk_freertos_iot_aws_smarthome_demo_uart.jpg)
 *     ![ScreenShot of embARC IoT Web App for Smart Home Demo](/doc/documents/pic/images/example/emsk/emsk_freertos_iot_aws_smarthome_demo_webapp.jpg)
 *
 * ### Extra Comments
 *
 */

/**
 * \file
 * \ingroup	EMBARC_APP_FREERTOS_IOT_AWS_SMARTHOME_DEMO
 * \brief	embARC IoT Amazon Web Services(AWS) smart home demo
 */

/**
 * \addtogroup	EMBARC_APP_FREERTOS_IOT_AWS_SMARTHOME_DEMO
 * @{
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <signal.h>
#include <sys/time.h>
#include <limits.h>

#include "os_hal_inc.h"

#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_shadow_interface.h"
#include "aws_iot_shadow_json_data.h"
#include "aws_iot_config.h"
#include "aws_iot_mqtt_interface.h"

#include "embARC.h"


#ifdef PATH_MAX
#undef PATH_MAX
#endif
#define PATH_MAX 256

#define ROOMTEMPERATURE_UPPERLIMIT 32.0
#define ROOMTEMPERATURE_LOWERLIMIT 25.0
#define STARTING_ROOMTEMPERATURE ROOMTEMPERATURE_LOWERLIMIT

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 512

//#define SIMULATE_TEMPERATURE

static char HostAddress[255] = AWS_IOT_MQTT_HOST;
static uint32_t port = AWS_IOT_MQTT_PORT;

static MQTTClient_t mqttClient;
static char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
static size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

static float temperature = STARTING_ROOMTEMPERATURE;
static bool DoorLocked = false;
static bool KitchenLights = false;
static bool LivingRoomLights = false;

static bool last_DoorLocked = false;
static bool last_KitchenLights = false;
static bool last_LivingRoomLights = false;

static bool temperature_updated = false;
static bool DoorLocked_updated = false;
static bool KitchenLights_updated = false;
static bool LivingRoomLights_updated = false;

static jsonStruct_t FrontDoorActuator;
static jsonStruct_t KitchenLightsActuator;
static jsonStruct_t LivingRoomLightsActuator;
static jsonStruct_t temperatureHandler;

static jsonStruct_t *curActuator[4];

#define CERT_ROOTDIR	"cert/smarthome"

static char rootCA[PATH_MAX + 1];
static char clientCRT[PATH_MAX + 1];
static char clientKey[PATH_MAX + 1];

static char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
static char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
static char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;

static void simulateRoomTemperature(float *pRoomTemperature) {
	static float deltaChange;

	if (*pRoomTemperature >= ROOMTEMPERATURE_UPPERLIMIT) {
		deltaChange = -0.5f;
	} else if (*pRoomTemperature <= ROOMTEMPERATURE_LOWERLIMIT) {
		deltaChange = 0.5f;
	}

	*pRoomTemperature += deltaChange;
}

static bool getRoomTemperature(float *pRoomTemperature)
{
#ifdef SIMULATE_TEMPERATURE
	simulateRoomTemperature(pRoomTemperature);
#else
	int32_t val = STARTING_ROOMTEMPERATURE;
	float cur_temp;
	temp_sensor_read(&val);
	cur_temp = (float)val / 10;
	if (cur_temp == (*pRoomTemperature)) {
		return false;
	} else {
		*pRoomTemperature = cur_temp;
		return true;
	}
#endif
}

void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
		const char *pReceivedJsonDocument, void *pContextData) {

	if (status == SHADOW_ACK_TIMEOUT) {
		INFO("Update Timeout--");
	} else if (status == SHADOW_ACK_REJECTED) {
		INFO("Update RejectedXX");
	} else if (status == SHADOW_ACK_ACCEPTED) {
		INFO("Update Accepted !!");
	}
}

#define FRONTDOOR_LEDMASK	0x30
static void controlFrontDoor(bool state)
{
	if (state) {
		INFO("FrontDoor is locked");
		led_write(FRONTDOOR_LEDMASK, FRONTDOOR_LEDMASK);
	} else {
		INFO("FrontDoor is open");
		led_write(0x0, FRONTDOOR_LEDMASK);
	}
}

#define KITCHEN_LEDMASK	0xC
static void controlKitchenLights(bool state)
{
	if (state) {
		INFO("Turn on KitchenLights");
		led_write(KITCHEN_LEDMASK, KITCHEN_LEDMASK);
	} else {
		INFO("Turn off KitchenLights");
		led_write(0x0, KITCHEN_LEDMASK);
	}
}

#define LIVINGROOM_LEDMASK	0x3
static void controlLivingRoomLights(bool state)
{
	if (state) {
		INFO("Turn on LivingRoomLights");
		led_write(LIVINGROOM_LEDMASK, LIVINGROOM_LEDMASK);
	} else {
		INFO("Turn off LivingRoomLights");
		led_write(0x0, LIVINGROOM_LEDMASK);
	}
}

void FrontDoorActuate_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	if (pContext != NULL) {
		bool temp = *(bool *)(pContext->pData);
		INFO("Delta - FrontDoor state changed to %d", temp);
		controlFrontDoor(DoorLocked);
	}
}

void KitchenLights_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	if (pContext != NULL) {
		bool temp = *(bool *)(pContext->pData);
		INFO("Delta - KitchenLights light state changed to %d", temp);
		controlKitchenLights(KitchenLights);
	}
}

void LivingRoomLights_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	if (pContext != NULL) {
		bool temp = *(bool *)(pContext->pData);
		INFO("Delta - LivingRoomLights light state changed to %d", temp);
		controlLivingRoomLights(LivingRoomLights);
	}
}

static void smarthome_init(void);
static void smarthome_close(void);
#define TIME_DELAY_UPDATE		(1)

#ifdef MID_NTSHELL
#define MAX_RETRY_TIMES			(20)
#else
#define MAX_RETRY_TIMES			(1000)
#endif

static void flash_leds(uint32_t delay_ms)
{
	for (int i = 0; i < 256; i ++) {
		led_write(i, 0xFF);
		vTaskDelay(delay_ms);
	}
}

static void smarthome_error(void)
{
#ifdef MID_NTSHELL
	led_write(0x1AA, BOARD_LED_MASK);
#else
	while (1) {
		flash_leds(100);
	}
#endif
}

static void smarthome_startup(void)
{
	INFO("++++Smarthome Startup++++");

#ifndef MID_NTSHELL
	flash_leds(15);
#else
	flash_leds(3);
#endif
	led_write(0x0, BOARD_LED_MASK);
}

#define NETWORK_LEDMASK		0x80
static void controlNetworkLed(bool state)
{
	if (state) {
		INFO("Network is ok");
		led_write(NETWORK_LEDMASK, NETWORK_LEDMASK);
	} else {
		INFO("Network is lost");
		led_write(0x0, NETWORK_LEDMASK);
	}
}

#define HEARTBEAT_LEDMASK	0x100
static bool heartbeat_state = true;
static void toggleHeartbeatLed(void)
{
	heartbeat_state = !heartbeat_state;
	if (heartbeat_state) {
		led_write(HEARTBEAT_LEDMASK, HEARTBEAT_LEDMASK);
	} else {
		led_write(0x0, HEARTBEAT_LEDMASK);
	}
}

static DEV_WNIC *p_wnic;
static void checkAndWaitNetwork(void)
{
	p_wnic = wnic_get_dev(BOARD_PMWIFI_0_ID);

	while (1) {
		if (p_wnic->wnic_info.conn_status == WNIC_CONNECTED) {
			controlNetworkLed(true);
			break;
		}
		controlNetworkLed(false);
		vTaskDelay(5000);
	}
}

int main(void)
{
	IoT_Error_t rc = NONE_ERROR;
	int32_t delay_ms = TIME_DELAY_UPDATE * 1000;
	int32_t retries = 0;

	smarthome_startup();

	temperature_updated = false;
	DoorLocked_updated = false;
	KitchenLights_updated = false;
	LivingRoomLights_updated = false;

	aws_iot_mqtt_init(&mqttClient);

	FrontDoorActuator.cb = FrontDoorActuate_Callback;
	FrontDoorActuator.pData = &DoorLocked;
	FrontDoorActuator.pKey = "DoorLocked";
	FrontDoorActuator.type = SHADOW_JSON_BOOL;

	KitchenLightsActuator.cb = KitchenLights_Callback;
	KitchenLightsActuator.pData = &KitchenLights;
	KitchenLightsActuator.pKey = "KitchenLights";
	KitchenLightsActuator.type = SHADOW_JSON_BOOL;

	LivingRoomLightsActuator.cb = LivingRoomLights_Callback;
	LivingRoomLightsActuator.pData = &LivingRoomLights;
	LivingRoomLightsActuator.pKey = "LivingRoomLights";
	LivingRoomLightsActuator.type = SHADOW_JSON_BOOL;

	temperatureHandler.cb = NULL;
	temperatureHandler.pKey = "temperature";
	temperatureHandler.pData = &temperature;
	temperatureHandler.type = SHADOW_JSON_FLOAT;

	INFO("\nAWS IoT SDK Version(dev) %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	sprintf(rootCA, "%s/%s", CERT_ROOTDIR, cafileName);
	sprintf(clientCRT, "%s/%s", CERT_ROOTDIR, clientCRTName);
	sprintf(clientKey, "%s/%s", CERT_ROOTDIR, clientKeyName);

	INFO("Using rootCA %s", rootCA);
	INFO("Using clientCRT %s", clientCRT);
	INFO("Using clientKey %s", clientKey);

	ShadowParameters_t sp = ShadowParametersDefault;

	while (retries++ < MAX_RETRY_TIMES) {
		sp.pMyThingName = AWS_IOT_MY_THING_NAME;
		sp.pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;
		sp.pHost = HostAddress;
		sp.port = port;
		sp.pClientCRT = clientCRT;
		sp.pClientKey = clientKey;
		sp.pRootCA = rootCA;

		checkAndWaitNetwork();

		INFO("Shadow Init");
		rc = aws_iot_shadow_init(&mqttClient);

		INFO("Shadow Connect");
		rc = aws_iot_shadow_connect(&mqttClient, &sp);

		if (NONE_ERROR != rc) {
			ERROR("Shadow Connection Error %d", rc);
		}

		/** Register delta actuators */
		rc = aws_iot_shadow_register_delta(&mqttClient, &FrontDoorActuator);
		rc = aws_iot_shadow_register_delta(&mqttClient, &KitchenLightsActuator);
		rc = aws_iot_shadow_register_delta(&mqttClient, &LivingRoomLightsActuator);

		if (NONE_ERROR != rc) {
			ERROR("Shadow Register Delta Error");
		}

	#ifndef SIMULATE_TEMPERATURE
		getRoomTemperature(&temperature);
	#endif
		smarthome_init();

		controlFrontDoor(DoorLocked);
		controlKitchenLights(KitchenLights);
		controlLivingRoomLights(LivingRoomLights);

		last_LivingRoomLights = !LivingRoomLights;
		last_KitchenLights = !KitchenLights;
		last_DoorLocked = !DoorLocked;

		uint32_t act_idx = 0;
		// loop and publish a change in temperature
		while (NONE_ERROR == rc) {
			rc = aws_iot_shadow_yield(&mqttClient, 200);
			if (rc != NONE_ERROR) {
				continue;
			}

			/** Update desired messages */
			act_idx = 0;
			if (DoorLocked_updated) {
				curActuator[act_idx] = &FrontDoorActuator;
				act_idx ++;
				DoorLocked = !DoorLocked;
				DoorLocked_updated = false;
				controlFrontDoor(DoorLocked);
			}
			if (KitchenLights_updated) {
				curActuator[act_idx] = &KitchenLightsActuator;
				act_idx ++;
				KitchenLights = !KitchenLights;
				KitchenLights_updated = false;
				controlKitchenLights(KitchenLights);
			}
			if (LivingRoomLights_updated) {
				curActuator[act_idx] = &LivingRoomLightsActuator;
				act_idx ++;
				LivingRoomLights = !LivingRoomLights;
				LivingRoomLights_updated = false;
				controlLivingRoomLights(LivingRoomLights);
			}
			if (act_idx > 0) {
				rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
				if (rc == NONE_ERROR) {
					if (act_idx == 1) {
						rc = aws_iot_shadow_add_desired(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 1, curActuator[0]);
					} else if (act_idx == 2) {
						rc = aws_iot_shadow_add_desired(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 2, curActuator[0], curActuator[1]);
					} else if (act_idx == 3) {
						rc = aws_iot_shadow_add_desired(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 3, curActuator[0], curActuator[1], curActuator[2]);
					} else {
						rc = aws_iot_shadow_add_desired(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 3, &FrontDoorActuator, &KitchenLightsActuator, &LivingRoomLightsActuator);
					}

					if (rc == NONE_ERROR) {
						rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
						if (rc == NONE_ERROR) {
							INFO("Update Shadow Desired: %s", JsonDocumentBuffer);
							rc = aws_iot_shadow_update(&mqttClient, AWS_IOT_MY_THING_NAME, JsonDocumentBuffer, ShadowUpdateStatusCallback,
							NULL, 4, true);
						}
					}
				}
			}

			if (rc != NONE_ERROR) {
				continue;
			}

			/** Update reported messages */
			temperature_updated = getRoomTemperature(&temperature);

			rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
			if (rc == NONE_ERROR) {
				rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 4, &temperatureHandler, &FrontDoorActuator, &KitchenLightsActuator, &LivingRoomLightsActuator);
				if (rc == NONE_ERROR) {
					rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
					if (rc == NONE_ERROR) {
						INFO("Update Shadow: %s", JsonDocumentBuffer);
						rc = aws_iot_shadow_update(&mqttClient, AWS_IOT_MY_THING_NAME, JsonDocumentBuffer, ShadowUpdateStatusCallback,
						NULL, 4, true);
					}
				}
			}

			toggleHeartbeatLed();

			INFO("*****************************************************************************************\n");
			vTaskDelay(delay_ms);
		}

		smarthome_close();
		if (NONE_ERROR != rc) {
			ERROR("An error occurred in the loop %d", rc);
		}

		checkAndWaitNetwork();

		if (rc == YIELD_ERROR) {
			INFO("Disconnecting");
			rc = aws_iot_shadow_disconnect(&mqttClient);

			if (NONE_ERROR != rc) {
				ERROR("Disconnect error %d", rc);
			}
		}
	}
	smarthome_error();

	return rc;
}


/* interrupt service routine for KitchenLights light button */
static void KitchenLights_button_isr(void *ptr)
{
	KitchenLights_updated = true;
}

/* interrupt service routine for LivingRoomLights light button */
static void LivingRoomLights_button_isr(void *ptr)
{
	LivingRoomLights_updated = true;
}

/* interrupt service routine for FrontDoor control */
static void FrontDoor_button_isr(void *ptr)
{
	DoorLocked_updated = true;
}

#define BUTTON_USED_MASK	(0x7)

static void smarthome_init(void)
{
	DEV_GPIO_BIT_ISR bit_isr;
	DEV_GPIO_INT_CFG int_cfg;

	temp_sensor_init(TEMP_I2C_SLAVE_ADDRESS);

	DEV_GPIO_PTR port = gpio_get_dev(EMSK_BUTTON_PORT);

	port->gpio_control(GPIO_CMD_DIS_BIT_INT, (void *)BUTTON_USED_MASK);

	int_cfg.int_bit_mask = BUTTON_USED_MASK;
	int_cfg.int_bit_type = BUTTON_USED_MASK; /* edge trigger */
	int_cfg.int_bit_polarity = 0x0; /* falling sensitive */
	int_cfg.int_bit_debounce = BUTTON_USED_MASK; /* with debounce */

	port->gpio_control(GPIO_CMD_SET_BIT_INT_CFG, &int_cfg);

	/* KitchenLights button */
	bit_isr.int_bit_ofs = 0;
	bit_isr.int_bit_handler = KitchenLights_button_isr;
	port->gpio_control(GPIO_CMD_SET_BIT_ISR, &bit_isr);

	/* LivingRoomLights button */
	bit_isr.int_bit_ofs = 1;
	bit_isr.int_bit_handler = LivingRoomLights_button_isr;
	port->gpio_control(GPIO_CMD_SET_BIT_ISR, &bit_isr);

	/* FrontDoor button */
	bit_isr.int_bit_ofs = 2;
	bit_isr.int_bit_handler = FrontDoor_button_isr;
	port->gpio_control(GPIO_CMD_SET_BIT_ISR, &bit_isr);

	port->gpio_control(GPIO_CMD_ENA_BIT_INT, (void *)BUTTON_USED_MASK);

}

static void smarthome_close(void)
{
	DEV_GPIO_PTR port = gpio_get_dev(EMSK_BUTTON_PORT);
	port->gpio_control(GPIO_CMD_DIS_BIT_INT, (void *)BUTTON_USED_MASK);
}
/** @} */