// Renders a web page in WebKit at phone size and saves PNG snapshots at given times.
//   phone_snapshot <url> <out-prefix> <seconds> [<seconds> ...]
// Writes <out-prefix>_0.png, _1.png, ... (used by the UI tests for the player screens).
// JavaScript errors from the page are printed to stderr; with SNAP_EVAL set, that
// expression is evaluated at each snapshot and its result printed too.
import Cocoa
import WebKit

let args = CommandLine.arguments
guard args.count >= 4, let url = URL(string: args[1]) else {
    FileHandle.standardError.write("usage: phone_snapshot <url> <out-prefix> <seconds>...\n".data(using: .utf8)!)
    exit(2)
}
let prefix = args[2]
let times = args.dropFirst(3).compactMap { Double($0) }

let app = NSApplication.shared
app.setActivationPolicy(.prohibited)
let frame = NSRect(x: 0, y: 0, width: 390, height: 844)  // an iPhone-sized viewport
let window = NSWindow(contentRect: frame, styleMask: [.borderless], backing: .buffered, defer: false)
// On screen (bottom-left corner) for the few seconds it runs: WebKit pauses CSS
// transitions and animations in windows that can't be seen.
window.setFrameOrigin(NSPoint(x: 0, y: 0))
window.level = .floating
// Report the page's JavaScript errors.
final class ErrorSink: NSObject, WKScriptMessageHandler {
    func userContentController(_ c: WKUserContentController, didReceive message: WKScriptMessage) {
        FileHandle.standardError.write("page error: \(message.body)\n".data(using: .utf8)!)
    }
}
let config = WKWebViewConfiguration()
let sink = ErrorSink()
config.userContentController.add(sink, name: "err")
config.userContentController.addUserScript(WKUserScript(
    source: "window.addEventListener('error',e=>webkit.messageHandlers.err.postMessage(e.message+' @'+e.lineno));" +
            "window.addEventListener('unhandledrejection',e=>webkit.messageHandlers.err.postMessage('rejection: '+e.reason));",
    injectionTime: .atDocumentStart, forMainFrameOnly: true))
let web = WKWebView(frame: frame, configuration: config)
window.contentView = web
window.orderFront(nil)
web.load(URLRequest(url: url))

var remaining = times.count
for (i, t) in times.enumerated() {
    DispatchQueue.main.asyncAfter(deadline: .now() + t) {
        if let expr = ProcessInfo.processInfo.environment["SNAP_EVAL"] {
            web.evaluateJavaScript(expr) { result, error in
                print("t=\(t): \(result.map { "\($0)" } ?? "nil") \(error.map { "\($0)" } ?? "")")
            }
        }
        web.takeSnapshot(with: WKSnapshotConfiguration()) { image, _ in
            if let image = image, let tiff = image.tiffRepresentation, let rep = NSBitmapImageRep(data: tiff),
               let png = rep.representation(using: .png, properties: [:]) {
                try? png.write(to: URL(fileURLWithPath: "\(prefix)_\(i).png"))
            }
            remaining -= 1
            if remaining == 0 { exit(0) }
        }
    }
}
DispatchQueue.main.asyncAfter(deadline: .now() + (times.max() ?? 0) + 15) { exit(1) }
app.run()
