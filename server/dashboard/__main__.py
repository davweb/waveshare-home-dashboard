"""Server for Waveshare Dashboard"""

from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import json
import logging
from . import get_data, initialise_data_fetching
from .config import CONFIG
from . import ota


class DashboardHandler(BaseHTTPRequestHandler):
    """Webserver Class"""

    def do_GET(self) -> None:  # pylint: disable=invalid-name
        path = urlparse(self.path).path
        if path == '/ota/version':
            self._handle_ota_version()
        elif path == '/ota/firmware':
            self._handle_ota_firmware_download()
        else:
            self._handle_dashboard_data()

    def do_POST(self) -> None:  # pylint: disable=invalid-name
        path = urlparse(self.path).path
        if path == '/ota/firmware':
            self._handle_ota_firmware_upload()
        else:
            self.send_error(404)

    # ------------------------------------------------------------------
    # Dashboard data
    # ------------------------------------------------------------------

    def _handle_dashboard_data(self) -> None:
        body = json.dumps(get_data()).encode()
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    # ------------------------------------------------------------------
    # OTA: version query
    # ------------------------------------------------------------------

    def _handle_ota_version(self) -> None:
        version = ota.get_version()
        firmware_path = ota.get_firmware_path()
        if version is None or firmware_path is None:
            body = json.dumps({'available': False, 'version': None}).encode()
        else:
            body = json.dumps({
                'available': True,
                'version': version,
                'size': firmware_path.stat().st_size,
            }).encode()
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    # ------------------------------------------------------------------
    # OTA: firmware download (device pulls this)
    # ------------------------------------------------------------------

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

        body = json.dumps({'ok': True, 'version': version, 'size': len(data)}).encode()
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, message_format, *args) -> None:  # pylint: disable=arguments-differ
        return logging.info(message_format, *args)


def configure_logging() -> None:
    """Configure logging"""

    logging.basicConfig(encoding='utf-8',
                        format='%(asctime)s %(levelname)s %(message)s',
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
    start_web_server()


if __name__ == '__main__':
    main()
