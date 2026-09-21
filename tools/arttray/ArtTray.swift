// ArtTray — a small Mac front end for story/packages.
//
// The loop it automates: pick the next package, copy its prompt and its reference images into
// ChatGPT, drop the generated image back here, and let `story_prompt.py ingest` cut it into the
// files the game loads.
//
// One file, AppKit, no dependencies. `ArtTray --selftest` runs the non-GUI logic and exits.

import AppKit
import Foundation

// MARK: - small helpers

struct ArtTrayError: LocalizedError {
    let msg: String
    init(_ msg: String) { self.msg = msg }
    var errorDescription: String? { msg }
}

let returnedStem = "returned"
let returnedSuffixes = ["png", "jpg", "jpeg", "webp", "heic", "gif", "tif", "tiff"]
let droppableSuffixes = ["png", "jpg", "jpeg", "webp", "heic", "tif", "tiff"]

func fileDate(_ url: URL) -> Date? {
    (try? url.resourceValues(forKeys: [.contentModificationDateKey]))?.contentModificationDate
}

// MARK: - package model

/// One generation inside a package. Most packages have exactly one; a `screen` package has three
/// (PAINT → returned.png, WALKABLE MASK → returned_walk.png, FOREGROUND MASK → returned_fg.png),
/// declared in package.json as `steps: [{title, prompt_index, return_file}]`.
struct PackageStep: Equatable {
    var title: String
    var promptIndex: Int
    var returnFile: String

    static let single = PackageStep(title: "Image", promptIndex: 0, returnFile: "returned.png")

    /// The name without its extension, so a returned.jpg still counts as that step's return.
    var stem: String { (returnFile as NSString).deletingPathExtension }
}

struct PackageMeta {
    var kind: String = ""
    var title: String = ""
    var makes: String = ""
    var dir: String = ""
    var outputs: [String] = []
    var handle: String?
    var scene: String?
    var map: String?
    /// Never empty: a package with no `steps` is one step returning returned.png.
    var steps: [PackageStep] = [.single]

    /// Parse a package.json. Only the fields the tray needs; unknown fields are ignored.
    static func parse(_ data: Data) throws -> PackageMeta {
        guard let obj = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw ArtTrayError("package.json is not a JSON object")
        }
        var m = PackageMeta()
        m.kind = obj["kind"] as? String ?? ""
        m.title = obj["title"] as? String ?? ""
        m.makes = obj["makes"] as? String ?? ""
        m.dir = obj["dir"] as? String ?? ""
        m.outputs = obj["outputs"] as? [String] ?? []
        m.handle = obj["handle"] as? String
        m.scene = obj["scene"] as? String
        m.map = obj["map"] as? String
        if let raw = obj["steps"] as? [[String: Any]], !raw.isEmpty {
            m.steps = raw.enumerated().map { i, s in
                PackageStep(title: s["title"] as? String ?? "Step \(i + 1)",
                            promptIndex: s["prompt_index"] as? Int ?? i,
                            returnFile: s["return_file"] as? String ?? (i == 0 ? "returned.png" : "returned_\(i + 1).png"))
            }
        }
        if m.kind.isEmpty { throw ArtTrayError("package.json has no \"kind\"") }
        return m
    }
}

enum PackageStatus: Equatable {
    case toGenerate
    case awaitingSteps(Int, Int)     // some of a multi-step package's images are back, not all
    case returnedNotCut
    case partial(Int, Int)
    case done

    var label: String {
        switch self {
        case .toGenerate: return "○ to generate"
        case .awaitingSteps(let have, let total): return "◔ \(have)/\(total) returned"
        case .returnedNotCut: return "▲ returned, not cut"
        case .partial(let have, let total): return "◒ \(have)/\(total) cut"
        case .done: return "● done"
        }
    }

    var isDone: Bool { self == .done }
}

/// The whole status rule, with no file system in it, so it can be tested directly.
/// `outputDates` is one entry per file the package makes: nil when that file does not exist.
/// `returnsHave`/`returnsTotal` count the package's steps: a package is only ready to cut when
/// every step's image is back.
func statusFrom(returned: Date?, returnsHave: Int = 1, returnsTotal: Int = 1,
                outputDates: [Date?]) -> PackageStatus {
    let have = outputDates.compactMap { $0 }
    if returnsHave > 0 && returnsHave < returnsTotal {
        let cut = !outputDates.isEmpty && have.count == outputDates.count
            && !have.contains { out in returned.map { out < $0 } ?? false }
        if !cut { return .awaitingSteps(returnsHave, returnsTotal) }
    }
    if let ret = returned {
        let stale = have.count < outputDates.count || have.contains { $0 < ret }
        if stale { return .returnedNotCut }
    }
    if !outputDates.isEmpty && have.count == outputDates.count { return .done }
    if !have.isEmpty { return .partial(have.count, outputDates.count) }
    return .toGenerate
}

struct PromptDoc {
    var attach: [String] = []
    /// Every fenced prompt in the file, in order. A step names its own by index.
    var prompts: [String] = []
    var checklist: String = ""

    /// The first prompt, which is the whole prompt for a one-step package.
    var prompt: String { prompts.first ?? "" }

    func prompt(at index: Int) -> String {
        index >= 0 && index < prompts.count ? prompts[index] : prompt
    }

    /// prompt.md is generated, so its shape is fixed:
    ///   "## 1. …attach these files…"   a numbered list of `backticked/paths`
    ///   "## 2. Paste this prompt exactly"  one fenced block (four backticks)
    ///   "## 3. Afterwards"             the checklist and the save-it-here instructions
    static func parse(_ text: String) -> PromptDoc {
        var doc = PromptDoc()
        let lines = text.components(separatedBy: .newlines)
        var section = 0
        var fenceMarker: String?
        var promptLines: [String] = []
        var checklistLines: [String] = []

        func isFence(_ line: String) -> Bool {
            let t = line.trimmingCharacters(in: .whitespaces)
            return t.count >= 3 && t.allSatisfy { $0 == "`" }
        }

        for line in lines {
            // A heading ends a section — but never in the middle of a fenced block, where a "## "
            // line is prompt text.
            if line.hasPrefix("## ") && fenceMarker == nil {
                let head = line.dropFirst(3)
                let lower = head.lowercased()
                if head.hasPrefix("1.") { section = 1 }
                else if head.hasPrefix("2") || lower.contains("paste this prompt") { section = 2 }
                else if head.hasPrefix("3.") { section = 3 }
                else { section = 0 }
                continue
            }
            switch section {
            case 1:
                // "1. `story/refs/style.png`"
                if let path = backtickedPath(in: line) { doc.attach.append(path) }
            case 2:
                if let marker = fenceMarker {
                    if isFence(line) && line.trimmingCharacters(in: .whitespaces).count >= marker.count {
                        fenceMarker = nil
                        doc.prompts.append(promptLines.joined(separator: "\n")
                            .trimmingCharacters(in: .whitespacesAndNewlines))
                        promptLines = []
                    } else {
                        promptLines.append(line)
                    }
                } else if isFence(line) {
                    fenceMarker = line.trimmingCharacters(in: .whitespaces)
                }
            case 3:
                checklistLines.append(line)
            default:
                break
            }
        }
        if fenceMarker != nil && !promptLines.isEmpty {       // an unterminated fence at the end
            doc.prompts.append(promptLines.joined(separator: "\n")
                .trimmingCharacters(in: .whitespacesAndNewlines))
        }
        doc.prompts = doc.prompts.filter { !$0.isEmpty }
        doc.checklist = checklistLines.joined(separator: "\n").trimmingCharacters(in: .whitespacesAndNewlines)
        return doc
    }

    private static func backtickedPath(in line: String) -> String? {
        let t = line.trimmingCharacters(in: .whitespaces)
        guard let dot = t.firstIndex(of: "."), Int(t[t.startIndex..<dot]) != nil else { return nil }
        guard let open = t.firstIndex(of: "`") else { return nil }
        let rest = t[t.index(after: open)...]
        guard let close = rest.firstIndex(of: "`") else { return nil }
        let path = String(rest[rest.startIndex..<close])
        return path.isEmpty ? nil : path
    }
}

final class Package {
    let relPath: String              // "ch01/halm/props", relative to story/packages
    let dir: URL
    let meta: PackageMeta
    var step: Int?                   // its number in the README's short path, if it is on it
    var order: Int = 0               // position in the queue
    var status: PackageStatus = .toGenerate

    init(relPath: String, dir: URL, meta: PackageMeta) {
        self.relPath = relPath
        self.dir = dir
        self.meta = meta
    }

    var title: String { meta.title.isEmpty ? relPath : meta.title }

    var steps: [PackageStep] { meta.steps }

