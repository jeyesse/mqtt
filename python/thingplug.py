#!/usr/bin/env python
# _*_ coding: utf-8 _*_
import paho.mqtt.client as mqtt
import sys
import time
from datetime import datetime
from enum import IntEnum

MQTTCLIENT_SUCCESS 	= 0
MQTTCLIENT_FAILURE	= -1 

APP_EUI			= "thingplug"
QOS			= 0
TIMEOUT			= 10000
BUF_SIZE		= 128

# The pathes will be created dynamically
mqttPubTopic		= ""
mqttSubTopic		= ""
mqttPubPath		= ""
mqttRemoteCSE		= ""
mqttContainer		= ""
mqttSubscription		= ""
mqttDictCallback		= {}

frameMqttPubTopic	= "/oneM2M/req_msg/{0}/{1}"	# appEUI, deviceId
frameMqttSubTopic	= "/oneM2M/resp/{0}/{1}"	# deviceId, appEUI
frameMqttPubPath	= "/oneM2M/req/{0}/{1}"		# deviceId, appEUI
frameMqttRemoteCSE	= "/{0}/v1_0/remoteCSE-{1}"	# appEUI, deviceId
frameMqttContainer 	= "{0}/container-{1}"		# remoteCSE, container
frameMqttSubscription	= "{0}/subscription-{1}"	#container, notifyName

bufRequest		= ""
strNL			= ""
strExt			= ""
strDkey			= ""
dataName 		= ""
dataValue 		= ""

address 	 	= ""
userName	 	= ""
passWord	 	= ""
deviceId	 	= ""
passCode	 	= ""
container	 	= ""

# MQTT Process Steps
class MqttStep(IntEnum):
	START					= 0
	CONNECT					= 1
	CONNECT_REQUESTED			= 2
	CREATE_NODE				= 3
	CREATE_NODE_REQUESTED			= 4
	CREATE_REMOTE_CSE			= 5
	CREATE_REMOTE_CSE_REQUESTED		= 6
	CREATE_CONTAINER			= 7
	CREATE_CONTAINER_REQUESTED		= 8
	CREATE_MGMT_CMD				= 9
	CREATE_MGMT_CMD_REQUESTED		= 10
	SUBSCRIBE				= 11
	SUBSCRIBE_REQUESTED			= 12
	CREATE_CONTENT_INSTANCE			= 13
	CREATE_CONTENT_INSTANCE_REQUESTED	= 14
	DELETE_SUBSCRIBE		= 15
	DELETE_SUBSCRIBE_REQUESTED		= 16
	FINISH					= 17
step = IntEnum('MqttStep', 'START')

# step 1 - params: appEUI, deviceId, ri, deviceId, deviceId, deviceId
frameCreateNode =\
"<m2m:req>\
<op>1</op>\
<ty>14</ty>\
<to>/{0}/v1_0</to>\
<fr>{1}</fr>\
<ri>{2}</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<nm>{3}</nm>\
<pc>\
<nod>\
<ni>{4}</ni>\
<mga>MQTT|{5}</mga>\
</nod>\
</pc>\
</m2m:req>"

# step 2 – params: AppEUI, deviceId, ri, passCode, deviceId, deviceId, nl
frameCreateRemoteCSE = \
"<m2m:req>\
<op>1</op>\
<ty>16</ty>\
<to>/{0}/v1_0</to>\
<fr>{1}</fr>\
<ri>{2}</ri>\
<passCode>{3}</passCode>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<nm>{4}</nm>\
<pc>\
<csr>\
<cst>3</cst>\
<csi>{5}</csi>\
<rr>true</rr>\
<nl>{6}</nl>\
</csr>\
</pc>\
</m2m:req>"

# step 3 – params: cse, deviceId, ri, container, dKey
frameCreateContainer = \
"<m2m:req>\
<op>1</op>\
<ty>3</ty>\
<to>{0}</to>\
<fr>{1}</fr>\
<ri>{2}</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<nm>{3}</nm>\
<dKey>{4}</dKey>\
<pc>\
<cnt>\
<lbl>con</lbl>\
</cnt>\
</pc>\
</m2m:req>"

