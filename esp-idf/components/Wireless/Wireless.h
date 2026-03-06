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
