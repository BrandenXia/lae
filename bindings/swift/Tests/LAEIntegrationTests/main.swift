import Foundation
import LAE

private var failures = 0

private final class ConcurrentResults: @unchecked Sendable {
    private let lock = NSLock()
    private var values: [Bool] = []

    func append(_ value: Bool) {
        lock.lock()
        values.append(value)
        lock.unlock()
    }

    var allPassed: Bool {
        lock.lock()
        defer { lock.unlock() }
        return values.count == 8 && values.allSatisfy { $0 }
    }
}

@MainActor
private func check(_ condition: @autoclosure () -> Bool, _ message: String) {
    if !condition() {
        FileHandle.standardError.write(Data("FAIL: \(message)\n".utf8))
        failures += 1
    }
}

@MainActor
private func testDefaultProcessing() throws {
    let runtime = try Runtime()
    defer { runtime.close() }
    let source = "éclair"
    let plan = try runtime.process(source)
    check(plan.map(\.span) == [TextSpan(begin: 0, end: 4)], "UTF-8 byte offsets")
    check(plan[0].span.substring(in: source) == "écl", "Swift String range conversion")
    check(TextSpan(begin: 1, end: 2).range(in: source) == nil,
          "invalid UTF-8 boundary rejection")
}

@MainActor
private func testLexicalCore() throws {
    let runtime = try Runtime()
    defer { runtime.close() }
    let options = ProcessOptions(language: "en", readingModel: .lexicalCore)
    let plan = try runtime.process("unbelievable reading", options: options)
    check(
        plan.map(\.span) == [TextSpan(begin: 2, end: 8), TextSpan(begin: 13, end: 17)],
        "English lexical-core routing"
    )
}

@MainActor
private func testExplicitRegions() throws {
    let runtime = try Runtime()
    defer { runtime.close() }
    let source = "unbelievable 日本語を 研究"
    let regions = [
        LanguageRegion(span: TextSpan(begin: 0, end: 13), language: "en"),
        LanguageRegion(span: TextSpan(begin: 13, end: 26), language: "ja", confidence: 0.9),
        LanguageRegion(span: TextSpan(begin: 26, end: 32), language: "zh-Hans", confidence: 0.8),
    ]
    let options = ProcessOptions(readingModel: .lexicalCore)
    let plan = try runtime.process(source, regions: regions, options: options)
    check(
        plan.map(\.span) == [
            TextSpan(begin: 2, end: 8),
            TextSpan(begin: 13, end: 22),
            TextSpan(begin: 26, end: 32),
        ],
        "explicit mixed-language routing"
    )
}

@MainActor
private func testExtendedGrapheme() throws {
    let runtime = try Runtime()
    defer { runtime.close() }
    let source = "👩‍🚀abc"
    let options = ProcessOptions(prefixStrategy: .fixed, fixedGraphemes: 1)
    let plan = try runtime.process(source, options: options)
    check(plan.map(\.span) == [TextSpan(begin: 0, end: 11)], "grapheme-safe prefix span")
    check(plan[0].span.substring(in: source) == "👩‍🚀", "extended grapheme conversion")
}

@MainActor
private func testConcurrentProcessing() throws {
    let runtime = try Runtime()
    defer { runtime.close() }
    let results = ConcurrentResults()
    DispatchQueue.concurrentPerform(iterations: 8) { _ in
        let passed = (try? runtime.process("reading").map(\.span))
            == [TextSpan(begin: 0, end: 4)]
        results.append(passed)
    }
    check(results.allPassed, "concurrent processing through one runtime")
}

@MainActor
private func testNativeDiagnostic() throws {
    let runtime = try Runtime()
    defer { runtime.close() }
    do {
        _ = try runtime.loadModel(Array("not a model".utf8))
        check(false, "invalid artifact rejection")
    } catch let error as LAEError {
        check(error.name == "LE_ERROR_MODEL_INVALID", "stable error name")
        check(!error.detail.isEmpty, "native error diagnostic")
    }
}

