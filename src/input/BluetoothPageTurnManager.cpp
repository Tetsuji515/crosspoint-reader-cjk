#include "BluetoothPageTurnManager.h"

#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <BLESecurity.h>
#include <BLEUUID.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "BluetoothPageTurnReportParser.h"
#include "CrossPointSettings.h"

namespace {
constexpr uint16_t kHidServiceUuid = 0x1812;
constexpr uint16_t kReportCharacteristicUuid = 0x2A4D;
constexpr uint16_t kBootKeyboardInputUuid = 0x2A22;
constexpr uint16_t kKeyboardAppearance = 0x03C1;
constexpr uint16_t kGenericHidAppearance = 0x03C0;
constexpr uint32_t kScanDurationSeconds = 5;

class BluetoothSecurityCallbacks final : public BLESecurityCallbacks {
 public:
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
  bool onSecurityRequest() override { return true; }
  bool onConfirmPIN(uint32_t) override { return true; }
  bool onAuthorizationRequest(uint16_t, uint16_t, bool) override { return true; }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t) override {}
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc*) override {}
#endif
};

class BluetoothScanCallbacks final : public BLEAdvertisedDeviceCallbacks {
 public:
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    BluetoothPageTurnManager::getInstance().handleScanResult(advertisedDevice);
  }
};

class BluetoothClientCallbacks final : public BLEClientCallbacks {
 public:
  void onConnect(BLEClient* client) override { BluetoothPageTurnManager::getInstance().handleClientConnected(client); }
  void onDisconnect(BLEClient*) override { BluetoothPageTurnManager::getInstance().handleClientDisconnected(); }
};

BluetoothSecurityCallbacks g_securityCallbacks;
BluetoothScanCallbacks g_scanCallbacks;
BluetoothClientCallbacks g_clientCallbacks;

}  // namespace

BluetoothPageTurnManager& BluetoothPageTurnManager::getInstance() {
  static BluetoothPageTurnManager instance;
  return instance;
}

BluetoothPageTurnManager::BluetoothPageTurnManager() : mutex(xSemaphoreCreateMutex()) {}

BluetoothPageTurnManager::~BluetoothPageTurnManager() {
  if (mutex != nullptr) {
    vSemaphoreDelete(mutex);
    mutex = nullptr;
  }
}

bool BluetoothPageTurnManager::ensureInitialized() {
  if (initialized) {
    return true;
  }

  if (!SETTINGS.bluetoothPageTurnEnabled) {
    setConnectionState(ConnectionState::Disabled);
    return false;
  }

  BLEDevice::init("");
  BLESecurity security;
  (void)security;
  BLESecurity::setAuthenticationMode(ESP_LE_AUTH_BOND);
  BLESecurity::setCapability(ESP_IO_CAP_NONE);
  BLEDevice::setSecurityCallbacks(&g_securityCallbacks);

  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(80);
  scan->setWindow(60);
  scan->setAdvertisedDeviceCallbacks(&g_scanCallbacks, false, true);

  initialized = true;
  setConnectionState(ConnectionState::Idle);
  return true;
}

void BluetoothPageTurnManager::begin() {
  if (!SETTINGS.bluetoothPageTurnEnabled) {
    setConnectionState(ConnectionState::Disabled);
    return;
  }

  setConnectionState(ConnectionState::Idle);
}

void BluetoothPageTurnManager::update() {
  state.clearFrameEvents();

  const uint8_t nextMask = keyboardMask.load() | consumerMask.load();

  if ((appliedMask & BluetoothPageTurnReportParser::PAGE_BACK_MASK) == 0 &&
      (nextMask & BluetoothPageTurnReportParser::PAGE_BACK_MASK) != 0) {
    state.reportKeyDown(BluetoothPageTurnState::Key::PageBack);
  } else if ((appliedMask & BluetoothPageTurnReportParser::PAGE_BACK_MASK) != 0 &&
             (nextMask & BluetoothPageTurnReportParser::PAGE_BACK_MASK) == 0) {
    state.reportKeyUp(BluetoothPageTurnState::Key::PageBack);
  }

  if ((appliedMask & BluetoothPageTurnReportParser::PAGE_FORWARD_MASK) == 0 &&
      (nextMask & BluetoothPageTurnReportParser::PAGE_FORWARD_MASK) != 0) {
    state.reportKeyDown(BluetoothPageTurnState::Key::PageForward);
  } else if ((appliedMask & BluetoothPageTurnReportParser::PAGE_FORWARD_MASK) != 0 &&
             (nextMask & BluetoothPageTurnReportParser::PAGE_FORWARD_MASK) == 0) {
    state.reportKeyUp(BluetoothPageTurnState::Key::PageForward);
  }

  appliedMask = nextMask;
}

