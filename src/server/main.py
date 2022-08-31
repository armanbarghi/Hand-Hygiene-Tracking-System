from server.server import ServerController as Server
import structlog
import sys


logger = structlog.get_logger(__name__)

def main():
    try:
        Server(Server.State.WORKING)
    except KeyboardInterrupt as error:
        logger.error("ctrl+C pressed", error=error)
        sys.exit(1)

if __name__ == '__main__':
    main()