    /// The image the owner saved for one step, whatever extension it came back with.
    func returnedImage(step: PackageStep, _ fm: FileManager = .default) -> URL? {
        guard let items = try? fm.contentsOfDirectory(at: dir, includingPropertiesForKeys: [.contentModificationDateKey]) else {
            return nil
        }
        let hits = items.filter {
            $0.deletingPathExtension().lastPathComponent == step.stem
                && returnedSuffixes.contains($0.pathExtension.lowercased())
        }
        return hits.max { (fileDate($0) ?? .distantPast) < (fileDate($1) ?? .distantPast) }
    }

    /// The image the owner saved here, for the first step (the whole package when there is one step).
    func returnedImage(_ fm: FileManager = .default) -> URL? {
        returnedImage(step: steps.first ?? .single, fm)
    }

    /// One entry per step: the image waiting for it, or nil.
    func returnedImages() -> [URL?] { steps.map { returnedImage(step: $0) } }

    var allStepsReturned: Bool { returnedImages().allSatisfy { $0 != nil } }

    func refreshStatus(repoRoot: URL) {
        let outs = meta.outputs.map { repoRoot.appendingPathComponent($0) }
        let returns = returnedImages().compactMap { $0 }.compactMap { fileDate($0) }
        status = statusFrom(returned: returns.max(),
                            returnsHave: returns.count, returnsTotal: steps.count,
                            outputDates: outs.map { fileDate($0) })
    }

    func promptDoc() -> PromptDoc? {
        guard let text = try? String(contentsOf: dir.appendingPathComponent("prompt.md"), encoding: .utf8) else {
            return nil
        }
        return PromptDoc.parse(text)
    }
}

// MARK: - the repo

final class Repo {
    let root: URL
    init(root: URL) { self.root = root }

    var packagesDir: URL { root.appendingPathComponent("story/packages") }
    var readme: URL { packagesDir.appendingPathComponent("README.md") }
    var storyPrompt: URL { root.appendingPathComponent("story_prompt.py") }
    var fastReload: URL { root.appendingPathComponent("fast_reload.sh") }

    static func looksLikeRepo(_ url: URL) -> Bool {
        FileManager.default.fileExists(atPath: url.appendingPathComponent("story_prompt.py").path)
    }

    /// Walk up from `start` looking for the directory that holds story_prompt.py.
    static func findUpwards(from start: URL) -> URL? {
        var dir = start.standardizedFileURL
        for _ in 0..<12 {
            if looksLikeRepo(dir) { return dir }
            let parent = dir.deletingLastPathComponent()
            if parent.path == dir.path { break }
            dir = parent
        }
        return nil
    }

    static func discover(arguments: [String] = CommandLine.arguments,
                         defaults: UserDefaults? = .standard) -> URL? {
        if let i = arguments.firstIndex(of: "--repo"), i + 1 < arguments.count {
            let url = URL(fileURLWithPath: (arguments[i + 1] as NSString).expandingTildeInPath)
            if looksLikeRepo(url) {
                defaults?.set(url.standardizedFileURL.path, forKey: "repoPath")   // remembered
                return url
            }
        }
        if let saved = defaults?.string(forKey: "repoPath") {
            let url = URL(fileURLWithPath: saved)
            if looksLikeRepo(url) { return url }
        }
        for start in [Bundle.main.bundleURL, Bundle.main.executableURL?.deletingLastPathComponent(),
                      URL(fileURLWithPath: FileManager.default.currentDirectoryPath)] {
            if let s = start, let hit = findUpwards(from: s) { return hit }
        }
        return nil
    }

    /// The queue: the README's order first (it is the source of truth), then anything it missed.
    func scanPackages() -> [Package] {
        let fm = FileManager.default
        var found: [String: Package] = [:]
        if let walker = fm.enumerator(at: packagesDir, includingPropertiesForKeys: nil) {
            for case let url as URL in walker where url.lastPathComponent == "package.json" {
                let dir = url.deletingLastPathComponent()
                guard let data = try? Data(contentsOf: url),
                      let meta = try? PackageMeta.parse(data) else { continue }
                let rel = relative(dir)
                found[rel] = Package(relPath: rel, dir: dir, meta: meta)
            }
        }
        var queue: [Package] = []
        var seen = Set<String>()
        for (rel, step) in Repo.readmeOrder(text: (try? String(contentsOf: readme, encoding: .utf8)) ?? "") {
            guard let pkg = found[rel], !seen.contains(rel) else { continue }
            pkg.step = step
            queue.append(pkg)
            seen.insert(rel)
        }
        for rel in found.keys.sorted() where !seen.contains(rel) {
            queue.append(found[rel]!)
        }
        for (i, pkg) in queue.enumerated() {
            pkg.order = i + 1
            pkg.refreshStatus(repoRoot: root)
        }
        return queue
    }

    private func relative(_ dir: URL) -> String {
        let base = packagesDir.standardizedFileURL.path
        let path = dir.standardizedFileURL.path
        if path.hasPrefix(base + "/") { return String(path.dropFirst(base.count + 1)) }
        return dir.lastPathComponent
    }

    /// Every `](<path>/prompt.md)` link in README.md, in order, de-duplicated. The number is the
    /// short path's step, taken from the nearest preceding `**N.**`, or nil once past that section.
    static func readmeOrder(text: String) -> [(String, Int?)] {
        var out: [(String, Int?)] = []
        var seen = Set<String>()
        var pendingStep: Int?
        var inShortPath = false

        for line in text.components(separatedBy: .newlines) {
            if line.hasPrefix("## ") {
                inShortPath = line.lowercased().contains("short path")
                pendingStep = nil
            }
            if inShortPath, let n = boldNumber(in: line) { pendingStep = n }
            for path in promptLinks(in: line) {
                let rel = String(path.dropLast("/prompt.md".count))
                if seen.contains(rel) { continue }
                seen.insert(rel)
                out.append((rel, inShortPath ? pendingStep : nil))
                pendingStep = nil
            }
        }
        return out
    }

    private static func boldNumber(in line: String) -> Int? {
        // "- [ ] **7.** Falke's walk sprite…"
        guard let open = line.range(of: "**") else { return nil }
        let rest = line[open.upperBound...]
        guard let close = rest.range(of: "**") else { return nil }
        let inner = rest[rest.startIndex..<close.lowerBound].trimmingCharacters(in: CharacterSet(charactersIn: ". "))
        return Int(inner)
    }

    private static func promptLinks(in line: String) -> [String] {
        var out: [String] = []
        var rest = Substring(line)
        while let open = rest.range(of: "](") {
            let after = rest[open.upperBound...]
            guard let close = after.firstIndex(of: ")") else { break }
            let target = String(after[after.startIndex..<close])
            if target.hasSuffix("/prompt.md") { out.append(target) }
            rest = after[after.index(after: close)...]
        }
        return out
    }
}

// MARK: - images

enum Img {
    /// PNG bytes for an image file, converting when it is not already a PNG.
    static func pngData(fromFile url: URL) throws -> Data {
        let data = try Data(contentsOf: url)
        if url.pathExtension.lowercased() == "png", data.starts(with: [0x89, 0x50, 0x4E, 0x47]) {
            return data                                     // keep the original bytes untouched
        }
        return try pngData(fromImageData: data)
    }

    static func pngData(fromImageData data: Data) throws -> Data {
        if let rep = NSBitmapImageRep(data: data),
           let png = rep.representation(using: .png, properties: [:]) { return png }
        if let image = NSImage(data: data), let tiff = image.tiffRepresentation,
           let rep = NSBitmapImageRep(data: tiff),
           let png = rep.representation(using: .png, properties: [:]) { return png }
        throw ArtTrayError("that data is not an image this Mac can read")
    }

    static func tiffData(fromPNG png: Data) -> Data? {
        NSBitmapImageRep(data: png)?.tiffRepresentation ?? NSImage(data: png)?.tiffRepresentation
    }

    static func copyToPasteboard(pngFile url: URL) throws {
        let png = try pngData(fromFile: url)
        let pb = NSPasteboard.general
        pb.clearContents()
        pb.declareTypes([.png, .tiff], owner: nil)
        pb.setData(png, forType: .png)
        if let tiff = tiffData(fromPNG: png) { pb.setData(tiff, forType: .tiff) }
    }

    /// PNG bytes sitting on the clipboard right now, from image data or from a copied image file.
    static func pngFromPasteboard() throws -> Data {
        let pb = NSPasteboard.general
        if let d = pb.data(forType: .png) { return try pngData(fromImageData: d) }
        if let d = pb.data(forType: .tiff) { return try pngData(fromImageData: d) }
        if let urls = pb.readObjects(forClasses: [NSURL.self],
                                     options: [.urlReadingFileURLsOnly: true]) as? [URL],
           let hit = urls.first(where: { droppableSuffixes.contains($0.pathExtension.lowercased()) }) {
            return try pngData(fromFile: hit)
        }
        throw ArtTrayError("the clipboard holds no image — copy the picture itself, not a link")
    }

