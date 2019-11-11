#include "WiFi.h"
#include "AzureIotHub.h"
#include "Esp32MQTTClient.h"
#include <Arduino.h>
#include "parson.h"
#include "servo_esp32.h"//versao modificada da servo.h

#define INTERVAL 10000 //intervalo entre envios de dados 
#define DEVICE_ID "ESP32_PI_IV"
#define MESSAGE_MAX_LEN 256
// pinos LDRs (ver gpios)
#define ldrrd  36 // direita baixo
#define ldrld  39 //esquerda baixo
#define ldrlt  34 //esquerda topo
#define ldrrt  35 //direita topo

const char* ssid = "Fe";
const char* password = "q1w2e3r4t5";

/*

Pinos GPIOS utilizados
36 ldr esquerda baixo
39 ldr direita baixo
34 ldr esquerda topo
35 ldr direita topo

33 servo horizontal
32 servo vertical

35 resistor potencia

*/

Control c;

typedef struct EVENT_MESSAGE_INSTANCE_TAG
{
  IOTHUB_MESSAGE_HANDLE messageHandle;
  size_t messageTrackingId; // For tracking the messages within the user callback.
} EVENT_MESSAGE_INSTANCE_TAG;

const char *messagedata= "{\"Tensao\":%.2f, \"Potencia\":%.2f}";
static char propText[1024];
static char msgText[1024];
static int trackingId = 0;
long lastConnectionTime;
static bool hasIoTHub = false;
static bool hasWifi = false;
static uint64_t send_interval_ms;
static uint64_t check_interval_ms;
static bool needs_reconnect = false;
int servoh = 90; // servo horizontal
int servov = 90; // servo vertical
static const char *connectionString = "HostName=Pi-teste.azure-devices.net;DeviceId=ESP32_PI_IV;SharedAccessKey=HEv+wHj4d6VeHDZ0jWDO7GoqyJkdK1GXBJr+YILiqD4=";

//declaração das funções
static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char *payLoad, size_t size, void *userContextCallback);
static int deviceMethodCallback(const char *method_name, const unsigned char *payload, size_t size, unsigned char **response, size_t *response_size, void *userContextCallback);
static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *user_context);
static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback);
static void reportedStateCallback(int status_code, void *userContextCallback);

Servo horizontal;
Servo vertical;

IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle = NULL;

static bool initIotHubClient(void)
{
  Serial.println("initIotHubClient Start!");
  if (platform_init() != 0)
  {
    Serial.println("Failed to initialize the platform.");
    return false;
  }

  if ((iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol)) == NULL)
  {
    Serial.println("ERROR: iotHubClientHandle is NULL!");
    return false;
  }

  IoTHubClient_LL_SetRetryPolicy(iotHubClientHandle, IOTHUB_CLIENT_RETRY_EXPONENTIAL_BACKOFF, 1200);
  bool traceOn = true;
  IoTHubClient_LL_SetOption(iotHubClientHandle, "logtrace", &traceOn);

  // Setting twin call back for desired properties receiving.
  if (IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, deviceTwinCallback, NULL) != IOTHUB_CLIENT_OK)
  {
    Serial.println("IoTHubClient_LL_SetDeviceTwinCallback..........FAILED!");
    return false;
  }

  // Setting direct method callback for direct method calls receiving
  if (IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, NULL) != IOTHUB_CLIENT_OK)
  {
    Serial.println("IoTHubClient_LL_SetDeviceMethodCallback..........FAILED!");
    return false;
  }

  // Connection status change callback
  if (IoTHubClient_LL_SetConnectionStatusCallback(iotHubClientHandle, connectionStatusCallback, NULL) != IOTHUB_CLIENT_OK)
  {
    Serial.println("IoTHubClient_LL_SetDeviceMethodCallback..........FAILED!");
    return false;
  }
  Serial.println("initIotHubClient End!");

  // toggle azure led to default off
  return true;
}

static void closeIotHubClient()
{
  if (iotHubClientHandle != NULL)
  {
    IoTHubClient_LL_Destroy(iotHubClientHandle);
    platform_deinit();
    iotHubClientHandle = NULL;
  }
  Serial.println("closeIotHubClient!");
}

