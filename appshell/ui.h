#include <QObject>
#include "html5applicationviewer/html5applicationviewer.h"
#include "dataserver.hpp"

class BridgeClientConn;

class Bridge : public QObject {
     Q_OBJECT

 public:
 	Bridge(Html5ApplicationViewer& _viewer);
    Q_INVOKABLE void send(QString msg);
    Q_INVOKABLE void connect();

signals:
	Q_INVOKABLE void onMessage(QString msg);

 public slots:
 	void addToPage();

 private:
 	boost::shared_ptr<BridgeClientConn> connection;
 	Html5ApplicationViewer& viewer;
 };

 class BridgeClientConn: public ClientConn {
 public:
 	BridgeClientConn(Bridge& b);

 	// On server thread
 	void connect();
 	void sendJSON(JSONNode &n);
 	void on_message(const std::string &msg);
 	void on_device_list_changed();

 	// On Qt thread
 	void handleMessageToServer(std::string);
 	void handleConnect();

 private:
 	Bridge& bridge;
 	EventListener l_device_list_changed;
 };