    static func thumbnail(_ url: URL, side: CGFloat) -> NSImage? {
        guard let image = NSImage(contentsOf: url) else { return nil }
        image.size = fit(image.size, side: side)
        return image
    }

    private static func fit(_ size: NSSize, side: CGFloat) -> NSSize {
        guard size.width > 0, size.height > 0 else { return NSSize(width: side, height: side) }
        let scale = min(side / size.width, side / size.height, 1)
        return NSSize(width: size.width * scale, height: size.height * scale)
    }

    /// Write PNG bytes as <package>/<name>, keeping any previous one as <stem>_prev.png.
    @discardableResult
    static func writeReturned(_ png: Data, into dir: URL, named name: String = "returned.png") throws -> URL {
        let target = dir.appendingPathComponent(name)
        let stem = (name as NSString).deletingPathExtension
        let fm = FileManager.default
        if fm.fileExists(atPath: target.path) {
            let prev = dir.appendingPathComponent("\(stem)_prev.png")
            if fm.fileExists(atPath: prev.path) { try? fm.removeItem(at: prev) }
            try? fm.copyItem(at: target, to: prev)
        }
        try png.write(to: target, options: .atomic)
        return target
    }

    /// Newest png/jpg/webp in ~/Downloads modified within `within` seconds.
    static func newestDownload(within: TimeInterval = 3600,
                               folder: URL = FileManager.default.homeDirectoryForCurrentUser
                                   .appendingPathComponent("Downloads")) -> URL? {
        let fm = FileManager.default
        guard let items = try? fm.contentsOfDirectory(at: folder,
                                                      includingPropertiesForKeys: [.contentModificationDateKey],
                                                      options: [.skipsHiddenFiles]) else { return nil }
        let cutoff = Date().addingTimeInterval(-within)
        let hits = items.filter {
            droppableSuffixes.contains($0.pathExtension.lowercased()) && (fileDate($0) ?? .distantPast) > cutoff
        }
        return hits.max { (fileDate($0) ?? .distantPast) < (fileDate($1) ?? .distantPast) }
    }
}

// MARK: - running the repo's own tools

final class Runner {
    private let queue = DispatchQueue(label: "arttray.runner")
    private(set) var busy = false

    /// Run a script in the repo with the repo as cwd, streaming both streams into `log`.
    func run(script: URL, args: [String], cwd: URL,
             log: @escaping (String) -> Void,
             done: @escaping (Int32, String?) -> Void) {
        guard !busy else {
            done(-1, "another command is still running")
            return
        }
        busy = true
        queue.async {
            let process = Process()
            let fm = FileManager.default
            if fm.isExecutableFile(atPath: script.path) {
                process.executableURL = script
                process.arguments = args
            } else if script.pathExtension == "py" {
                process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
                process.arguments = ["python3", script.path] + args
            } else {
                process.executableURL = URL(fileURLWithPath: "/bin/sh")
                process.arguments = [script.path] + args
            }
            process.currentDirectoryURL = cwd
            var env = ProcessInfo.processInfo.environment
            env["PYTHONUNBUFFERED"] = "1"
            // A GUI app inherits a thin PATH; the repo's scripts want the usual one.
            env["PATH"] = (env["PATH"].map { $0 + ":" } ?? "") + "/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
            process.environment = env

            let out = Pipe(), err = Pipe()
            process.standardOutput = out
            process.standardError = err
            let shown = "$ " + ([script.lastPathComponent] + args).joined(separator: " ")
            DispatchQueue.main.async { log(shown) }

            func pump(_ pipe: Pipe, prefix: String) {
                pipe.fileHandleForReading.readabilityHandler = { handle in
                    let data = handle.availableData
                    guard !data.isEmpty, let text = String(data: data, encoding: .utf8) else { return }
                    DispatchQueue.main.async { log(prefix + text.trimmingCharacters(in: .newlines)) }
                }
            }
            pump(out, prefix: "")
            pump(err, prefix: "")

            do {
                try process.run()
            } catch {
                out.fileHandleForReading.readabilityHandler = nil
                err.fileHandleForReading.readabilityHandler = nil
                self.busy = false
                DispatchQueue.main.async { done(-1, "could not start \(script.lastPathComponent): \(error.localizedDescription)") }
                return
            }
            process.waitUntilExit()
            // Drain whatever arrived between the last handler call and exit.
            out.fileHandleForReading.readabilityHandler = nil
            err.fileHandleForReading.readabilityHandler = nil
            for pipe in [out, err] {
                if let rest = try? pipe.fileHandleForReading.readToEnd(),
                   let text = String(data: rest, encoding: .utf8),
                   !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                    DispatchQueue.main.async { log(text.trimmingCharacters(in: .newlines)) }
                }
            }
            let code = process.terminationStatus
            self.busy = false
            DispatchQueue.main.async { done(code, nil) }
        }
    }
}

// MARK: - little view helpers

final class ActionButton: NSButton {
    private var handler: (() -> Void)?

    convenience init(title: String, handler: @escaping () -> Void) {
        self.init(frame: .zero)
        self.title = title
        self.bezelStyle = .rounded
        self.handler = handler
        self.target = self
        self.action = #selector(fire)
        self.setContentHuggingPriority(.defaultHigh, for: .horizontal)
    }

    @objc private func fire() { handler?() }
}

enum Dropped {
    case file(URL)
    case data(Data)
}

final class DropZone: NSView {
    var onDrop: ((Dropped) -> Void)?
    private var hot = false
    private let label = NSTextField(labelWithString: "")

    override init(frame: NSRect) {
        super.init(frame: frame)
        wantsLayer = true
        registerForDraggedTypes([.fileURL, .png, .tiff])
        label.translatesAutoresizingMaskIntoConstraints = false
        label.alignment = .center
        label.textColor = .secondaryLabelColor
        label.font = .systemFont(ofSize: 12)
        label.stringValue = "Drop the generated image here\n(or use the buttons below)"
        label.maximumNumberOfLines = 2
        addSubview(label)
        NSLayoutConstraint.activate([
            label.centerXAnchor.constraint(equalTo: centerXAnchor),
            label.centerYAnchor.constraint(equalTo: centerYAnchor),
        ])
    }

    required init?(coder: NSCoder) { nil }

    override func draw(_ dirtyRect: NSRect) {
        let path = NSBezierPath(roundedRect: bounds.insetBy(dx: 2, dy: 2), xRadius: 8, yRadius: 8)
        (hot ? NSColor.controlAccentColor.withAlphaComponent(0.15) : NSColor.textBackgroundColor).setFill()
        path.fill()
        path.lineWidth = hot ? 2.5 : 1.5
        path.setLineDash([6, 4], count: 2, phase: 0)
        (hot ? NSColor.controlAccentColor : NSColor.separatorColor).setStroke()
        path.stroke()
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        hot = true; needsDisplay = true
        return .copy
    }

    override func draggingExited(_ sender: NSDraggingInfo?) {
        hot = false; needsDisplay = true
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        hot = false; needsDisplay = true
        let pb = sender.draggingPasteboard
        if let urls = pb.readObjects(forClasses: [NSURL.self],
                                     options: [.urlReadingFileURLsOnly: true]) as? [URL],
           let hit = urls.first(where: { droppableSuffixes.contains($0.pathExtension.lowercased()) }) {
            onDrop?(.file(hit))
            return true
        }
        if let data = pb.data(forType: .png) ?? pb.data(forType: .tiff) {
            onDrop?(.data(data))
            return true
        }
        return false
    }
}

func sectionLabel(_ text: String) -> NSTextField {
    let field = NSTextField(labelWithString: text)
    field.font = .systemFont(ofSize: 11, weight: .semibold)
    field.textColor = .secondaryLabelColor
    return field
}

/// A scroll view that keeps its text view exactly as wide as itself, so the text rewraps when the
/// window is resized instead of staying at whatever width it was first given.
final class TextScrollView: NSScrollView {
    override func layout() {
        super.layout()
        if let text = documentView as? NSTextView, text.frame.width != contentSize.width {
            text.frame.size.width = contentSize.width
            text.textContainer?.containerSize = NSSize(width: contentSize.width,
                                                       height: CGFloat.greatestFiniteMagnitude)
        }
    }
}

