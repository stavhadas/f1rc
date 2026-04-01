import IPython
from controller_cnc import ControllerCNC

if __name__ == "__main__":
    controller = ControllerCNC()
    controller.start()
    IPython.embed()