void BluetoothPageTurnManager::setReaderSessionActive(const bool active) {
  if (readerSessionActive == active) {
    return;
  }

  readerSessionActive = active;
  updateRuntimeState();
}

void BluetoothPageTurnManager::setSettingsSessionActive(const bool active) {
  if (settingsSessionActive == active) {
    return;
  }

  settingsSessionActive = active;
  updateRuntimeState();
}

void BluetoothPageTurnManager::setEnabled(const bool enabled) {
  SETTINGS.bluetoothPageTurnEnabled = enabled ? 1 : 0;
  SETTINGS.saveToFile();

  updateRuntimeState();
}

bool BluetoothPageTurnManager::isEnabled() const { return SETTINGS.bluetoothPageTurnEnabled != 0; }

bool BluetoothPageTurnManager::startScan() {
  if (!ensureInitialized()) {
    return false;
  }

  disconnect();

  if (mutex != nullptr) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    scannedDevices.clear();
    xSemaphoreGive(mutex);
  }

  setConnectionState(ConnectionState::Scanning, "");
  return BLEDevice::getScan()->start(kScanDurationSeconds, handleScanComplete, false);
}

void BluetoothPageTurnManager::stopScan() {
  if (!initialized) {
    return;
  }
  BLEDevice::getScan()->stop();
  if (connectionState == ConnectionState::Scanning) {
    setConnectionState(ConnectionState::Idle);
  }
}

bool BluetoothPageTurnManager::connectToDevice(const std::string& address, const std::string& name) {
  if (!ensureInitialized() || address.empty()) {
    return false;
  }

  const std::string displayName = name.empty() ? address : name;

  stopScan();
  clearPendingMasks();
  subscriptionsActive = false;
  setConnectionState(ConnectionState::Connecting, displayName);

  if (client == nullptr) {
    client = BLEDevice::createClient();
    client->setClientCallbacks(&g_clientCallbacks);
  }

  if (client->isConnected()) {
    client->disconnect();
  }

  BLEAddress bleAddress(String(address.c_str()));
  if (!client->connect(bleAddress)) {
    setConnectionState(ConnectionState::Error, displayName);
    return false;
  }

  client->secureConnection();

  if (!subscribeToInputReports()) {
    setConnectionState(ConnectionState::Error, displayName);
    client->disconnect();
    return false;
  }

  rememberBondedDevice(address, displayName);
  setConnectionState(ConnectionState::Connected, displayName);
  return true;
}

bool BluetoothPageTurnManager::connectBondedDevice() {
  if (!SETTINGS.bluetoothPageTurnBonded || SETTINGS.bluetoothPageTurnAddr[0] == '\0') {
    return false;
  }
  return connectToDevice(SETTINGS.bluetoothPageTurnAddr, SETTINGS.bluetoothPageTurnName);
}

void BluetoothPageTurnManager::disconnect() {
  stopScan();
  clearPendingMasks();
  subscriptionsActive = false;
  if (client != nullptr && client->isConnected()) {
    client->disconnect();
  }
  if (isEnabled()) {
    setConnectionState(ConnectionState::Idle);
  }
}

void BluetoothPageTurnManager::forgetBondedDevice() {
  disconnect();
  clearBondedDevice();
}

void BluetoothPageTurnManager::updateRuntimeState() {
  const bool shouldKeepRuntimeActive = isEnabled() && (readerSessionActive || (settingsSessionActive && initialized));
  if (!shouldKeepRuntimeActive) {
    deactivateRuntime();
    return;
  }

  runtimeActive = true;
  if (!ensureInitialized()) {
    return;
  }

  if (readerSessionActive && SETTINGS.bluetoothPageTurnBonded && SETTINGS.bluetoothPageTurnAddr[0] != '\0' &&
      !isConnected() && connectionState != ConnectionState::Connecting) {
    connectBondedDevice();
    return;
  }

  if (connectionState == ConnectionState::Disabled) {
    setConnectionState(ConnectionState::Idle);
  }
}

