from station.station import StationController as Station
import structlog
import sys


logger = structlog.get_logger(__name__)

def main():
    try:
        Station(Station.State.ENV_MODEL)
    except KeyboardInterrupt as error:
        logger.error("ctrl+C pressed", error=error)
        sys.exit(1)

if __name__ == '__main__':
    main()