/// `constrained: false` is for a view an NSSplitView frames itself — a split view positions its
/// arranged subviews by frame, and a translated-off, height-pinned subview collapses to nothing.
func readOnlyTextView(height: CGFloat, monospaced: Bool, constrained: Bool = true) -> (NSScrollView, NSTextView) {
    let scroll = TextScrollView(frame: NSRect(x: 0, y: 0, width: 600, height: height))
    if constrained {
        scroll.translatesAutoresizingMaskIntoConstraints = false
        scroll.heightAnchor.constraint(equalToConstant: height).isActive = true
    }
    scroll.hasVerticalScroller = true
    scroll.borderType = .bezelBorder
    let text = NSTextView()
    text.isEditable = false
    text.isSelectable = true
    text.isRichText = false
    text.drawsBackground = true
    text.font = monospaced ? .monospacedSystemFont(ofSize: 11, weight: .regular) : .systemFont(ofSize: 12)
    text.textContainerInset = NSSize(width: 6, height: 6)
    text.autoresizingMask = [.width]
    text.minSize = NSSize(width: 0, height: 0)
    text.maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
    text.isVerticallyResizable = true
    text.isHorizontallyResizable = false
    text.textContainer?.widthTracksTextView = true
    scroll.documentView = text
    return (scroll, text)
}

// MARK: - the window