void BluetoothPageTurnManager::deactivateRuntime() {
  runtimeActive = false;
  disconnect();

  if (mutex != nullptr) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    scannedDevices.clear();
    xSemaphoreGive(mutex);
  }

  client = nullptr;
  if (initialized) {
    BLEDevice::deinit(false);
    initialized = false;
  }

  subscriptionsActive = false;
  clearPendingMasks();

  if (isEnabled()) {
    setConnectionState(ConnectionState::Idle);
  } else {
    setConnectionState(ConnectionState::Disabled);
  }
}

int BluetoothPageTurnManager::getScannedDeviceCount() const {
  if (mutex == nullptr) {
    return 0;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);
  const int count = static_cast<int>(scannedDevices.size());
  xSemaphoreGive(mutex);
  return count;
}

BluetoothPageTurnManager::ScannedDevice BluetoothPageTurnManager::getScannedDevice(const int index) const {
  ScannedDevice device;
  if (mutex == nullptr) {
    return device;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);
  if (index >= 0 && index < static_cast<int>(scannedDevices.size())) {
    device = scannedDevices[index];
  }
  xSemaphoreGive(mutex);
  return device;
}

bool BluetoothPageTurnManager::hasBondedDevice() const { return SETTINGS.bluetoothPageTurnBonded != 0; }

std::string BluetoothPageTurnManager::getBondedDeviceName() const { return SETTINGS.bluetoothPageTurnName; }

std::string BluetoothPageTurnManager::getBondedDeviceAddress() const { return SETTINGS.bluetoothPageTurnAddr; }

bool BluetoothPageTurnManager::isConnected() const {
  return client != nullptr && client->isConnected() && connectionState == ConnectionState::Connected;
}

BluetoothPageTurnManager::ConnectionState BluetoothPageTurnManager::getConnectionState() const {
  return connectionState;
}

std::string BluetoothPageTurnManager::getStatusMessage() const { return statusMessage; }

void BluetoothPageTurnManager::handleScanComplete(BLEScanResults) {
  auto& manager = BluetoothPageTurnManager::getInstance();
  if (manager.connectionState == ConnectionState::Scanning) {
    manager.setConnectionState(ConnectionState::Idle);
  }
}

void BluetoothPageTurnManager::handleScanResult(BLEAdvertisedDevice& advertisedDevice) {
  if (!isLikelyHidDevice(advertisedDevice)) {
    return;
  }

  ScannedDevice device;
  device.name = advertisedDevice.haveName() ? advertisedDevice.getName().c_str()
                                            : advertisedDevice.getAddress().toString().c_str();
  device.address = advertisedDevice.getAddress().toString().c_str();
  device.rssi = advertisedDevice.haveRSSI() ? advertisedDevice.getRSSI() : 0;
  device.hasHidService =
      advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(kHidServiceUuid));

  if (mutex == nullptr) {
    return;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);
  auto it = std::find_if(scannedDevices.begin(), scannedDevices.end(),
                         [&device](const ScannedDevice& existing) { return existing.address == device.address; });
  if (it == scannedDevices.end()) {
    scannedDevices.push_back(device);
  } else {
    *it = device;
  }
  std::sort(scannedDevices.begin(), scannedDevices.end(),
            [](const ScannedDevice& a, const ScannedDevice& b) { return a.rssi > b.rssi; });
  xSemaphoreGive(mutex);
}

void BluetoothPageTurnManager::handleClientConnected(BLEClient* connectedClient) { client = connectedClient; }

void BluetoothPageTurnManager::handleClientDisconnected() {
  subscriptionsActive = false;
  clearPendingMasks();
  if (isEnabled()) {
    setConnectionState(ConnectionState::Idle);
  }
}