# step 4 – params: appEUI, deviceId, ri, deviceId, dkey, strExt
frameCreateMgmtCmd = \
"<m2m:req>\
<op>1</op>\
<ty>12</ty>\
<to>/{0}/v1_0</to>\
<fr>{1}</fr>\
<ri>{2}</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<nm>{3}_turnOn</nm>\
<dKey>{4}</dKey>\
<pc>\
<mgc>\
<cmt>turnOn</cmt>\
<exe>false</exe>\
<ext>{5}</ext>\
</mgc>\
</pc>\
</m2m:req>"

# step 4-1 - params: mqttContainer, deviceId, strRi, notificationName, uKey, deviceId
frameSubscribe =\
"<m2m:req>\
<op>1</op>\
<to>{0}</to>\
<fr>{1}</fr>\
<ty>23</ty>\
<ri>{2}</ri>\
<nm>{3}</nm>\
<uKey>{4}</uKey>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<pc>\
<sub>\
<enc>\
<rss>1</rss>\
</enc>\
<nu>MQTT|{5}</nu>\
<nct>2</nct>\
</sub>\
</pc>\
</m2m:req>"

# step 5 - params: container, deviceId, ri, dkey, name, value
frameCreateContentInstance = \
"<m2m:req>\
<op>1</op>\
<ty>4</ty>\
<to>{0}</to>\
<fr>{1}</fr>\
<ri>{2}</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<dKey>{3}</dKey>\
<pc>\
<cin>\
<cnf>{4}</cnf>\
<con>{5}</con>\
</cin>\
</pc>\
</m2m:req>"

#step 6 - params: container, deviceId, ri, dKey, deviceId
frameDeleteSubscribe = \
"<m2m:req>\
<op>4</op>\
<to>{0}</to>\
<fr>{1}</fr>\
<ri>{2}</ri>\
<uKey>{3}</uKey>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<pc>\
<sub>\
<enc>\
<rss>1</rss>\
</enc>\
<nu>MQTT|{4}</nu>\
<nct>2</nct>\
</sub>\
</pc>\
</m2m:req>"

def mqttConnect(client, address, devId):
	global deviceId
	global mqttPubTopic
	global mqttSubTopic
	global mqttPubPath 
	global mqttRemoteCSE

	deviceId = devId

	mqttPubTopic	= frameMqttPubTopic.format(APP_EUI, deviceId)
	mqttSubTopic	= frameMqttSubTopic.format(deviceId, APP_EUI)
	mqttPubPath 	= frameMqttPubPath.format(deviceId, APP_EUI)
	mqttRemoteCSE	= frameMqttRemoteCSE.format(APP_EUI, deviceId)

	# set callback functions
	client.on_connect	= on_connect
	client.on_publish	= on_publish
	client.on_subscribe	= on_subscribe
	client.on_message	= on_message

	print("Attempting MQTT connection...:addr={0}\n".format(address))
	rc = client.connect(address, port=1883, keepalive=60)
	if rc == 0:
		print("Mqtt connecting...\n")
	else :
		print("Failed to connect request, return code %d\n" %int(rc[0]))
		return False

	global step
	step = MqttStep.CONNECT

	client.loop_start()
	while step<=MqttStep.CONNECT:
		print("Waiting Connection...\n")
		time.sleep(1)
	client.loop_stop()

	# registration of the topics
	client.subscribe(mqttPubTopic)
	client.subscribe(mqttSubTopic)

	return True 

def printRC(rc):
	if rc==0:
		print("Connection successful\n") 
	elif rc==1:
		printf("Connection refused - incorrect protocol version\n") 
	elif rc==2: 
		print("Connection refused - invalid client identifier\n") 
	elif rc==3: 
		print("Connection refused - server unavailable\n")
	elif rc==4: 
		print("Connection refused - bad username or password\n")
	elif rc==5: 
		print("Connection refused - not authorised\n")
	else: 
		print("Currently unused.\n")

def mqttDisconnect(client):
	client.disconnect()
	client.loop_stop()