static void sendTelemetry(const char *payload)
{
  if (needs_reconnect)
  {
    closeIotHubClient();
    initIotHubClient();
    needs_reconnect = false;
  }

  EVENT_MESSAGE_INSTANCE_TAG *thisMessage = (EVENT_MESSAGE_INSTANCE_TAG *)malloc(sizeof(EVENT_MESSAGE_INSTANCE_TAG));
  thisMessage->messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char *)payload, strlen(payload));

  if (thisMessage->messageHandle == NULL)
  {
    Serial.println("ERROR: iotHubMessageHandle is NULL!");
    free(thisMessage);
    return;
  }

  thisMessage->messageTrackingId = trackingId++;

  MAP_HANDLE propMap = IoTHubMessage_Properties(thisMessage->messageHandle);

  (void)sprintf_s(propText, sizeof(propText), "PropMsg_%zu", trackingId);
  if (Map_AddOrUpdate(propMap, "PropName", propText) != MAP_OK)
  {
    Serial.println("ERROR: Map_AddOrUpdate Failed!");
  }

  // send message to the Azure Iot hub
  if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle,
                                     thisMessage->messageHandle, sendConfirmationCallback, thisMessage) != IOTHUB_CLIENT_OK)
  {
    Serial.println("ERROR: IoTHubClient_LL_SendEventAsync..........FAILED!");
    return;
  }

  /* Turn on Azure LED */
  IoTHubClient_LL_DoWork(iotHubClientHandle);
  Serial.println("IoTHubClient sendTelemetry completed!");
}

static void sendReportedProperty(const char *payload)
{
  if (needs_reconnect)
  {
    closeIotHubClient();
    initIotHubClient();
    needs_reconnect = false;
  }
  if (IoTHubClient_LL_SendReportedState(iotHubClientHandle, (const unsigned char *)payload,
                                        strlen((const char *)payload), reportedStateCallback, NULL) != IOTHUB_CLIENT_OK)
  {
    Serial.println("ERROR: IoTHubClient sendReportedProperty..........FAILED!");
    return;
  }

  Serial.println("IoTHubClient sendReportedProperty completed!");
}

static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char *payLoad, size_t size, void *userContextCallback)
{
  Serial.print("Device Twin Callback method with : ");
  Serial.println((const char *)payLoad);

  JSON_Value *root_value = json_parse_string((const char *)payLoad);
  JSON_Object *root_object = json_value_get_object(root_value);

  if (update_state == DEVICE_TWIN_UPDATE_PARTIAL)
  {
    Serial.print("DEVICE_TWIN_UPDATE_STATE is :");
    Serial.println(update_state);

    int version = (uint8_t)json_object_dotget_number(root_object, "$version");
    for (int i = 0, count = json_object_get_count(root_object); i < count; i++)
    {
      const char *partialName = json_object_get_name(root_object, i);
      if (partialName != NULL && partialName[0] != '$')
      {
        JSON_Object *partialObject = json_object_dotget_object(root_object, partialName);
        int partialValue = (uint8_t)json_object_dotget_number(partialObject, "value");

        (void)sprintf_s(propText, sizeof(propText),
                        "{\"%s\":{\"value\":%d,\"status\":\"completed\",\"desiredVersion\":%d}}",
                        partialName, partialValue, version);

        sendReportedProperty(propText);
      }
    }
  }
  else
  {
    JSON_Object *desired, *reported;

    desired = json_object_dotget_object(root_object, "desired");
    reported = json_object_dotget_object(root_object, "reported");

    int version = (uint8_t)json_object_dotget_number(desired, "$version");
    for (int i = 0, count = json_object_get_count(desired); i < count; i++)
    {
      const char *itemName = json_object_get_name(desired, i);
      if (itemName != NULL && itemName[0] != '$')
      {
        int desiredVersion = 0, value = 0;

        JSON_Object *itemObject = json_object_dotget_object(desired, itemName);
        value = (uint8_t)json_object_dotget_number(itemObject, "value");
        
        JSON_Object *keyObject = json_object_dotget_object(reported, itemName);
        if (keyObject != NULL)
        {
          desiredVersion = (uint8_t)json_object_dotget_number(keyObject, "desiredVersion");
        }

        if (keyObject != NULL && (version == desiredVersion))
        {
          Serial.print("key: ");
          Serial.print(itemName);
          Serial.println(" found in reported and versions match.");
        }
        else
        {
          Serial.print("key: ");
          Serial.print(itemName);
          Serial.println(" either not found in reported or versions do not match.");

          (void)sprintf_s(propText, sizeof(propText),
                          "{\"%s\":{\"value\":%d,\"status\":\"completed\",\"desiredVersion\":%d}}",
                          itemName, value, version);
          sendReportedProperty(propText);
        }
      }
    }
  }
  json_value_free(root_value);
}

