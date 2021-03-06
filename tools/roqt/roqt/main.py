import sys
import os
import argparse
from PySide import QtGui, QtSvg
from roqt import Wafer, WaferView


class MainWindow(QtGui.QMainWindow):
    def __init__(self, scene):
        super(MainWindow, self).__init__()
        self.scene = scene
        self.view = WaferView(scene)
        self.setCentralWidget(self.view)
        self.setWindowTitle("Routing Qt Visualizer")


def invoke_painter(scene, img):
    painter = QtGui.QPainter(img)
    scene.render(painter)
    painter.end()


def save_figure(scene, fname):
    suffix = os.path.splitext(fname)[-1]
    if suffix == '.svg':
        img = QtSvg.QSvgGenerator()
        img.setFileName(fname)
        invoke_painter(scene, img)
    else:
        img = QtGui.QImage(3000, 3000, QtGui.QImage.Format_ARGB32)
        invoke_painter(scene, img)
        img.save(fname)


def valid_file(arg):
    if not os.path.isfile(arg):
        raise argparse.ArgumentTypeError(
            "{!r} does not exist".format(arg))
    return arg


def main():
    app = QtGui.QApplication(sys.argv)

    parser = argparse.ArgumentParser()
    parser.add_argument('file', type=valid_file, help='marocco results file')
    parser.add_argument('-o', type=str, default=None,
            help='write rendering to file, e.g.: *.png, *.svg')
    parser.add_argument('--switches', action='store_true',
                        help='draw switches (slows down rendering)')
    parser.add_argument('--l1routes', action='store_true',
                        help='load pickled vector of l1routes from file')
    args = parser.parse_args()

    scene = QtGui.QGraphicsScene()

    wafer = Wafer(scene, args.switches)

    if args.l1routes:
        import pickle
        with open(args.file) as f:
            obj = pickle.load(f)
        if not isinstance(obj, list):
            obj = [obj]
        wafer.draw_routes(obj)
    else:
        from pymarocco.results import Marocco
        wafer.draw(Marocco.from_file(args.file))

    if args.o:
        save_figure(scene, args.o)
    else:
        window = MainWindow(scene)
        window.show()

        sys.exit(app.exec_())


if __name__ == '__main__':
    main()
