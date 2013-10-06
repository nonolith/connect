#include <QApplication>
#include <QtWebKit/QWebInspector>
#include <QtWebKit/QGraphicsWebView>
#include <QtWebKit/QWebFrame>
#include "html5applicationviewer/html5applicationviewer.h"
#include "ui.h"

int ui_main(int argc, char* argv[]) {
	QApplication app(argc, argv);

	Html5ApplicationViewer viewer;
	viewer.setOrientation(Html5ApplicationViewer::ScreenOrientationAuto);
	viewer.showExpanded();
	viewer.setMinimumSize(800, 600);

	Bridge bridge(viewer);
	viewer.webView()->page()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);

	QWebInspector inspector;
	inspector.setPage(viewer.webView()->page());
	//inspector.setVisible(true);

	viewer.loadUrl(QUrl(QLatin1String("http://localhost:3000/pixelpulse.html")));

	return app.exec();
}

Bridge::Bridge(Html5ApplicationViewer& _viewer): viewer(_viewer) {
	QObject::connect(viewer.webView()->page()->mainFrame(), SIGNAL(javaScriptWindowObjectCleared()),
	                     this, SLOT(addToPage()));
}

void Bridge::addToPage() {
	viewer.webView()->page()->mainFrame()->addToJavaScriptWindowObject("nonolith_connect", this);
	connection = boost::shared_ptr<BridgeClientConn>(new BridgeClientConn(*this));
}

void Bridge::send(QString msg) {
	connection->handleMessageToServer(msg.toStdString());
}

void Bridge::connect() {
	connection->handleConnect();
}
