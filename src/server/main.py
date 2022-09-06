from server.server import ServerController as Server
import structlog
import logging
import sys

structlog.configure(
    wrapper_class=structlog.make_filtering_bound_logger(logging.DEBUG)
)
logger = structlog.get_logger(__name__)

def main():
    try:
        # server = Server(Server.State.ENV_MODEL)
        # server = Server(Server.State.WORKING)
        server = Server(Server.State.MONITOR)
        server.start_mqtt()
        # server.start_serial()
        server.start()
    except KeyboardInterrupt as error:
        logger.error("ctrl+C pressed", error=error)
        sys.exit(1)

if __name__ == '__main__':
    main()