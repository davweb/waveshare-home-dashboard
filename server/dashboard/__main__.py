"""Server for Waveshare Dashboard"""

from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import json
import logging

from . import initialise_data_fetching
from .config import CONFIG, FIRMWARE_PATH
from . import ota


class DashboardHandler(BaseHTTPRequestHandler):
    """Webserver Class"""

    def do_GET(self) -> None:  # pylint: disable=invalid-name
        """Handle GET requests"""
        path = urlparse(self.path).path
        if path == '/':
            self._handle_health_check()
        elif path == FIRMWARE_PATH:
            self._handle_ota_firmware_download()
        else:
            self.send_error(404)

    def do_POST(self) -> None:  # pylint: disable=invalid-name
        """Handle POST requests"""
        path = urlparse(self.path).path
        if path == FIRMWARE_PATH:
            self._handle_ota_firmware_upload()
        else:
            self.send_error(404)

    # ------------------------------------------------------------------
    # OTA: firmware download (device pulls this via HTTP)
    # ------------------------------------------------------------------

    def _handle_health_check(self) -> None:
        body = b'OK'
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _handle_ota_firmware_download(self) -> None:
        firmware_path = ota.get_firmware_path()
        if firmware_path is None:
            self.send_error(404, 'No firmware available')
            return
        data = firmware_path.read_bytes()
        self.send_response(200)
        self.send_header('Content-type', 'application/octet-stream')
        self.send_header('Content-Disposition', 'attachment; filename="firmware.bin"')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)
        logging.info('Served firmware (%d bytes) to %s', len(data), self.client_address[0])

    # ------------------------------------------------------------------
    # OTA: firmware upload (you push a new build to the server)
    #
    # Usage:
    #   curl -X POST "http://server:8000/ota/firmware?version=1.0.1" \
    #        --data-binary @build/waveshare_dashboard.bin
    # ------------------------------------------------------------------

    def _handle_ota_firmware_upload(self) -> None:
        params = parse_qs(urlparse(self.path).query)
        version_values = params.get('version', [])
        if not version_values:
            self.send_error(400, 'Missing required query parameter: version')
            return

        version = version_values[0].strip()
        if not version:
            self.send_error(400, 'version must not be empty')
            return

        content_length = int(self.headers.get('Content-Length', 0))
        if content_length == 0:
            self.send_error(400, 'Request body is empty')
            return

        data = self.rfile.read(content_length)
        ota.save_firmware(data, version)

        ota.publish_version()

        body = json.dumps({'ok': True, 'version': version, 'size': len(data)}).encode()
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, message_format: str, *args: object) -> None:  # pylint: disable=arguments-differ
        return logging.info(message_format, *args)


def configure_logging() -> None:
    """Configure logging"""

    logging.basicConfig(format='%(asctime)s %(levelname)s %(message)s',
                        datefmt='[%Y-%m-%dT%H:%M:%S%z]',
                        level=CONFIG.log_level)


def start_web_server() -> None:
    """Start the webserver listening on the required port"""
    server = ThreadingHTTPServer(('', CONFIG.port), DashboardHandler)
    server.timeout = 10
    server.serve_forever()


def main() -> None:
    """Entrypoint"""

    configure_logging()
    initialise_data_fetching()

    # Publish OTA version info if firmware is already present on disk
    ota.publish_version()

    start_web_server()


if __name__ == '__main__':
    main()
