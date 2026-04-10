/**
 * Callback invoked on WiFi connect (connected=true, IP obtained) and
 * disconnect (connected=false). Register before calling startWiFi().
 */
typedef void (*WifiStateCallback)(bool connected);

/**
 * Register a callback to be invoked on WiFi connect and disconnect.
 */
void setWiFiStateCallback(WifiStateCallback callback);

/**
 * Connect to the WiFi network, logging the result.
 *
 */
void startWiFi();

/**
 * Disconnect from the WiFi network, logging the result.
 *
 * @return `true` if the WiFi network was disconnected from successfully, `false` otherwise.
 */
bool stopWiFi();

/**
 * Check if the device is connected to a WiFi network.
 *
 * @return `true` if the device is connected to a WiFi network, `false` otherwise.
*/
bool isWiFiConnected();

/**
 * Get the local IP address of the device a string.
 *
 * @return The local IP address of the device as a string if the device is
 * connected to a WiFi network, an empty string otherwise.
*/
const char* getLocalIpAddress();

/**
 * Get the Mac address of the device a string.
 *
 * @return The MAC address of the device as a string.
*/
const char* getMacAddress();

/**
 * Get the subnet mask as a string.
 *
 * @return The subnet mask as a string if connected, empty string otherwise.
 */
const char* getSubnetMask();

/**
 * Get the default gateway as a string.
 *
 * @return The default gateway as a string if connected, empty string otherwise.
 */
const char* getDefaultGateway();

/**
 * Get the SSID of the connected WiFi network.
 *
 * @return The SSID as a string if connected, empty string otherwise.
 */
const char* getWiFiSSID();

/**
 * Get the WiFi signal strength (RSSI).
 *
 * @return RSSI in dBm, or 0 if not connected.
 */
int getWiFiRSSI();
