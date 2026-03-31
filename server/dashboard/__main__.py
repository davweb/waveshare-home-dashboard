"""Server for Inkplate Dashboard"""

from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import json
import logging
from . import get_data, initialise_data_fetching
from .config import CONFIG


class DashboardHandler(BaseHTTPRequestHandler):
    """Webserver Class"""

    def do_GET(self) -> None:  # pylint: disable=invalid-name
        """Server any and all GET requests with the same JSON response"""
        body = json.dumps(get_data()).encode()
        self.send_response(200)
        self.send_header('Content-type', 'application/Json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, message_format, *args) -> None:  # pylint: disable=arguments-differ
        """Log a message"""
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