final class TrayController: NSObject, NSWindowDelegate, NSTableViewDataSource, NSTableViewDelegate,
                            NSSplitViewDelegate {
    var repo: Repo?
    private var packages: [Package] = []
    private var shown: [Package] = []
    private var selected: Package?
    private var doc: PromptDoc?
    private var stepIndex = 0
    private let runner = Runner()

    let window: NSWindow
    private let table = NSTableView()
    private let hideDone = NSButton(checkboxWithTitle: "Hide done", target: nil, action: nil)
    private let repoLabel = NSTextField(labelWithString: "")
    private let statusLine = NSTextField(labelWithString: "")
    private let detailStack = NSStackView()
    private var promptView: NSTextView!
    private var logView: NSTextView!
    private var topSplit: NSSplitView!
    private var mainSplit: NSSplitView!
    private let busyIndicator = NSProgressIndicator()

    override init() {
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1240, height: 840),
                          styleMask: [.titled, .closable, .miniaturizable, .resizable],
                          backing: .buffered, defer: false)
        super.init()
        window.title = "Art Tray"
        window.delegate = self
        window.setFrameAutosaveName("ArtTrayWindow")
        buildUI()
    }

    // MARK: building

    private func buildUI() {
        let content = NSView()
        content.translatesAutoresizingMaskIntoConstraints = false

        // Toolbar row.
        let refresh = ActionButton(title: "Refresh") { [weak self] in self?.reload() }
        let regen = ActionButton(title: "Regenerate packages") { [weak self] in self?.regeneratePackages() }
        let reload = ActionButton(title: "Hot reload") { [weak self] in self?.hotReload() }
        let choose = ActionButton(title: "Choose repo…") { [weak self] in self?.chooseRepo() }
        hideDone.target = self
        hideDone.action = #selector(filterChanged)
        busyIndicator.style = .spinning
        busyIndicator.controlSize = .small
        busyIndicator.isDisplayedWhenStopped = false
        busyIndicator.translatesAutoresizingMaskIntoConstraints = false
        repoLabel.font = .systemFont(ofSize: 11)
        repoLabel.textColor = .secondaryLabelColor
        repoLabel.lineBreakMode = .byTruncatingHead

        let bar = NSStackView(views: [refresh, hideDone, regen, reload, choose, busyIndicator, repoLabel])
        bar.orientation = .horizontal
        bar.spacing = 8
        bar.alignment = .centerY
        bar.translatesAutoresizingMaskIntoConstraints = false
        repoLabel.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)

        statusLine.font = .systemFont(ofSize: 11)
        statusLine.translatesAutoresizingMaskIntoConstraints = false
        statusLine.lineBreakMode = .byTruncatingTail

        // Left: the queue. (Split-view children are framed by the split view, so they keep
        // translatesAutoresizingMaskIntoConstraints on.)
        let tableScroll = NSScrollView(frame: NSRect(x: 0, y: 0, width: 440, height: 600))
        tableScroll.hasVerticalScroller = true
        tableScroll.borderType = .bezelBorder
        table.usesAlternatingRowBackgroundColors = true
        table.allowsMultipleSelection = false
        table.rowHeight = 20
        table.dataSource = self
        table.delegate = self
        table.columnAutoresizingStyle = .uniformColumnAutoresizingStyle
        for (id, title, width) in [("step", "step / kind", 110.0), ("path", "package", 230.0),
                                   ("status", "status", 140.0)] {
            let col = NSTableColumn(identifier: NSUserInterfaceItemIdentifier(id))
            col.title = title
            col.width = width
            col.minWidth = 60
            table.addTableColumn(col)
        }
        tableScroll.documentView = table

        // Right: the selected package.
        let detailScroll = NSScrollView(frame: NSRect(x: 0, y: 0, width: 760, height: 600))
        detailScroll.hasVerticalScroller = true
        detailScroll.drawsBackground = false
        detailStack.orientation = .vertical
        detailStack.alignment = .leading
        detailStack.spacing = 10
        detailStack.edgeInsets = NSEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)
        detailStack.translatesAutoresizingMaskIntoConstraints = false
        detailScroll.documentView = detailStack
        NSLayoutConstraint.activate([
            detailStack.leadingAnchor.constraint(equalTo: detailScroll.contentView.leadingAnchor),
            detailStack.trailingAnchor.constraint(equalTo: detailScroll.contentView.trailingAnchor),
            detailStack.topAnchor.constraint(equalTo: detailScroll.contentView.topAnchor),
        ])

        let topSplit = NSSplitView(frame: NSRect(x: 0, y: 0, width: 1200, height: 600))
        topSplit.isVertical = true
        topSplit.dividerStyle = .thin
        topSplit.autoresizingMask = [.width, .height]
        topSplit.addArrangedSubview(tableScroll)
        topSplit.addArrangedSubview(detailScroll)

        // Bottom: the log.
        let (logScroll, logText) = readOnlyTextView(height: 170, monospaced: true, constrained: false)
        logView = logText

        let mainSplit = NSSplitView(frame: NSRect(x: 0, y: 0, width: 1200, height: 780))
        mainSplit.isVertical = false
        mainSplit.dividerStyle = .thin
        mainSplit.translatesAutoresizingMaskIntoConstraints = false
        mainSplit.addArrangedSubview(topSplit)
        mainSplit.addArrangedSubview(logScroll)
        self.topSplit = topSplit
        self.mainSplit = mainSplit
        topSplit.delegate = self
        mainSplit.delegate = self

        content.addSubview(bar)
        content.addSubview(statusLine)
        content.addSubview(mainSplit)
        NSLayoutConstraint.activate([
            bar.topAnchor.constraint(equalTo: content.topAnchor, constant: 10),
            bar.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 12),
            bar.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -12),
            statusLine.topAnchor.constraint(equalTo: bar.bottomAnchor, constant: 6),
            statusLine.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 12),
            statusLine.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -12),
            mainSplit.topAnchor.constraint(equalTo: statusLine.bottomAnchor, constant: 8),
            mainSplit.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 12),
            mainSplit.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -12),
            mainSplit.bottomAnchor.constraint(equalTo: content.bottomAnchor, constant: -12),
        ])
        window.contentView = content
        content.layoutSubtreeIfNeeded()
        mainSplit.setPosition(mainSplit.bounds.height - 180, ofDividerAt: 0)
        topSplit.setPosition(510, ofDividerAt: 0)      // wide enough for all three columns
        window.center()
    }

    /// Keep the queue readable when the window is resized.
    func splitView(_ splitView: NSSplitView, constrainMinCoordinate proposed: CGFloat,
                   ofSubviewAt index: Int) -> CGFloat {
        splitView === topSplit ? 260 : 120
    }

    func splitView(_ splitView: NSSplitView, constrainMaxCoordinate proposed: CGFloat,
                   ofSubviewAt index: Int) -> CGFloat {
        splitView === topSplit ? max(260, splitView.bounds.width - 420) : max(120, splitView.bounds.height - 80)
    }

    // MARK: data

    func reload() {
        guard let repo = repo else {
            say("No repository. Use “Choose repo…” to point at the folder holding story_prompt.py.", error: true)
            return
        }
        let previous = selected?.relPath
        packages = repo.scanPackages()
        applyFilter()
        repoLabel.stringValue = repo.root.path
        if let previous, let row = shown.firstIndex(where: { $0.relPath == previous }) {
            table.selectRowIndexes(IndexSet(integer: row), byExtendingSelection: false)
        } else if !shown.isEmpty && selected == nil {
            table.selectRowIndexes(IndexSet(integer: 0), byExtendingSelection: false)
        }
        showSelected()
        let waiting = packages.filter { $0.status == .returnedNotCut }.count
        let todo = packages.filter { !$0.status.isDone }.count
        if packages.isEmpty {
            say("No packages under story/packages. Run “Regenerate packages”.", error: true)
        } else {
            say("\(packages.count) packages · \(todo) not done · \(waiting) waiting to be cut", error: false)
        }
    }

    @objc private func filterChanged() {
        applyFilter()
        showSelected()
    }

    private func applyFilter() {
        shown = hideDone.state == .on ? packages.filter { !$0.status.isDone } : packages
        table.reloadData()
        if let sel = selected, let row = shown.firstIndex(where: { $0.relPath == sel.relPath }) {
            table.selectRowIndexes(IndexSet(integer: row), byExtendingSelection: false)
        } else if shown.isEmpty {
            selected = nil
        }
    }

    func numberOfRows(in tableView: NSTableView) -> Int { shown.count }

    private func cellText(_ pkg: Package, column: String) -> String {
        switch column {
        case "step":
            let step = pkg.step.map { "\($0)." } ?? "\(pkg.order)"
            let steps = pkg.steps.count > 1 ? " ×\(pkg.steps.count)" : ""
            return "\(step)  \(pkg.meta.kind)\(steps)"
        case "path": return pkg.relPath
        case "status": return pkg.status.label
        default: return ""
        }
    }

    // Cell-based path, kept for the selftest and for any older AppKit behaviour.
    func tableView(_ tableView: NSTableView, objectValueFor column: NSTableColumn?, row: Int) -> Any? {
        guard row < shown.count, let id = column?.identifier.rawValue else { return nil }
        return cellText(shown[row], column: id)
    }

    /// A table built in code is view-based: without this the rows draw empty.
    func tableView(_ tableView: NSTableView, viewFor column: NSTableColumn?, row: Int) -> NSView? {
        guard row < shown.count, let column = column else { return nil }
        let id = column.identifier
        let cell: NSTableCellView
        if let reused = tableView.makeView(withIdentifier: id, owner: self) as? NSTableCellView {
            cell = reused
        } else {
            cell = NSTableCellView()
            cell.identifier = id
            let field = NSTextField(labelWithString: "")
            field.translatesAutoresizingMaskIntoConstraints = false
            field.font = .systemFont(ofSize: 12)
            field.lineBreakMode = .byTruncatingMiddle
            cell.addSubview(field)
            cell.textField = field
            NSLayoutConstraint.activate([
                field.leadingAnchor.constraint(equalTo: cell.leadingAnchor, constant: 2),
                field.trailingAnchor.constraint(equalTo: cell.trailingAnchor, constant: -2),
                field.centerYAnchor.constraint(equalTo: cell.centerYAnchor),
            ])
        }
        let pkg = shown[row]
        cell.textField?.stringValue = cellText(pkg, column: id.rawValue)
        if id.rawValue == "status" {
            switch pkg.status {
            case .done: cell.textField?.textColor = .secondaryLabelColor
            case .returnedNotCut: cell.textField?.textColor = .systemOrange
            case .awaitingSteps: cell.textField?.textColor = .systemTeal
            default: cell.textField?.textColor = .labelColor
            }
        } else {
            cell.textField?.textColor = .labelColor
        }
        return cell
    }

    func tableViewSelectionDidChange(_ notification: Notification) {
        let row = table.selectedRow
        let pkg = (row >= 0 && row < shown.count) ? shown[row] : nil
        if pkg?.relPath != selected?.relPath { stepIndex = 0 }
        selected = pkg
        showSelected()
    }

    /// The step the right-hand pane is showing. Never out of range.
    private var currentStep: PackageStep {
        guard let pkg = selected else { return .single }
        let steps = pkg.steps
        return steps[min(max(stepIndex, 0), steps.count - 1)]
    }

    @objc private func stepChanged(_ sender: NSSegmentedControl) {
        stepIndex = sender.selectedSegment
        showSelected()
    }

    // MARK: the detail pane

    private func showSelected() {
        for view in detailStack.arrangedSubviews {
            detailStack.removeArrangedSubview(view)
            view.removeFromSuperview()
        }
        promptView = nil
        guard let pkg = selected, let repo = repo else { return }
        doc = pkg.promptDoc()

        func addFull(_ view: NSView) {
            detailStack.addArrangedSubview(view)
            view.widthAnchor.constraint(equalTo: detailStack.widthAnchor, constant: -24).isActive = true
        }

        let title = NSTextField(labelWithString: pkg.title)
        title.font = .systemFont(ofSize: 15, weight: .semibold)
        detailStack.addArrangedSubview(title)

        let subtitle = NSTextField(labelWithString: "\(pkg.relPath) · \(pkg.meta.kind) · \(pkg.status.label)")
        subtitle.font = .systemFont(ofSize: 11)
        subtitle.textColor = .secondaryLabelColor
        detailStack.addArrangedSubview(subtitle)

        if !pkg.meta.outputs.isEmpty {
            let outs = NSTextField(labelWithString: "makes: " + pkg.meta.outputs.joined(separator: ", "))
            outs.font = .systemFont(ofSize: 11)
            outs.textColor = .secondaryLabelColor
            outs.lineBreakMode = .byTruncatingMiddle
            addFull(outs)
        }

        // The steps, when this package is more than one generation.
        let steps = pkg.steps
        stepIndex = min(max(stepIndex, 0), steps.count - 1)
        if steps.count > 1 {
            detailStack.addArrangedSubview(sectionLabel("STEPS — each is its own ChatGPT generation"))
            let seg = NSSegmentedControl()
            seg.segmentCount = steps.count
            seg.segmentStyle = .automatic
            seg.trackingMode = .selectOne
            for (i, step) in steps.enumerated() {
                let have = pkg.returnedImage(step: step) != nil
                seg.setLabel("\(i + 1). \(step.title)" + (have ? " ✓" : ""), forSegment: i)
                seg.setWidth(0, forSegment: i)
            }
            seg.selectedSegment = stepIndex
            seg.target = self
            seg.action = #selector(stepChanged(_:))
            detailStack.addArrangedSubview(seg)
        }

        // 1. attach these files, in this order
        detailStack.addArrangedSubview(sectionLabel("1 · ATTACH THESE FILES, IN THIS ORDER"))
        let attach = doc?.attach ?? []
        if attach.isEmpty {
            detailStack.addArrangedSubview(NSTextField(labelWithString: "prompt.md lists no attachments."))
        }
        for (i, rel) in attach.enumerated() {
            addFull(attachRow(index: i + 1, rel: rel, repo: repo))
        }

        // 2. the prompt
        let step = currentStep
        detailStack.addArrangedSubview(sectionLabel(
            steps.count > 1 ? "2 · THE PROMPT — \(step.title.uppercased())" : "2 · THE PROMPT"))
        let promptButtons = NSStackView(views: [
            ActionButton(title: "Copy prompt") { [weak self] in self?.copyPrompt() },
            ActionButton(title: "Open ChatGPT") { NSWorkspace.shared.open(URL(string: "https://chatgpt.com")!) },
            ActionButton(title: "Open prompt.md") { [weak self] in
                guard let pkg = self?.selected else { return }
                NSWorkspace.shared.open(pkg.dir.appendingPathComponent("prompt.md"))
            },
        ])
        promptButtons.orientation = .horizontal
        promptButtons.spacing = 8
        detailStack.addArrangedSubview(promptButtons)

        let (promptScroll, promptText) = readOnlyTextView(height: 220, monospaced: true)
        promptText.string = doc.map { $0.prompt(at: step.promptIndex) } ?? "prompt.md could not be read."
        promptView = promptText
        addFull(promptScroll)

        // 3. the return area
        detailStack.addArrangedSubview(sectionLabel(
            "3 · BRING THE IMAGE BACK — saved as \(step.returnFile)"))
        let drop = DropZone()
        drop.translatesAutoresizingMaskIntoConstraints = false
        drop.heightAnchor.constraint(equalToConstant: 84).isActive = true
        drop.onDrop = { [weak self] dropped in self?.accept(dropped) }
        addFull(drop)

        let returnButtons = NSStackView(views: [
            ActionButton(title: "Paste image from clipboard") { [weak self] in self?.pasteImage() },
            ActionButton(title: "Use newest image in ~/Downloads") { [weak self] in self?.useNewestDownload() },
            ActionButton(title: "Reveal package") { [weak self] in
                guard let pkg = self?.selected else { return }
                NSWorkspace.shared.activateFileViewerSelecting([pkg.dir])
            },
        ])
        returnButtons.orientation = .horizontal
        returnButtons.spacing = 8
        detailStack.addArrangedSubview(returnButtons)

        // One slot per step: a thumbnail and a ✓ when its image is in the folder.
        for (i, s) in steps.enumerated() {
            let returned = pkg.returnedImage(step: s)
            let mark = returned == nil ? "○" : "✓"
            let name = returned?.lastPathComponent ?? s.returnFile
            let text = steps.count > 1
                ? "\(mark) \(i + 1). \(s.title) — \(name)\(returned == nil ? " (not back yet)" : "")"
                : "\(mark) \(name)\(returned == nil ? " (not back yet)" : "")"
            let label = NSTextField(labelWithString: text)
            label.font = .systemFont(ofSize: 12)
            label.textColor = returned == nil ? .secondaryLabelColor : .labelColor
            var views: [NSView] = []
            if let returned { views.append(thumbView(returned, side: 64)) }
            views.append(label)
            if returned != nil {
                views.append(ActionButton(title: "Reveal") {
                    NSWorkspace.shared.activateFileViewerSelecting([returned!])
                })
            }
            let row = NSStackView(views: views)
            row.orientation = .horizontal
            row.spacing = 8
            row.alignment = .centerY
            detailStack.addArrangedSubview(row)
        }
        if pkg.allStepsReturned {
            detailStack.addArrangedSubview(
                ActionButton(title: "Cut it now (ingest)") { [weak self] in self?.ingestSelected() })
        } else if steps.count > 1 {
            let waiting = NSTextField(labelWithString:
                "ingest runs by itself once all \(steps.count) images are back.")
            waiting.font = .systemFont(ofSize: 11)
            waiting.textColor = .secondaryLabelColor
            detailStack.addArrangedSubview(waiting)
        }

        // The checklist.
        detailStack.addArrangedSubview(sectionLabel("CHECKLIST"))
        let (checkScroll, checkText) = readOnlyTextView(height: 190, monospaced: false)
        checkText.string = doc?.checklist ?? ""
        addFull(checkScroll)
    }

    private func thumbView(_ url: URL, side: CGFloat) -> NSImageView {
        let view = NSImageView()
        view.translatesAutoresizingMaskIntoConstraints = false
        view.imageScaling = .scaleProportionallyDown
        view.image = Img.thumbnail(url, side: side)
        view.widthAnchor.constraint(equalToConstant: side).isActive = true
        view.heightAnchor.constraint(equalToConstant: side).isActive = true
        view.wantsLayer = true
        view.layer?.borderWidth = 1
        view.layer?.borderColor = NSColor.separatorColor.cgColor
        return view
    }

    private func attachRow(index: Int, rel: String, repo: Repo) -> NSView {
        let url = repo.root.appendingPathComponent(rel)
        let exists = FileManager.default.fileExists(atPath: url.path)
        let thumb = thumbView(url, side: 56)

        let label = NSTextField(labelWithString: "\(index). \(rel)" + (exists ? "" : "   — MISSING"))
        label.font = .systemFont(ofSize: 12)
        label.textColor = exists ? .labelColor : .systemRed
        label.lineBreakMode = .byTruncatingMiddle
        label.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)

        let copy = ActionButton(title: "Copy") { [weak self] in
            do {
                try Img.copyToPasteboard(pngFile: url)
                self?.say("Copied \(rel) to the clipboard — paste it into ChatGPT.", error: false)
                self?.log("copied image: \(rel)")
            } catch {
                self?.fail("could not copy \(rel): \(error.localizedDescription)")
            }
        }
        copy.isEnabled = exists
        let reveal = ActionButton(title: "Reveal") {
            NSWorkspace.shared.activateFileViewerSelecting([url])
        }
        reveal.isEnabled = exists

        let row = NSStackView(views: [thumb, label, copy, reveal])
        row.orientation = .horizontal
        row.spacing = 8
        row.alignment = .centerY
        row.translatesAutoresizingMaskIntoConstraints = false
        return row
    }

    // MARK: actions

    func copyPrompt() {
        let step = currentStep
        guard let prompt = doc?.prompt(at: step.promptIndex), !prompt.isEmpty else {
            fail("no prompt text in this package's prompt.md")
            return
        }
        let pb = NSPasteboard.general
        pb.clearContents()
        pb.setString(prompt, forType: .string)
        let which = (selected?.steps.count ?? 1) > 1 ? " — \(step.title)" : ""
        say("Prompt copied\(which) (\(prompt.count) characters).", error: false)
        log("copied prompt for \(selected?.relPath ?? "?")\(which)")
    }

    func pasteImage() {
        do {
            let png = try Img.pngFromPasteboard()
            try store(png: png, note: "clipboard")
        } catch {
            fail(error.localizedDescription)
        }
    }

    func useNewestDownload() {
        guard let hit = Img.newestDownload() else {
            fail("no png/jpg/webp modified in ~/Downloads in the last hour")
            return
        }
        let alert = NSAlert()
        alert.messageText = "Use this image?"
        alert.informativeText = "\(hit.lastPathComponent)\n\nIt will be saved as \(currentStep.returnFile) in \(selected?.relPath ?? "")."
        alert.addButton(withTitle: "Use it")
        alert.addButton(withTitle: "Cancel")
        guard alert.runModal() == .alertFirstButtonReturn else { return }
        do {
            let png = try Img.pngData(fromFile: hit)
            try store(png: png, note: hit.lastPathComponent)
        } catch {
            fail(error.localizedDescription)
        }
    }

    private func accept(_ dropped: Dropped) {
        do {
            switch dropped {
            case .file(let url):
                try store(png: try Img.pngData(fromFile: url), note: url.lastPathComponent)
            case .data(let data):
                try store(png: try Img.pngData(fromImageData: data), note: "dropped image")
            }
        } catch {
            fail(error.localizedDescription)
        }
    }

    /// Save the current step's image into the selected package, and cut once every step is back.
    private func store(png: Data, note: String) throws {
        guard let pkg = selected else { throw ArtTrayError("select a package first") }
        let step = currentStep
        let target = try Img.writeReturned(png, into: pkg.dir, named: step.returnFile)
        log("saved \(note) → \(pkg.relPath)/\(target.lastPathComponent) (\(png.count / 1024) KB)")
        if pkg.allStepsReturned {
            say("Saved \(target.lastPathComponent) in \(pkg.relPath). Cutting it…", error: false)
            ingestSelected()
        } else {
            let missing = zip(pkg.steps, pkg.returnedImages())
                .filter { $0.1 == nil }.map { $0.0.title }
            say("Saved \(target.lastPathComponent). Still waiting for: \(missing.joined(separator: ", ")).",
                error: false)
            // Move on to the first step that has no image yet.
            if let next = pkg.returnedImages().firstIndex(where: { $0 == nil }) { stepIndex = next }
            reload()
        }
    }

    func ingestSelected() {
        guard let repo = repo, let pkg = selected else { return }
        guard pkg.allStepsReturned else {
            fail("this package needs all \(pkg.steps.count) images before it can be cut")
            return
        }
        let rel = "story/packages/" + pkg.relPath
        runScript(repo.storyPrompt, args: ["ingest", rel], label: "ingest \(pkg.relPath)")
    }

    func regeneratePackages() {
        guard let repo = repo else { return }
        runScript(repo.storyPrompt, args: ["packages"], label: "packages")
    }

    func hotReload() {
        guard let repo = repo else { return }
        runScript(repo.fastReload, args: [], label: "fast_reload")
    }

    private func runScript(_ script: URL, args: [String], label: String) {
        guard let repo = repo else { return }
        guard FileManager.default.fileExists(atPath: script.path) else {
            fail("\(script.lastPathComponent) is not in \(repo.root.path)")
            return
        }
        if runner.busy {
            fail("a command is already running — wait for it to finish")
            return
        }
        busyIndicator.startAnimation(nil)
        say("running \(label)…", error: false)
        runner.run(script: script, args: args, cwd: repo.root,
                   log: { [weak self] line in self?.log(line) },
                   done: { [weak self] code, startError in
                       guard let self = self else { return }
                       self.busyIndicator.stopAnimation(nil)
                       if let startError = startError {
                           self.fail(startError)
                       } else if code == 0 {
                           self.say("\(label) finished.", error: false)
                           self.log("— \(label) exit 0 —")
                       } else {
                           self.fail("\(label) failed (exit \(code)) — see the log above. The image is still in the package folder.")
                           self.log("— \(label) exit \(code) —")
                       }
                       self.reload()
                   })
    }

    func chooseRepo() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = false
        panel.title = "Choose the repository (the folder with story_prompt.py)"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        guard Repo.looksLikeRepo(url) else {
            fail("\(url.path) has no story_prompt.py in it")
            return
        }
        UserDefaults.standard.set(url.path, forKey: "repoPath")
        repo = Repo(root: url)
        selected = nil
        log("repository: \(url.path)")
        reload()
    }

    // MARK: log and status

    func log(_ text: String) {
        guard let logView = logView else { return }
        let stamp = DateFormatter.localizedString(from: Date(), dateStyle: .none, timeStyle: .medium)
        logView.string += (logView.string.isEmpty ? "" : "\n") + "[\(stamp)] " + text
        logView.scrollToEndOfDocument(nil)
    }

    func say(_ text: String, error: Bool) {
        statusLine.stringValue = text
        statusLine.textColor = error ? .systemRed : .secondaryLabelColor
    }

    func fail(_ text: String) {
        say(text, error: true)
        log("ERROR: " + text)
        NSSound.beep()
    }

    // MARK: window

    func windowDidBecomeKey(_ notification: Notification) {
        reload()
    }

    /// ⌘C: the selection in a text view if there is one, otherwise the prompt.
    func commandCopy() {
        if let responder = window.firstResponder as? NSTextView, responder.selectedRange().length > 0 {
            responder.copy(nil)
            return
        }
        copyPrompt()
    }
}