def on_connect(client, userdata, flags, rc):
	print("on_connect called:\n"+str(flags)+", Result code="+str(rc))
	printRC(rc)
	global step
	step = MqttStep.CONNECT_REQUESTED
	client.on_connect = None
	
def on_publish(client, userdata, result):
	print("data published\n")

def on_subscribe(client, userdata, mid, granted_qos):
	print("on_subscribe: mid="+str(mid)+", grantedQos="+str(granted_qos)+"\n")

def on_message(client, userdata, message):
	print("on_message called\n")
	payload = message.payload.decode("utf-8")
	length = len(payload)
	
#	if length>0: payload[length]=u'\0'
	print("Payload({0}): {1}\n".format(length, payload))

	# error check
	strRsc  = [""]
	rc= parseValue(strRsc, payload, length, "rsc")
	print("parse result(rsc): "+strRsc[0])	

	if rc== MQTTCLIENT_SUCCESS : 
		printResultCode(strRsc[0])
		resultCode = int(strRsc[0])
		if resultCode == 4004: return
		if resultCode == 4000: return

		strRi = generateRi(deviceId)

		global step
		if step == MqttStep.CREATE_NODE_REQUESTED:
			# parse response message
			indexPC = payload.find("pc")
			buf = [""]
			rc = parseValue(buf, payload[indexPC:], len, "ri")
			global strNL
			strNL = buf[0]
			if rc==MQTTCLIENT_SUCCESS:
				print("ri:%s\n" %str(strNL))
				global strExt
				strExt = strNL
				step = MqttStep.CREATE_REMOTE_CSE

		elif step == MqttStep.CREATE_REMOTE_CSE_REQUESTED : 
			buf = [""]
			rc = parseValue(buf, payload, length, "dKey")
			global strDkey
			strDkey = buf[0]
			print("dKey=%s\n" %str(strDkey))
			if rc==MQTTCLIENT_SUCCESS:
				step = MqttStep.CREATE_CONTAINER

		elif step == MqttStep.CREATE_CONTAINER_REQUESTED:
			step = MqttStep.CREATE_MGMT_CMD

		elif step == MqttStep.CREATE_MGMT_CMD_REQUESTED:
			step = MqttStep.SUBSCRIBE

		elif step == MqttStep.SUBSCRIBE_REQUESTED:
			step = MqttStep.CREATE_CONTENT_INSTANCE

		else:
			step = MqttStep.FINISH
	else:
		# Notification from ThingPlug Server
		buf = [""]
		rc = parseValue(buf, payload, length, "con")
		global strCon
		strCon = buf[0]
		
		strRoute = [""]
		parseValue(strRoute, payload, length, "sr")
		strSr = strRoute[0]
		notifyName = strSr.split('subscription-')
		
		global mqttDictCallback
		if strCon is not None and mqttDictCallback is not None: 
			try:
				callback = mqttDictCallback[notifyName[1]]
				print(notifyName[1])
				callback(strCon)
			except KeyError:
				pass

	return 1	# Do Not Need to be recalled.

# generates a unique resource ID
def generateRi(deviceId):
	return "{0}_{1}".format(deviceId, datetime.now().microsecond)

def parseValue(buf, payload, length, param):
	if payload is None:
		print("parseValue error: Payload is NULL\n")
		return None

	result = MQTTCLIENT_FAILURE 
	lenParam = len(param)

	tagBegin= "<{0}>".format(param)
	tagEnd	= "</{0}>".format(param)

	indexBegin = payload.find(tagBegin)
	if indexBegin is None: return result

	if indexBegin>0:
		indexEnd = payload.find(tagEnd)
		indexValue = indexBegin+lenParam+2
		lenValue = indexEnd-indexValue
		print("indexValue={0}, lenValue={1}\n".format(indexValue, lenValue))
		buf[0]= payload[indexValue : indexValue+lenValue]
		#buf+='\0'
		result = MQTTCLIENT_SUCCESS

	print("buf="+buf[0])

	return result

