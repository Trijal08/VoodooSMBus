/*
 * VoodooSMBusDeviceNub.cpp
 * SMBus Controller Driver for macOS X
 *
 * Copyright (c) 2019 Leonard Kleinhans <leo-labs>
 *
 */

#include "VoodooSMBusControllerDriver.hpp"
#include "VoodooSMBusDeviceNub.hpp"

#define super IOService
OSDefineMetaClassAndStructors(VoodooSMBusDeviceNub, IOService);

bool VoodooSMBusDeviceNub::init() {
    if(!super::init()) {
        return false;
    }
    
    workloop = IOWorkLoop::workLoop();
    if (!workloop) {
        return false;
    }

    auto action = OSMemberFunctionCast(IOInterruptEventAction, this, &VoodooSMBusDeviceNub::handleHostNotifyGated);
    interruptSource = IOInterruptEventSource::interruptEventSource(this, action, nullptr);
    if (!interruptSource) {
        return false;
    }
    workloop->addEventSource(interruptSource);
    return true;
}

void VoodooSMBusDeviceNub::free(void) {
    if (interruptSource) {
        workloop->removeEventSource(interruptSource);
        interruptSource->release();
        interruptSource = nullptr;
    }

    OSSafeReleaseNULL(workloop);
    super::free();
}

void VoodooSMBusDeviceNub::handleHostNotifyGated (OSObject* owner, IOInterruptEventSource* src, int intCount) {
    IOService* device_driver = getClient();
    
    if(device_driver) {
        super::messageClient(kIOMessageVoodooSMBusHostNotify, device_driver);
    }
}

void VoodooSMBusDeviceNub::handleHostNotify() {
    interruptSource->interruptOccurred(nullptr, nullptr, 0);
}

bool VoodooSMBusDeviceNub::attach(IOService* provider, UInt8 address) {
    if (!super::attach(provider))
        return false;
    
    controller = OSDynamicCast(VoodooSMBusControllerDriver, provider);
    if (!controller) {
        IOLogError("%s Could not get controller", provider->getName());
        return false;
    }
    
    setProperty("VoodooSMBUS Slave Device Address", address, 8);
    slave_device.addr = address;
    slave_device.flags = 0;
    
    return true;
}

IOReturn VoodooSMBusDeviceNub::wakeupController() {
    if (controller) {
       return controller->makeUsable();
    } else {
       return kIOReturnError;
    }
}

void VoodooSMBusDeviceNub::setSlaveDeviceFlags(unsigned short flags) {
    slave_device.flags = flags;
}

IOReturn VoodooSMBusDeviceNub::readByteData(u8 command) {
    return controller->readByteData(&slave_device, command);
}

IOReturn VoodooSMBusDeviceNub::readBlockData(u8 command, u8 *values) {
    return controller->readBlockData(&slave_device, command, values);
}

IOReturn VoodooSMBusDeviceNub::writeByteData(u8 command, u8 value) {
    return controller->writeByteData(&slave_device, command, value);
}

IOReturn VoodooSMBusDeviceNub::writeByte(u8 value) {
    return controller->writeByte(&slave_device, value);
}

IOReturn VoodooSMBusDeviceNub::writeBlockData(u8 command, u8 length, const u8 *values) {
    return controller->writeBlockData(&slave_device, command, length, values);
}

bool VoodooSMBusDeviceNub::createPS2Stub(const char *ps2TrackpadName, const char *ps2DictName, IOService **ps2Controller) {
    // Previous driver killed the PS2 trackpad already
    if (grabPS2Info() != nullptr) {
        *ps2Controller = controller->grabService("ApplePS2Controller");
        return *ps2Controller != nullptr;
    }
    
    IOService *ps2Trackpad = controller->grabService(ps2TrackpadName);
    *ps2Controller = controller->grabService("ApplePS2Controller");
    
    if (ps2Trackpad == nullptr || ps2Controller == nullptr) {
        OSSafeReleaseNULL(ps2Trackpad);
        OSSafeReleaseNULL(*ps2Controller);
        return nullptr;
    }
    
    // Grab any useful information from Trackpad driver
    OSObject *gpio = ps2Trackpad->getProperty(ps2DictName);
    if (gpio != nullptr) {
        IOLogDebug("Found GPIO data!");
        setProperty("PS/2 Data", gpio);
    }
    
    // Do a reset over PS2, replace the PS2 Synaptics Driver with a stub driver
    bool stubCreated = controller->createPS2Stub(ps2Trackpad);
    OSSafeReleaseNULL(ps2Trackpad);
    return stubCreated;
}

OSDictionary *VoodooSMBusDeviceNub::grabPS2Info() {
    return OSDynamicCast(OSDictionary, getProperty("PS/2 Data"));
}

bool VoodooSMBusDeviceNub::acidantheraTrackpadExists() {
    return controller->acidantheraTrackpadExists();
}