// MARK: - app

final class AppDelegate: NSObject, NSApplicationDelegate {
    let controller = TrayController()

    func applicationDidFinishLaunching(_ notification: Notification) {
        buildMenu()
        if let root = Repo.discover() {
            controller.repo = Repo(root: root)
            controller.log("repository: \(root.path)")
        }
        controller.window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        if controller.repo == nil {
            controller.say("No repository found. Use “Choose repo…”.", error: true)
        } else {
            controller.reload()
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    private func buildMenu() {
        let main = NSMenu()

        let appItem = NSMenuItem()
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "About Art Tray", action: #selector(NSApplication.orderFrontStandardAboutPanel(_:)), keyEquivalent: "")
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Choose repo…", action: #selector(chooseRepo), keyEquivalent: "o").target = self
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Hide Art Tray", action: #selector(NSApplication.hide(_:)), keyEquivalent: "h")
        appMenu.addItem(withTitle: "Quit Art Tray", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = appMenu
        main.addItem(appItem)

        let editItem = NSMenuItem()
        let editMenu = NSMenu(title: "Edit")
        editMenu.addItem(withTitle: "Copy", action: #selector(menuCopy), keyEquivalent: "c").target = self
        editMenu.addItem(withTitle: "Paste image", action: #selector(menuPaste), keyEquivalent: "v").target = self
        editMenu.addItem(.separator())
        editMenu.addItem(withTitle: "Select All", action: #selector(NSText.selectAll(_:)), keyEquivalent: "a")
        editItem.submenu = editMenu
        main.addItem(editItem)

        let viewItem = NSMenuItem()
        let viewMenu = NSMenu(title: "Tray")
        viewMenu.addItem(withTitle: "Refresh", action: #selector(menuRefresh), keyEquivalent: "r").target = self
        viewMenu.addItem(withTitle: "Regenerate packages", action: #selector(menuRegenerate), keyEquivalent: "R").target = self
        viewMenu.addItem(withTitle: "Hot reload", action: #selector(menuHotReload), keyEquivalent: "l").target = self
        viewMenu.addItem(.separator())
        viewMenu.addItem(withTitle: "Open ChatGPT", action: #selector(menuChatGPT), keyEquivalent: "g").target = self
        viewItem.submenu = viewMenu
        main.addItem(viewItem)

        NSApp.mainMenu = main
    }

    @objc private func menuCopy() { controller.commandCopy() }
    @objc private func menuPaste() { controller.pasteImage() }
    @objc private func menuRefresh() { controller.reload() }
    @objc private func menuRegenerate() { controller.regeneratePackages() }
    @objc private func menuHotReload() { controller.hotReload() }
    @objc private func menuChatGPT() { NSWorkspace.shared.open(URL(string: "https://chatgpt.com")!) }
    @objc private func chooseRepo() { controller.chooseRepo() }
}

// MARK: - selftest

enum SelfTest {
    static var failures = 0
    static var checksRun = 0
    static var packageCount = -1

    static func check(_ ok: Bool, _ what: String) {
        checksRun += 1
        if ok {
            print("  ok    \(what)")
        } else {
            failures += 1
            print("  FAIL  \(what)")
        }
    }

    static func equal<T: Equatable>(_ got: T, _ want: T, _ what: String) {
        checksRun += 1
        if got == want {
            print("  ok    \(what)")
        } else {
            failures += 1
            print("  FAIL  \(what): got \(got), wanted \(want)")
        }
    }

    static func run() -> Int32 {
        print("ArtTray selftest")

        // 1. README order parsing.
        let readme = """
        # story/packages — the art tray

        ## 1. The short path to something on the phone

        - [ ] **1.** Guildclerk's reference sheet\u{0020}
              [`cast/guildclerk/refsheet`](cast/guildclerk/refsheet/prompt.md) · to generate
        - [x] **2.** Hart's reference sheet\u{0020}
              [`cast/hart/refsheet`](cast/hart/refsheet/prompt.md) · done

        ## 2. Character reference sheets

        | character | folder | status |
        | --- | --- | --- |
        | Hart | [`cast/hart/refsheet`](cast/hart/refsheet/prompt.md) | done |
        | Stolz | [`cast/stolz/refsheet`](cast/stolz/refsheet/prompt.md) | to generate |
        | Blocked | `cast/blocked/walker` | needs the reference sheet first |
        """
        let order = Repo.readmeOrder(text: readme)
        equal(order.map { $0.0 }, ["cast/guildclerk/refsheet", "cast/hart/refsheet", "cast/stolz/refsheet"],
              "README order, de-duplicated, unlinked rows skipped")
        equal(order[0].1, 1, "short-path step 1")
        equal(order[1].1, 2, "short-path step 2")
        equal(order[2].1, nil, "rows past the short path carry no step")

        // 2. package.json parsing.
        let json = """
        {"kind": "props", "map": "halm", "title": "halm — 9 props",
         "makes": "stuff", "outputs": ["story/field/props/barrel.png", "story/field/props/well.png"],
         "dir": "story/packages/ch01/halm/props"}
        """.data(using: .utf8)!
        let meta = try! PackageMeta.parse(json)
        equal(meta.kind, "props", "package.json kind")
        equal(meta.outputs.count, 2, "package.json outputs")
        equal(meta.map ?? "", "halm", "package.json map")
        check((try? PackageMeta.parse("{}".data(using: .utf8)!)) == nil, "package.json with no kind is rejected")

        // 3. prompt.md parsing.
        let prompt = """
        # ChatGPT package: Hart — reference sheet

        ## What exists already

        - reference sheet `story/refs/hart.png` — missing

        ## 1. Start a new chat and attach these files, in this order

        1. `story/refs/style.png`
        2. `story/refs/bron.png`

        ## 2. Paste this prompt exactly

        ````
        Create ONE image: a character reference sheet.
        RENDERING: ```inline``` stays inside.
        ````

        ## 3. Afterwards

        - [ ] All 9 slots filled
        """
        let doc = PromptDoc.parse(prompt)
        equal(doc.attach, ["story/refs/style.png", "story/refs/bron.png"], "attach list, in order")
        check(doc.prompt.hasPrefix("Create ONE image"), "fenced prompt starts right")
        check(doc.prompt.contains("```inline```"), "a shorter fence inside the block is kept")
        check(!doc.prompt.contains("````"), "the fence itself is not in the prompt")
        check(doc.checklist.contains("All 9 slots filled"), "checklist text")

        equal(doc.prompts.count, 1, "a one-prompt package has one prompt")
        check(meta.steps == [.single], "a package.json with no steps is one step returning returned.png")

        // 3b. a three-step package: `screen` — PAINT, WALKABLE MASK, FOREGROUND MASK.
        let screenJSON = """
        {"kind": "screen", "title": "halm / square — screen",
         "outputs": ["story/field/views/halm_square_paint.png"],
         "steps": [{"title": "PAINT", "prompt_index": 0, "return_file": "returned.png"},
                   {"title": "WALKABLE MASK", "prompt_index": 1, "return_file": "returned_walk.png"},
                   {"title": "FOREGROUND MASK", "prompt_index": 2, "return_file": "returned_fg.png"}]}
        """.data(using: .utf8)!
        let screenMeta = try! PackageMeta.parse(screenJSON)
        equal(screenMeta.steps.count, 3, "three steps parsed")
        equal(screenMeta.steps[2].returnFile, "returned_fg.png", "third step's return file")
        equal(screenMeta.steps[1].promptIndex, 1, "second step's prompt index")
        equal(screenMeta.steps[2].stem, "returned_fg", "return file stem")

        let threePrompts = """
        ## 1. Start a new chat and attach these files, in this order

        1. `story/field/views/halm_square.png`

        ## 2a. Paste this prompt exactly — PAINT

        ````
        Paint over the capture.
        ## not a heading: fenced text
        ````

        ## 2b. Paste this prompt exactly — WALKABLE MASK

        ````
        White where the player may stand.
        ````

        ## 2c. Paste this prompt exactly — FOREGROUND MASK

        ````
        White where the art is in front of the player.
        ````

        ## 3. Afterwards

        - [ ] Nothing moved
        """
        let three = PromptDoc.parse(threePrompts)
        equal(three.prompts.count, 3, "three fenced prompts collected in order")
        check(three.prompt(at: 1).hasPrefix("White where the player"), "step 2 gets its own prompt")
        check(three.prompt(at: 2).hasPrefix("White where the art"), "step 3 gets its own prompt")
        check(three.prompt(at: 9).hasPrefix("Paint over"), "an out-of-range index falls back to the first")
        check(three.prompts[0].contains("## not a heading"), "a heading inside a fence stays prompt text")
        equal(three.attach, ["story/field/views/halm_square.png"], "attach list of a screen package")

        // 4. status derivation.
        let now = Date()
        let older = now.addingTimeInterval(-60)
        let newer = now.addingTimeInterval(60)
        equal(statusFrom(returned: nil, outputDates: [nil, nil]), .toGenerate, "no image, no outputs → to generate")
        equal(statusFrom(returned: nil, outputDates: [older, older]), .done, "outputs, no image → done")
        equal(statusFrom(returned: now, outputDates: [older, older]), .returnedNotCut, "image newer than outputs")
        equal(statusFrom(returned: now, outputDates: [newer, newer]), .done, "outputs newer than image → done")
        equal(statusFrom(returned: now, outputDates: [newer, nil]), .returnedNotCut, "an output missing → not cut")
        equal(statusFrom(returned: nil, outputDates: [older, nil]), .partial(1, 2), "half cut, no image waiting")
        equal(statusFrom(returned: now, returnsHave: 1, returnsTotal: 3, outputDates: [nil]),
              .awaitingSteps(1, 3), "one of three step images back")
        equal(statusFrom(returned: now, returnsHave: 3, returnsTotal: 3, outputDates: [nil]),
              .returnedNotCut, "every step back, nothing cut")
        equal(statusFrom(returned: older, returnsHave: 2, returnsTotal: 3, outputDates: [now]),
              .done, "an older part-returned package whose outputs are newer is done")

        // 5. status straight off the file system, including returned_prev handling.
        let tmp = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("arttray-selftest-\(getpid())")
        let pkgDir = tmp.appendingPathComponent("story/packages/ch01/halm/props")
        let outDir = tmp.appendingPathComponent("story/field/props")
        try? FileManager.default.createDirectory(at: pkgDir, withIntermediateDirectories: true)
        try? FileManager.default.createDirectory(at: outDir, withIntermediateDirectories: true)
        let metaJSON = """
        {"kind": "props", "title": "t", "outputs": ["story/field/props/barrel.png"], "dir": "x"}
        """
        try? metaJSON.write(to: pkgDir.appendingPathComponent("package.json"), atomically: true, encoding: .utf8)
        let pkg = Package(relPath: "ch01/halm/props", dir: pkgDir,
                          meta: try! PackageMeta.parse(metaJSON.data(using: .utf8)!))
        pkg.refreshStatus(repoRoot: tmp)
        equal(pkg.status, .toGenerate, "fresh package folder → to generate")

        // A 4x4 red image, TIFF on the way in, PNG on the way out.
        let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 4, pixelsHigh: 4, bitsPerSample: 8,
                                   samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                                   colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
        // Fill it red by hand: setColor(atX:y:) logs colorspace noise on this OS.
        if let pixels = rep.bitmapData {
            for i in stride(from: 0, to: rep.bytesPerRow * 4, by: 4) {
                pixels[i] = 255; pixels[i + 1] = 0; pixels[i + 2] = 0; pixels[i + 3] = 255
            }
        }
        let tiff = rep.tiffRepresentation!
        let png = try! Img.pngData(fromImageData: tiff)
        check(png.starts(with: [0x89, 0x50, 0x4E, 0x47]), "TIFF data converts to real PNG bytes")
        check(NSBitmapImageRep(data: png)?.pixelsWide == 4, "converted PNG keeps its size")

        try! Img.writeReturned(png, into: pkgDir)
        pkg.refreshStatus(repoRoot: tmp)
        equal(pkg.status, .returnedNotCut, "returned.png with no outputs → returned, not cut")
        check(pkg.returnedImage()?.lastPathComponent == "returned.png", "returned image found")

        try! Img.writeReturned(png, into: pkgDir)
        check(FileManager.default.fileExists(atPath: pkgDir.appendingPathComponent("returned_prev.png").path),
              "a second save keeps the previous one as returned_prev.png")
        check(try! Img.pngData(fromFile: pkgDir.appendingPathComponent("returned.png")).starts(with: [0x89, 0x50]),
              "a PNG file is passed through unchanged")

        // 5b. a three-step package on disk: only ready to cut when all three images are back.
        let screenDir = tmp.appendingPathComponent("story/packages/ch01/halm/screens/square")
        try? FileManager.default.createDirectory(at: screenDir, withIntermediateDirectories: true)
        let screenPkg = Package(relPath: "ch01/halm/screens/square", dir: screenDir, meta: screenMeta)
        screenPkg.refreshStatus(repoRoot: tmp)
        equal(screenPkg.status, .toGenerate, "screen package with nothing back → to generate")
        try! Img.writeReturned(png, into: screenDir, named: "returned.png")
        screenPkg.refreshStatus(repoRoot: tmp)
        equal(screenPkg.status, .awaitingSteps(1, 3), "one image back → 1/3 returned")
        check(!screenPkg.allStepsReturned, "not ready to cut with one image")
        try! Img.writeReturned(png, into: screenDir, named: "returned_walk.png")
        try! Img.writeReturned(png, into: screenDir, named: "returned_fg.png")
        screenPkg.refreshStatus(repoRoot: tmp)
        equal(screenPkg.status, .returnedNotCut, "all three back → returned, not cut")
        check(screenPkg.allStepsReturned, "ready to cut with all three")
        try! Img.writeReturned(png, into: screenDir, named: "returned_fg.png")
        check(FileManager.default.fileExists(atPath: screenDir.appendingPathComponent("returned_fg_prev.png").path),
              "a step keeps its own previous image as <stem>_prev.png")

        try? Data([0, 1, 2]).write(to: outDir.appendingPathComponent("barrel.png"))
        pkg.refreshStatus(repoRoot: tmp)
        equal(pkg.status, .done, "output newer than the image → done")
        try? FileManager.default.removeItem(at: tmp)

        // 6. the real repository, when the binary can find one.
        if let root = Repo.discover(arguments: CommandLine.arguments, defaults: .standard) {
            let repo = Repo(root: root)
            let packages = repo.scanPackages()
            packageCount = packages.count
            print("  note  repository \(root.path): \(packages.count) package(s)")
            check(!packages.isEmpty, "the repository has packages")
            check(packages.allSatisfy { !$0.meta.kind.isEmpty }, "every package.json has a kind")
            let linked = Repo.readmeOrder(text: (try? String(contentsOf: repo.readme, encoding: .utf8)) ?? "")
            check(linked.allSatisfy { rel, _ in packages.contains { $0.relPath == rel } },
                  "every README link points at a real package folder")
            if let first = packages.first, let doc = first.promptDoc() {
                check(!doc.prompt.isEmpty, "first package (\(first.relPath)) has prompt text")
                check(!doc.attach.isEmpty, "first package lists files to attach")
                check(!doc.checklist.isEmpty, "first package has a checklist")
                check(doc.attach.allSatisfy { !$0.contains("`") }, "attach paths are clean")
            }
        } else {
            print("  note  no repository found from here; skipped the live checks")
        }

        // One line a script or a human can read at a glance: the verdict, how much was checked and
        // how many packages the tray actually holds.
        let tray = packageCount < 0 ? "no repository found" : "\(packageCount) packages in the tray"
        print(failures == 0
              ? "\nPASS — \(checksRun) checks, 0 failed, \(tray)"
              : "\nFAIL — \(checksRun) checks, \(failures) failed, \(tray)")
        return failures == 0 ? 0 : 1
    }
}

// MARK: - main

@main
struct ArtTrayMain {
    static func main() {
        if CommandLine.arguments.contains("--selftest") {
            exit(SelfTest.run())
        }
        if CommandLine.arguments.contains("--help") || CommandLine.arguments.contains("-h") {
            print("""
            ArtTray — the art tray for story/packages.

              ArtTray                 open the window
              ArtTray --repo <path>   use that repository (remembered)
              ArtTray --selftest      run the non-GUI checks and exit
            """)
            exit(0)
        }
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)
        let delegate = AppDelegate()
        app.delegate = delegate
        app.run()
    }
}