def printResultCode(buf) :
	if buf is None or len(buf)==0 : return
	print("[result code]: ")

	resultCode = int(buf)
	if resultCode == 2000: print("OK\n")
	elif resultCode == 2001: print("CREATED\n")
	elif resultCode == 2002: print("DELETED\n")
	elif resultCode == 2004: print("UPDATED\n")
	elif resultCode == 2100: print("CONTENT_EMPTY\n")
	elif resultCode == 4105: print("EXIST\n")
	elif resultCode == 4004: print("NOT FOUND\n")
	else: print("UNKNOWN ERROR\n")

def mqttCreateNode(client, dId, pCode):
	global deviceId
	global passCode
	global step

	deviceId = dId
	passCode = pCode

	ri = generateRi(deviceId)

	bufRequest = frameCreateNode.format(APP_EUI, deviceId, ri, deviceId, deviceId, deviceId)
	print("1. Create Node :\n payload=%s\n" %str(bufRequest) )
	print(" mqttPubPath=%s\n" %str(mqttPubPath) )

	# publish bufRequest
	rc = client.publish(mqttPubPath, bufRequest, QOS, False)
	global step
	step = MqttStep.CREATE_NODE_REQUESTED

	if rc[0]!=MQTTCLIENT_SUCCESS:
		print("Create Node Failed\n\n") 
		return False
	client.loop_start()
	while step<=MqttStep.CREATE_NODE_REQUESTED:
		print("Waiting Response from broker...\n")
		time.sleep(1)
	client.loop_stop()

	print("Create Node Success\n\n")
	return True

def mqttCreateRemoteCSE(client):
	global APP_EUI
	global deviceId
	global passCode
	global strNL
	global mqttPubPath
	global step

	strRi = generateRi(deviceId)

	bufRequest= frameCreateRemoteCSE.format(APP_EUI,
		deviceId, strRi, passCode, deviceId, deviceId, strNL)

	print("2. Create RemoteCSE :\n payload=%s\n" %str(bufRequest))
	print(" pub topic:{0}\n".format(mqttPubTopic))

	client.loop_start()
	rc = client.publish(mqttPubPath, bufRequest, QOS, False)
	step = MqttStep.CREATE_REMOTE_CSE_REQUESTED
	if rc[0]!=MQTTCLIENT_SUCCESS:
		print("Publish Failed\n\n") 
		return False

	while step<=MqttStep.CREATE_REMOTE_CSE_REQUESTED:
		print("Waiting message on create remote CSE\n")
		time.sleep(1)
	client.loop_stop()

	print("Publish Success\n\n")
	return True


def mqttCreateContainer(client, deviceId, contain):
	global mqttRemoteCSE
	global container
	global strDkey
	global QOS
	global mqttPubPath
	global step
	global mqttContainer

	container = contain

	strRi = generateRi(deviceId)

	mqttContainer = frameMqttContainer.format(mqttRemoteCSE, container)
	bufRequest= frameCreateContainer.format(mqttRemoteCSE, deviceId, strRi, container, strDkey)

	print("3. Create Container :\n payload=%s\n" %str(bufRequest))
	rc = client.publish(mqttPubPath, bufRequest, QOS, False)
	step = MqttStep.CREATE_CONTAINER_REQUESTED

	if rc[0]!=MQTTCLIENT_SUCCESS:
		print("Publish Failed\n\n") 
		return False

	client.loop_start()
	while step<=MqttStep.CREATE_CONTAINER_REQUESTED:
		time.sleep(1)
	client.loop_stop()

	print("Publish Success\n\n")

	return True


def mqttCreateMgmtCmd(client, deviceId):
	global APP_EUI
	global strDkey
	global strExt
	global step
	global mqttPubPath

	strRi = generateRi(deviceId)

	bufRequest = frameCreateMgmtCmd.format(APP_EUI, deviceId, strRi, deviceId, strDkey, strExt)

	print("4. Create Mgmt Cmd :\n payload=%s\n" %str(bufRequest))
	rc = client.publish(mqttPubPath, bufRequest, QOS, False)
	step = MqttStep.CREATE_MGMT_CMD_REQUESTED

	if rc[0]!=MQTTCLIENT_SUCCESS:
		print("Publish Failed\n\n")
		return FALSE

	client.loop_start()
	while step<=MqttStep.CREATE_MGMT_CMD_REQUESTED:
		time.sleep(1)
	client.loop_stop()

	print("Publish Success\n\n")

	return True


