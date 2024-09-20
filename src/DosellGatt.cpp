// Copyright 2017-2019 Paul Nettle
//
// This file is part of Gobbledegook.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.

#include <algorithm>

#include "DosellGatt.h"
#include "ServerUtils.h"
#include "Utils.h"
#include "Globals.h"
#include "DBusObject.h"
#include "DBusInterface.h"
#include "GattProperty.h"
#include "GattService.h"
#include "GattUuid.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "Logger.h"

namespace ggk {

// There's a good chance there will be a bunch of unused parameters from the lambda macros
#if defined(__GNUC__) && defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------------------------------------------------------------

// Our one and only server. It's global.
std::shared_ptr<DosellGatt> THESERVER = nullptr;

DosellGatt::DosellGatt(const std::string &serviceName, const std::string &advertisingName, const std::string &advertisingShortName, 
	GGKServerDataGetter getter, GGKServerDataSetter setter)
	:mEnableBREDR(false),
	mEnableSecureConnection(false),
	mEnableConnectable(true),
	mEnableDiscoverable(true),
	mEnableAdvertising(true),
	mEnableBondable(false),
	mAdvertisingName(advertisingName),
	mAdvertisingShortName(advertisingShortName),
	mServiceName(serviceName)
{
	// Save our names
	std::transform(mServiceName.begin(), mServiceName.end(), mServiceName.begin(), ::tolower);

	// Register getter & setter for server data
	dataGetter = getter;
	dataSetter = setter;


	//
	// Define the server
	//

	// Create the root D-Bus object and push it into the list
	mObjects.push_back(DBusObject(DBusObjectPath() + "com" + getServiceName()));

	// We're going to build off of this object, so we need to get a reference to the instance of the object as it resides in the
	// list (and not the object that would be added to the list.)
	mObjects.back()
	// Both GATT Generic Access Service (1800) and GATT Generic Attribute Service (0x1801) are created and managed by BlueZ
	// Trying to create them here will render "DBus.Error:org.bluez.Error.Failed: Failed to create entry in database"
	
	// Service: Device Information (0x180A)
	.gattServiceBegin("device/information", "180A")
		.gattCharacteristicBegin("manufacture/name", "2A29", {"read"}) 
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				self.methodReturnValue(pInvocation, "Dosell AB", true);
			})

		.gattCharacteristicEnd()
		.gattCharacteristicBegin("hardware/revision", "2A27", {"read"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				self.methodReturnValue(pInvocation, "V3", true);
			})
		.gattDescriptorBegin("description", "2901", {"read"})
			.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
			{
				const char *pDescription = "Device Information";
				self.methodReturnValue(pInvocation, pDescription, true);
			})
		.gattDescriptorEnd()
		.gattCharacteristicEnd()

	.gattServiceEnd()
	//     GATT Dosell Service-1 (6151EC38-ECFA-4EE0-BBF7-50C1B04F4322)
	.gattServiceBegin("service/1", "6151EC38-ECFA-4EE0-BBF7-50C1B04F4322")
		.gattCharacteristicBegin("authentication/id", "6151BE6E-ECFA-4EE0-BBF7-50C1B04F4322", {"read"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Retrieve the array pointer using getDataValue
				auto pTextString = self.getDataValue<unsigned char>("authentication/id", nullptr);
				// int arrayLength = self.getDataValue<int>("authentication/id_length", nullptr);
				// Check if data was retrieved and is valid
				if (pTextString == nullptr)
				{
					self.methodReturnValue(pInvocation, nullptr, false);
					return;
				}
				GVariant* variant = Utils::gvariantFromByteArray(pTextString, 11);
				self.methodReturnVariant(pInvocation, variant, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("authentication/id", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "authentication/id"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("authentication/id", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Authentication-ID (=ICCID) encoded as BCD coded string.";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		
		// Status 6151ED7B-ECFA-4EE0-BBF7-50C1B04F4322
		.gattCharacteristicBegin("status", "6151ED7B-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "notify"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "status"; //pName is the lookup name in dataGetter(const char *pName)
				u_int64_t stat = self.getDataValue(pName.c_str(), static_cast<uint64_t>(0));
				self.methodReturnValue(pInvocation, stat, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("status", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "status"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("status", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.onEvent(2, nullptr, CHARACTERISTIC_EVENT_CALLBACK_LAMBDA
			{
				std::string pName = "status"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.sendChangeNotificationValue(pConnection, pTextString);
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Status";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		// Control 6151E030-ECFA-4EE0-BBF7-50C1B04F4322 
		.gattCharacteristicBegin("control", "6151E030-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write", "notify"}) // "encrypt-write"
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "control"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("control", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "control"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("control", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.onEvent(2, nullptr, CHARACTERISTIC_EVENT_CALLBACK_LAMBDA
			{
				std::string pName = "control"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.sendChangeNotificationValue(pConnection, pTextString);
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Control";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		// Factory Reset Enable 61517D43-ECFA-4EE0-BBF7-50C1B04F4322 
		.gattCharacteristicBegin("factory/reset/enable", "61517D43-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "factory/reset/enable"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("factory/reset/enable", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "factory/reset/enable"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("factory/reset/enable", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Factory Reset Enable";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		// Caregiver token 6151A71F-ECFA-4EE0-BBF7-50C1B04F4322 
		.gattCharacteristicBegin("caregiver/token", "6151A71F-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "caregiver/token"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("caregiver/token", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "caregiver/token"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("caregiver/token", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Caregiver token";
					self.methodReturnValue(pInvocation, pDescription, true);
				})

			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		// Current time 615124D3-ECFA-4EE0-BBF7-50C1B04F4322 
		.gattCharacteristicBegin("current/time", "615124D3-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write", "notify"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "current/time"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("current/time", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "current/time"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("current/time", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Current time";
					self.methodReturnValue(pInvocation, pDescription, true);
				})

			.gattDescriptorEnd()
		.gattCharacteristicEnd()
	.gattServiceEnd()
	//     GATT Dosell Service-2 (61515260-ECFA-4EE0-BBF7-50C1B04F4322)
	.gattServiceBegin("service/2", "61515260-ECFA-4EE0-BBF7-50C1B04F4322")
		.gattCharacteristicBegin("name/first", "2A8A", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "name/first"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("name/first", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "name/first"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("name/first", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "First Name";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		.gattCharacteristicBegin("name/last", "2A90", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "name/last"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("name/last", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "name/last"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("name/last", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Last Name";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		.gattCharacteristicBegin("birthday", "61516D3B-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "birthday"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("birthday", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "birthday"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("birthday", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Birthday";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		.gattCharacteristicBegin("dispense/lastdate", "61515ACE-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/lastdate"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("dispense/lastdate", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/lastdate"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("dispense/lastdate", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Last Dispense Date";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		.gattCharacteristicBegin("dispense/daysbeforelastdispensealert", "6151BD09-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/daysbeforelastdispensealert"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("dispense/daysbeforelastdispensealert", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/daysbeforelastdispensealert"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("dispense/daysbeforelastdispensealert", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Days Before Last Dispense Date Alert";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		.gattCharacteristicBegin("dispense/daysbeforelastdispensenotification", "61517926-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/daysbeforelastdispensenotification"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("dispense/daysbeforelastdispensenotification", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/daysbeforelastdispensenotification"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("dispense/daysbeforelastdispensenotification", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Days Before Last Dispense Date Notification";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		.gattCharacteristicBegin("uncollected/minutesbefore", "615135D0-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "uncollected/minutesbefore"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("uncollected/minutesbefore", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "uncollected/minutesbefore"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("uncollected/minutesbefore", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Minutes Before Uncollected Sachet Notification";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		.gattCharacteristicBegin("dispense/nexttime", "6151B9E4-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "notify"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/nexttime"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("dispense/nexttime", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/nexttime"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("dispense/nexttime", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Next Dispense Time";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()
		.gattCharacteristicBegin("dispense/first", "615135D1-ECFA-4EE0-BBF7-50C1B04F4322", {"read", "write"})
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/first"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>(pName.c_str(), "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("dispense/first", Utils::stringFromGVariantByteArray(pAyBuffer).c_str());
				self.callOnUpdatedValue(pConnection, pUserData);
				self.methodReturnVariant(pInvocation, NULL);
			})
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				std::string pName = "dispense/first"; //pName is the lookup name in dataGetter(const char *pName)
				const char *pTextString = self.getDataPointer<const char *>("dispense/first", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})
			.gattDescriptorBegin("description", "2901", {"read"})
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "First Dispense Time";
					self.methodReturnValue(pInvocation, pDescription, true);
				})
			.gattDescriptorEnd()
		.gattCharacteristicEnd()		
	.gattServiceEnd(); // << -- NOTE THE SEMICOLON

	//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
	//                                                ____ _____ ___  _____
	//                                               / ___|_   _/ _ \|  _  |
	//                                               \___ \ | || | | | |_) |
	//                                                ___) || || |_| |  __/
	//                                               |____/ |_| \___/|_|
	//
	// You probably shouldn't mess with stuff beyond this point. It is required to meet BlueZ's requirements for a GATT Service.
	//
	// >>
	// >>  WHAT IT IS
	// >>
	//
	// From the BlueZ D-Bus GATT API description (https://git.kernel.org/pub/scm/bluetooth/bluez.git/plain/doc/gatt-api.txt):
	//
	//     "To make service registration simple, BlueZ requires that all objects that belong to a GATT service be grouped under a
	//     D-Bus Object Manager that solely manages the objects of that service. Hence, the standard DBus.ObjectManager interface
	//     must be available on the root service path."
	//
	// The code below does exactly that. Notice that we're doing much of the same work that our Server description does except that
	// instead of defining our own interfaces, we're following a pre-defined standard.
	//
	// The object types and method names used in the code below may look unfamiliar compared to what you're used to seeing in the
	// Server desecription. That's because the server description uses higher level types that define a more GATT-oriented framework
	// to build your GATT services. That higher level functionality was built using a set of lower-level D-Bus-oriented framework,
	// which is used in the code below.
	//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

	// Create the root object and push it into the list. We're going to build off of this object, so we need to get a reference
	// to the instance of the object as it resides in the list (and not the object that would be added to the list.)
	//
	// This is a non-published object (as specified by the 'false' parameter in the DBusObject constructor.) This way, we can
	// include this within our server hieararchy (i.e., within the `objects` list) but it won't be exposed by BlueZ as a Bluetooth
	// service to clietns.
	mObjects.push_back(DBusObject(DBusObjectPath(), false));

	// Get a reference to the new object as it resides in the list
	DBusObject &objectManager = mObjects.back();

	// Create an interface of the standard type 'org.freedesktop.DBus.ObjectManager'
	//
	// See: https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager
	auto omInterface = std::make_shared<DBusInterface>(objectManager, "org.freedesktop.DBus.ObjectManager");

	// Add the interface to the object manager
	objectManager.addInterface(omInterface);

	// Finally, we setup the interface. We do this by adding the `GetManagedObjects` method as specified by D-Bus for the
	// 'org.freedesktop.DBus.ObjectManager' interface.
	const char *pInArgs[] = { nullptr };
	const char *pOutArgs = "a{oa{sa{sv}}}";
	omInterface->addMethod("GetManagedObjects", pInArgs, pOutArgs, INTERFACE_METHOD_CALLBACK_LAMBDA
	{
		ServerUtils::getManagedObjects(pInvocation);
	});
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Utilitarian
// ---------------------------------------------------------------------------------------------------------------------------------

// Find a D-Bus interface within the given D-Bus object
//
// If the interface was found, it is returned, otherwise nullptr is returned
std::shared_ptr<const DBusInterface> DosellGatt::findInterface(const DBusObjectPath &objectPath, const std::string &interfaceName) const
{
	for (const DBusObject &object : mObjects)
	{
		std::shared_ptr<const DBusInterface> pInterface = object.findInterface(objectPath, interfaceName);
		if (pInterface != nullptr)
		{
			return pInterface;
		}
	}

	return nullptr;
}

// Find and call a D-Bus method within the given D-Bus object on the given D-Bus interface
//
// If the method was called, this method returns true, otherwise false. There is no result from the method call itself.
bool DosellGatt::callMethod(const DBusObjectPath &objectPath, const std::string &interfaceName, const std::string &methodName, GDBusConnection *pConnection, GVariant *pParameters, GDBusMethodInvocation *pInvocation, gpointer pUserData) const
{
	for (const DBusObject &object : mObjects)
	{
		if (object.callMethod(objectPath, interfaceName, methodName, pConnection, pParameters, pInvocation, pUserData))
		{
			return true;
		}
	}

	return false;
}

// Find a GATT Property within the given D-Bus object on the given D-Bus interface
//
// If the property was found, it is returned, otherwise nullptr is returned
const GattProperty *DosellGatt::findProperty(const DBusObjectPath &objectPath, const std::string &interfaceName, const std::string &propertyName) const
{
	std::shared_ptr<const DBusInterface> pInterface = findInterface(objectPath, interfaceName);

	// Try each of the GattInterface types that support properties?
	if (std::shared_ptr<const GattInterface> pGattInterface = TRY_GET_CONST_INTERFACE_OF_TYPE(pInterface, GattInterface))
	{
		return pGattInterface->findProperty(propertyName);
	}
	else if (std::shared_ptr<const GattService> pGattInterface = TRY_GET_CONST_INTERFACE_OF_TYPE(pInterface, GattService))
	{
		return pGattInterface->findProperty(propertyName);
	}
	else if (std::shared_ptr<const GattCharacteristic> pGattInterface = TRY_GET_CONST_INTERFACE_OF_TYPE(pInterface, GattCharacteristic))
	{
		return pGattInterface->findProperty(propertyName);
	}

	return nullptr;
}

}; // namespace ggk
