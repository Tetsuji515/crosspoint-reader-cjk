#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <string>
#include <vector>

#include "BluetoothPageTurnState.h"

class BLEAdvertisedDevice;
class BLEClient;
class BLEScanResults;

class BluetoothPageTurnManager {
 public:
  enum class ConnectionState { Disabled, Idle, Scanning, Connecting, Connected, Error };

  struct ScannedDevice {
    std::string name;
    std::string address;
    int rssi = 0;
    bool hasHidService = false;
  };

  static BluetoothPageTurnManager& getInstance();

  BluetoothPageTurnManager(const BluetoothPageTurnManager&) = delete;
  BluetoothPageTurnManager& operator=(const BluetoothPageTurnManager&) = delete;

  void begin();
  void update();
  void setReaderSessionActive(bool active);
  void setSettingsSessionActive(bool active);

  BluetoothPageTurnState& getState() { return state; }
  const BluetoothPageTurnState& getState() const { return state; }

  void setEnabled(bool enabled);
  bool isEnabled() const;

  bool startScan();
  void stopScan();
  bool connectToDevice(const std::string& address, const std::string& name = "");
  bool connectBondedDevice();
  void disconnect();
  void forgetBondedDevice();

  int getScannedDeviceCount() const;
  ScannedDevice getScannedDevice(int index) const;

  bool hasBondedDevice() const;
  std::string getBondedDeviceName() const;
  std::string getBondedDeviceAddress() const;

  bool isConnected() const;
  ConnectionState getConnectionState() const;
  std::string getStatusMessage() const;
  static void handleScanComplete(BLEScanResults results);

  // Internal callback entrypoints used by BLE callback adapters.
  void handleScanResult(BLEAdvertisedDevice& advertisedDevice);
  void handleClientConnected(BLEClient* connectedClient);
  void handleClientDisconnected();
  void handleInputReport(const uint8_t* data, size_t length);

 private:
  BluetoothPageTurnManager();
  ~BluetoothPageTurnManager();

  bool ensureInitialized();
  void updateRuntimeState();
  void deactivateRuntime();
  bool subscribeToInputReports();
  void rememberBondedDevice(const std::string& address, const std::string& name);
  void clearBondedDevice();
  void clearPendingMasks();
  void setConnectionState(ConnectionState newState, const std::string& message = "");
  static bool isLikelyHidDevice(BLEAdvertisedDevice& advertisedDevice);

  mutable SemaphoreHandle_t mutex = nullptr;
  BluetoothPageTurnState state;
  std::vector<ScannedDevice> scannedDevices;
  BLEClient* client = nullptr;
  bool initialized = false;
  bool subscriptionsActive = false;
  bool readerSessionActive = false;
  bool settingsSessionActive = false;
  bool runtimeActive = false;
  uint8_t appliedMask = 0;
  std::atomic<uint8_t> keyboardMask = 0;
  std::atomic<uint8_t> consumerMask = 0;
  ConnectionState connectionState = ConnectionState::Disabled;
  std::string statusMessage;
};

#define BLUETOOTH_PAGE_TURN BluetoothPageTurnManager::getInstance()