@MainActor
private func prefixArtifact() -> [UInt8] {
    var bytes: [UInt8] = Array("LAEMODL\0".utf8)
    func appendU16(_ value: UInt16) {
        bytes.append(UInt8(truncatingIfNeeded: value))
        bytes.append(UInt8(truncatingIfNeeded: value >> 8))
    }
    func appendU32(_ value: UInt32) {
        for shift in stride(from: 0, through: 24, by: 8) {
            bytes.append(UInt8(truncatingIfNeeded: value >> UInt32(shift)))
        }
    }
    appendU16(1)
    appendU16(0)
    for value: UInt32 in [
        64, 80, 0, (1 << 16) | 7, 1, 7, 1, 0, 64, 68, 68, 3, 0,
    ] {
        appendU32(value)
    }
    appendU16(2)
    bytes.append(contentsOf: Array("en".utf8))
    appendU32(2)
    appendU32(1)
    appendU32(Float(0.5).bitPattern)

    var checksum: UInt32 = 0xffff_ffff
    for byte in bytes {
        checksum ^= UInt32(byte)
        for _ in 0..<8 {
            checksum = (checksum >> 1) ^ ((checksum & 1) == 0 ? 0 : 0xedb8_8320)
        }
    }
    checksum ^= 0xffff_ffff
    for offset in 0..<4 {
        bytes[20 + offset] = UInt8(truncatingIfNeeded: checksum >> UInt32(offset * 8))
    }
    return bytes
}

@MainActor
private func testModelLifecycle() throws {
    let runtime = try Runtime()
    let model = try runtime.loadModel(prefixArtifact())
    defer {
        model.close()
        runtime.close()
    }
    let type = try model.type
    let version = try model.version
    let abi = try model.minimumABI
    let languages = try model.languages
    let requiredFeatures = try model.requiredFeatures
    let supportsEnglish = try model.supports(language: "en-US")
    check(type == .prefix, "model type metadata")
    check(version == 7, "model version metadata")
    check(abi.major == 1 && abi.minor == 7, "minimum ABI metadata")
    check(languages == ["en"], "model language metadata")
    check(requiredFeatures.isEmpty, "model feature metadata")
    check(supportsEnglish, "model language capability")
    let options = ProcessOptions(language: "en")
    let plan = try runtime.process("reading", options: options, model: model)
    check(plan.map(\.span) == [TextSpan(begin: 0, end: 1)], "artifact processing")
    let regionPlan = try runtime.process(
        "reading",
        regions: [LanguageRegion(span: TextSpan(begin: 0, end: 7), language: "en")],
        model: model
    )
    check(regionPlan.map(\.span) == [TextSpan(begin: 0, end: 1)],
          "artifact processing with explicit regions")

    runtime.close()
    let survivingLanguages = try model.languages
    check(survivingLanguages == ["en"], "model outlives loading runtime")
    let secondRuntime = try Runtime()
    defer { secondRuntime.close() }
    let secondPlan = try secondRuntime.process("reading", options: options, model: model)
    check(secondPlan.map(\.span) == [TextSpan(begin: 0, end: 1)],
          "model works with a later runtime")
    model.close()
    do {
        _ = try model.type
        check(false, "closed model rejection")
    } catch is LAEError {
    }
}

@MainActor
private func testClosedRuntime() throws {
    let runtime = try Runtime()
    runtime.close()
    do {
        _ = try runtime.process("reading")
        check(false, "closed runtime rejection")
    } catch is LAEError {
    }
}

do {
    try testDefaultProcessing()
    try testLexicalCore()
    try testExplicitRegions()
    try testExtendedGrapheme()
    try testConcurrentProcessing()
    try testNativeDiagnostic()
    try testModelLifecycle()
    try testClosedRuntime()
} catch {
    FileHandle.standardError.write(Data("FAIL: unexpected error: \(error)\n".utf8))
    failures += 1
}

if failures > 0 {
    exit(EXIT_FAILURE)
}
print("Swift binding integration tests passed")
