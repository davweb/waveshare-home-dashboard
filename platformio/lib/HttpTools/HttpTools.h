#include <ArduinoJson.h>

/**
 * Fetch and parse a JSON document from a URL
 *
 * @param doc A reference to a `JsonDocument` object to store the parsed JSON.
 * @param url The URL to fetch the JSON from.
 * @return `true` if the JSON was fetched and parsed successfully, `false` otherwise.
 */
boolean getJsonFromUrl(JsonDocument &doc, String url);
