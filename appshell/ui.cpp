#include <QApplication>
#include <QtWebKit/QWebInspector>
#include <QtWebKit/QGraphicsWebView>
#include "html5applicationviewer/html5applicationviewer.h"

int ui_main(int argc, char* argv[]) {
	QApplication app(argc, argv);

	Html5ApplicationViewer viewer;
	viewer.setOrientation(Html5ApplicationViewer::ScreenOrientationAuto);
	viewer.showExpanded();
	viewer.setMinimumSize(800, 600);

	viewer.webView()->page()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);

	QWebInspector inspector;
	inspector.setPage(viewer.webView()->page());
	//inspector.setVisible(true);

	viewer.loadUrl(QUrl(QLatin1String("http://apps.nonolithlabs.com/pixelpulse")));

	return app.exec();
}