import QtQuick 2.5
import net.asivery.AppLoad 1.0

// Root QML file of the example AppLoad app (id "cpp-backend-example").
// AppLoad requires the root file to declare `signal close` and a
// `function unloading()` (see README.MD of asivery/rm-appload).
Rectangle {
    id: root
    anchors.fill: parent

    signal close

    function unloading() {
        console.log("cpp-example: unloading frontend, terminating backend");
        // Kills the backend process and unloads all remaining frontends.
        endpoint.terminate();
    }

    // Endpoint to the C++ backend. An application can have multiple
    // endpoints; all of them receive every message sent by the backend.
    AppLoad {
        id: endpoint
        applicationID: "cpp-backend-example"

        onMessageReceived: (type, contents) => {
            console.log("cpp-example: message from backend, type=" + type + " contents=" + contents);
            switch (type) {
                case 100: // welcome, sent by the backend on NEW_COORDINATOR
                case 101: // echo reply
                case 102: // time reply
                case 103: // counter reply
                case 104: // stats reply
                    replyText.text = "type " + type + ": " + contents;
                    break;
                default:
                    replyText.text = "unknown type " + type + ": " + contents;
                    break;
            }
        }
    }

    Text {
        id: title
        anchors.top: parent.top
        anchors.topMargin: 40
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: "AppLoad C++ backend demo"
        font.pointSize: 40
    }

    Column {
        id: buttons
        anchors.top: title.bottom
        anchors.topMargin: 30
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12

        Repeater {
            model: [
                { label: "Echo \"Hello\" (type 1)",    msgType: 1, payload: "Hello" },
                { label: "Ask for time (type 2)",       msgType: 2, payload: "" },
                { label: "Increment counter (type 3)",  msgType: 3, payload: "" },
                { label: "Get stats (type 4)",          msgType: 4, payload: "" }
            ]
            delegate: Rectangle {
                width: 460
                height: 90
                border.width: 2
                border.color: "black"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    font.pointSize: 22
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: () => endpoint.sendMessage(modelData.msgType, modelData.payload)
                }
            }
        }
    }

    Text {
        id: replyText
        anchors.top: buttons.bottom
        anchors.topMargin: 30
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: "No message from the backend yet"
        font.pointSize: 28
    }
}