static int deviceMethodCallback(const char *method_name, const unsigned char *payload, size_t size, unsigned char **response, size_t *response_size, void *userContextCallback)
{
  (void)userContextCallback;
  (void)payload;
  (void)size;

  int result = 200;

  Serial.print("Executed direct method: ");
  Serial.println(method_name);

  Serial.print("Executed direct method payload: ");
  Serial.println((const char *)payload);

}
static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *user_context)
{
  (void)reason;
  (void)user_context;

  Serial.print("iotHubClient connectionStatusCallback result: ");
  Serial.println(result);

  if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
  {
    Serial.println("Network connection.");
  }
  else
  {
    Serial.println("No network connection.");
    Serial.println("Needs reconnect to the IoT Hub.");
    needs_reconnect = true;
  }
}

static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
  EVENT_MESSAGE_INSTANCE_TAG *eventInstance = (EVENT_MESSAGE_INSTANCE_TAG *)userContextCallback;
  size_t id = eventInstance->messageTrackingId;

  Serial.print("Confirmation received for message tracking id = ");
  Serial.print(id);
  Serial.print(" with result = ");
  Serial.println(ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));

  IoTHubMessage_Destroy(eventInstance->messageHandle);
  free(eventInstance);
  
}

static void reportedStateCallback(int status_code, void *userContextCallback)
{
  (void)userContextCallback;
  Serial.print("Device Twin reported properties update completed with result: ");
  Serial.println(status_code);
}

void setup()
{
  Serial.begin(115200);
  lastConnectionTime = 0;
  Serial.println("PI - IV");
  Serial.println("Conectando-se à rede WiFi...");
  Serial.println();
  WiFi.begin(ssid,password);
  
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    hasWifi = false;
    Serial.print(WiFi.status());
  }
  
  hasWifi = true;
  Serial.println("");
  Serial.println("WiFi connectado com sucesso!");
  Serial.println("IP obtido: ");
  Serial.println(WiFi.localIP());
  Serial.println(" > IoT Hub");
  
  if (!initIotHubClient())
  {
    hasIoTHub = false;
    Serial.println("Initializing IoT hub failed.");
    return;
  }
  hasIoTHub = true;
  Serial.println("Start sending events.");
  send_interval_ms = millis();
  check_interval_ms = millis();
   
  //horizontal.attach(32);//gpio 32
  //vertical.attach(33);//gpio 33
  //horizontal.write(servoh);
  //vertical.write(servov);
  c.init();
}

void loop()
{
/*
  int lt = analogRead(ldrlt); // topo esquerda
  int rt = analogRead(ldrrt); // top direita
  int ld = analogRead(ldrld); // baixo esquerda
  int rd = analogRead(ldrrd); // baixo direita
  Serial.print("pino lt: ");
  Serial.println(lt);
  Serial.println(rt);
  Serial.println(ld);
  Serial.println(rd);

  //int dtime = analogRead(32) / 20; // potenciometros gpio 32
  //int tol = analogRead(33) / 4; //gpio 33

  int tol = 50;

  int avt = (lt + rt) / 2; // media valor topo
  int avd = (ld + rd) / 2; // media valor baixo

  int avl = (lt + ld) / 2; // media valor esquerda
  int avr = (rt + rd) / 2; // media valor direita
  
  int dvert = abs(avt - avd); //diferenca vertical - topo e  baixo

  int dhoriz = abs(avl - avr); // iferenca horizontal - esquerda e direita

  if ((avl > avr) && (dhoriz > tol)){ // horizontal  - esquerda e direita < 180
    if(servoh < 180){
      servoh++;
      horizontal.write(servoh);
    }
    
  }

   if((avr > avl) && (dhoriz > tol)){ // horizontal  - esquerda e direita > 0
    if (servoh > 0) 
      {
      servoh--;
      horizontal.write(servoh);
      }
  }
  delay(1000);
  
  if ((avt > avd) && (dvert > tol)){ // vertical  - topo e baixo < 180
    if(servov < 180){
      servov++;
      vertical.write(servov);
    }
  }

  if((avd > avr) && (dvert > tol)){ // vertical  - topo e baixo > 0
    if (servov > 0){ 
      servov--;
      vertical.write(servov);
    }
  }

  delay(1000);  
*/
  c.move();
  delay(100);

  if (hasWifi && hasIoTHub)//enviar para o azure
  {
    int sensorValue = analogRead(23);
    double tensao = sensorValue * (3.3 / 1023);
    double potencia = tensao * 10000;
 
    if ((int)(millis() - send_interval_ms) >= INTERVAL)
    {
      sprintf_s(msgText, sizeof(msgText), messagedata,tensao,potencia);
       
      sendTelemetry(msgText);

      Serial.println(msgText);

      send_interval_ms = millis();
    }
  }
}