void BluetoothPageTurnManager::handleInputReport(const uint8_t* data, const size_t length) {
  if (!data || length == 0) {
    return;
  }

  const uint8_t keyboard = BluetoothPageTurnReportParser::parseKeyboardReport(data, length);
  if (keyboard != 0 || length >= 8) {
    keyboardMask.store(keyboard);
    return;
  }

  consumerMask.store(BluetoothPageTurnReportParser::parseConsumerReport(data, length));
}

bool BluetoothPageTurnManager::subscribeToInputReports() {
  if (client == nullptr || !client->isConnected()) {
    return false;
  }

  BLERemoteService* hidService = client->getService(BLEUUID(kHidServiceUuid));
  if (hidService == nullptr) {
    return false;
  }

  bool subscribed = false;
  if (BLERemoteCharacteristic* bootKeyboardInput = hidService->getCharacteristic(BLEUUID(kBootKeyboardInputUuid))) {
#if defined(CONFIG_NIMBLE_ENABLED)
    subscribed =
        bootKeyboardInput->subscribe(true, [](BLERemoteCharacteristic*, const uint8_t* data, size_t length, bool) {
          BluetoothPageTurnManager::getInstance().handleInputReport(data, length);
        });
#else
    bootKeyboardInput->registerForNotify([](BLERemoteCharacteristic*, const uint8_t* data, size_t length, bool) {
      BluetoothPageTurnManager::getInstance().handleInputReport(data, length);
    });
    subscribed = true;
#endif
  }

  auto* characteristics = hidService->getCharacteristicsByHandle();
  for (const auto& entry : *characteristics) {
    BLERemoteCharacteristic* characteristic = entry.second;
    if (!characteristic->getUUID().equals(BLEUUID(kReportCharacteristicUuid))) {
      continue;
    }
#if defined(CONFIG_NIMBLE_ENABLED)
    subscribed = characteristic->subscribe(true, [](BLERemoteCharacteristic*, const uint8_t* data, size_t length,
                                                    bool) {
      BluetoothPageTurnManager::getInstance().handleInputReport(data, length);
    }) || subscribed;
#else
    characteristic->registerForNotify([](BLERemoteCharacteristic*, const uint8_t* data, size_t length, bool) {
      BluetoothPageTurnManager::getInstance().handleInputReport(data, length);
    });
    subscribed = true;
#endif
  }

  subscriptionsActive = subscribed;
  return subscribed;
}

void BluetoothPageTurnManager::rememberBondedDevice(const std::string& address, const std::string& name) {
  SETTINGS.bluetoothPageTurnBonded = 1;
  strncpy(SETTINGS.bluetoothPageTurnAddr, address.c_str(), sizeof(SETTINGS.bluetoothPageTurnAddr) - 1);
  SETTINGS.bluetoothPageTurnAddr[sizeof(SETTINGS.bluetoothPageTurnAddr) - 1] = '\0';
  strncpy(SETTINGS.bluetoothPageTurnName, name.c_str(), sizeof(SETTINGS.bluetoothPageTurnName) - 1);
  SETTINGS.bluetoothPageTurnName[sizeof(SETTINGS.bluetoothPageTurnName) - 1] = '\0';
  SETTINGS.saveToFile();
}

void BluetoothPageTurnManager::clearBondedDevice() {
  SETTINGS.bluetoothPageTurnBonded = 0;
  SETTINGS.bluetoothPageTurnAddr[0] = '\0';
  SETTINGS.bluetoothPageTurnName[0] = '\0';
  SETTINGS.saveToFile();
}

void BluetoothPageTurnManager::clearPendingMasks() {
  keyboardMask.store(0);
  consumerMask.store(0);
  appliedMask = 0;
}

void BluetoothPageTurnManager::setConnectionState(const ConnectionState newState, const std::string& message) {
  connectionState = newState;
  statusMessage = message;
}

bool BluetoothPageTurnManager::isLikelyHidDevice(BLEAdvertisedDevice& advertisedDevice) {
  if (!advertisedDevice.isConnectable()) {
    return false;
  }

  if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(kHidServiceUuid))) {
    return true;
  }

  if (advertisedDevice.haveAppearance()) {
    const uint16_t appearance = advertisedDevice.getAppearance();
    return appearance == kKeyboardAppearance || appearance == kGenericHidAppearance;
  }

  return false;
}