def mqttCreateContentInstance(client, deviceId, contain, dataValue):
	global mqttContainer
	global strDkey
	global mqttTopic
	global QOS
	global step	
	global mqttPubPath
	global container
	
	container = contain
	
	strRi = generateRi(deviceId)
	mqttRemoteCSE = frameMqttRemoteCSE.format(APP_EUI, deviceId)
	mqttContainer = frameMqttContainer.format(mqttRemoteCSE, container)
	
	dataName = "text"	# data type
	bufRequest= frameCreateContentInstance.format(
		mqttContainer, deviceId, strRi, strDkey, dataName, dataValue)

	print("5. Create Content Instance :\n payload=%s\n", bufRequest)
	rc=client.publish(mqttPubPath, bufRequest, QOS, False)
	step = MqttStep.CREATE_CONTENT_INSTANCE_REQUESTED

	if rc[0] != MQTTCLIENT_SUCCESS:
		print("Publish Failed\n\n")
		return False

	client.loop_start()
	while step<=MqttStep.CREATE_CONTENT_INSTANCE_REQUESTED:
		time.sleep(1)
	client.loop_stop()

	print("Publish Success\n\n")
	return True


def mqttSubscribe(client, targetDeviceId, container, passWord, callback):
	global deviceId
	global mqttRemoteCSE
	global mqttContainer
	global step
	global mqttPubPath
	global mqttDictCallback

	strRi = generateRi(deviceId)
	notifySubName = container+targetDeviceId
	print(notifySubName)
	mqttDictCallback[notifySubName] = callback
	
	mqttRemoteCSE = frameMqttRemoteCSE.format(APP_EUI, targetDeviceId)
	mqttContainer = frameMqttContainer.format(mqttRemoteCSE, container)

	bufRequest= frameSubscribe.format(
		mqttContainer, deviceId, strRi, notifySubName, passWord, deviceId)

	print("4-1. Subscribe :\n payload=%s\n" %str(bufRequest))
	rc=client.publish(mqttPubPath, bufRequest, QOS, False)
	step = MqttStep.SUBSCRIBE_REQUESTED

	if rc[0] != MQTTCLIENT_SUCCESS:
		print("Publish Failed\n\n")
		return False

	client.loop_start()
	while step<=MqttStep.SUBSCRIBE_REQUESTED:
		time.sleep(1)
	client.loop_stop()

	print("Publish Success\n\n")

	return True

def mqttDeleteSubscribe(client, targetDeviceId, passWord, container):
	global mqttRemoteCSE
	global mqttContainer
	global mqttSubscription
	global mqttPubPath
	global deviceId
	global strNL
	global step

	strRi = generateRi(deviceId)
	notifySubName = container+targetDeviceId
	mqttRemoteCSE = frameMqttRemoteCSE.format(APP_EUI, targetDeviceId)
	mqttContainer = frameMqttContainer.format(mqttRemoteCSE, container)
	mqttSubscription = frameMqttSubscription.format(mqttContainer, notifySubName)
	bufRequest= frameDeleteSubscribe.format(mqttSubscription, deviceId, strRi, passWord, deviceId)
	
	print("4-2. Delete Subscribe : \n payload=%s \n" %str(bufRequest))
	rc = client.publish(mqttPubPath, bufRequest, QOS, False)
	step = MqttStep.DELETE_SUBSCRIBE_REQUESTED
	
	if rc[0] != MQTTCLIENT_SUCCESS:
		print("Publish Failed\n\n")
		return False
	
	client.loop_start()
	while step<=MqttStep.DELETE_SUBSCRIBE_REQUESTED:
		time.sleep(1)
	client.loop_stop()
	
	print("Publish Success\n\n")
	
	return True
