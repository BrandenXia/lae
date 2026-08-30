import CLAE
import Foundation

public enum PrefixStrategy: UInt32, Sendable {
    case proportional = 1
    case fixed = 2
}

public enum ReadingModel: UInt32, Sendable {
    case prefix = 1
    case lexicalCore = 2
}

public enum PresentationPolicy: UInt32, Sendable {
    case binary = 1
    case variableStrength = 2
}

public enum ModelType: UInt32, Sendable {
    case prefix = 1
    case lexicalCore = 2
    case linearSalience = 3
}

public struct ProcessOptions: Sendable, Equatable {
    public var language: String
    public var prefixStrategy: PrefixStrategy
    public var fixedGraphemes: UInt32
    public var prefixProportion: Float
    public var emphasisStrength: Float
    public var presentationPolicy: PresentationPolicy
    public var minimumEmphasisStrength: Float
    public var salienceThreshold: Float
    public var readingModel: ReadingModel

    public init(
        language: String = "",
        prefixStrategy: PrefixStrategy = .proportional,
        fixedGraphemes: UInt32 = 1,
        prefixProportion: Float = 0.5,
        emphasisStrength: Float = 1,
        presentationPolicy: PresentationPolicy = .binary,
        minimumEmphasisStrength: Float = 0,
        salienceThreshold: Float = 0,
        readingModel: ReadingModel = .prefix
    ) {
        self.language = language
        self.prefixStrategy = prefixStrategy
        self.fixedGraphemes = fixedGraphemes
        self.prefixProportion = prefixProportion
        self.emphasisStrength = emphasisStrength
        self.presentationPolicy = presentationPolicy
        self.minimumEmphasisStrength = minimumEmphasisStrength
        self.salienceThreshold = salienceThreshold
        self.readingModel = readingModel
    }
}

public struct TextSpan: Sendable, Equatable {
    public let begin: UInt64
    public let end: UInt64

    public init(begin: UInt64, end: UInt64) {
        self.begin = begin
        self.end = end
    }

    public func range(in text: String) -> Range<String.Index>? {
        guard
            let lowerOffset = Int(exactly: begin),
            let upperOffset = Int(exactly: end),
            lowerOffset <= upperOffset,
            upperOffset <= text.utf8.count
        else {
            return nil
        }
        let utf8 = text.utf8
        let lowerUTF8 = utf8.index(utf8.startIndex, offsetBy: lowerOffset)
        let upperUTF8 = utf8.index(utf8.startIndex, offsetBy: upperOffset)
        guard
            let lower = String.Index(lowerUTF8, within: text),
            let upper = String.Index(upperUTF8, within: text)
        else {
            return nil
        }
        return lower..<upper
    }

    public func substring(in text: String) -> Substring? {
        range(in: text).map { text[$0] }
    }
}

public struct Emphasis: Sendable, Equatable {
    public let span: TextSpan
    public let strength: Float
    public let styleClass: UInt32

    public init(span: TextSpan, strength: Float, styleClass: UInt32) {
        self.span = span
        self.strength = strength
        self.styleClass = styleClass
    }
}

public struct LAEError: Error, Sendable, Equatable, CustomStringConvertible {
    public let status: Int32
    public let name: String
    public let detail: String

    public init(status: Int32, name: String, detail: String = "") {
        self.status = status
        self.name = name
        self.detail = detail
    }

    public var description: String {
        detail.isEmpty ? name : "\(name): \(detail)"
    }
}

private func string(from view: le_string_view_t) -> String {
    guard let data = view.data, view.size > 0 else {
        return ""
    }
    let bytes = UnsafeRawPointer(data).assumingMemoryBound(to: UInt8.self)
    return String(decoding: UnsafeBufferPointer(start: bytes, count: view.size), as: UTF8.self)
}

private func statusName(_ status: le_status_t) -> String {
    guard let name = le_status_string(status) else {
        return "LE_ERROR_UNKNOWN"
    }
    return String(cString: name)
}

private func error(_ status: le_status_t, runtime: OpaquePointer?) -> LAEError {
    let detail = runtime.map { string(from: le_runtime_last_error($0)) } ?? ""
    return LAEError(status: status, name: statusName(status), detail: detail)
}

public final class Model: @unchecked Sendable {
    fileprivate var handle: OpaquePointer?

    fileprivate init(handle: OpaquePointer) {
        self.handle = handle
    }

    deinit {
        close()
    }

    fileprivate func openHandle() throws -> OpaquePointer {
        guard let handle else {
            throw LAEError(status: LE_ERROR_INVALID_ARGUMENT, name: "LE_ERROR_INVALID_ARGUMENT",
                           detail: "model is closed")
        }
        return handle
    }

    public var type: ModelType {
        get throws {
            let raw = le_model_type(try openHandle())
            guard let type = ModelType(rawValue: raw) else {
                throw LAEError(status: LE_ERROR_MODEL_INCOMPATIBLE,
                               name: "LE_ERROR_MODEL_INCOMPATIBLE",
                               detail: "model type is not recognized by this binding")
            }
            return type
        }
    }

    public var version: UInt32 {
        get throws { le_model_version(try openHandle()) }
    }

    public var minimumABI: (major: UInt16, minor: UInt16) {
        get throws {
            let packed = le_model_minimum_abi_version(try openHandle())
            return (UInt16(packed >> 16), UInt16(packed & 0xffff))
        }
    }

    public var languages: [String] {
        get throws {
            let handle = try openHandle()
            return (0..<le_model_language_count(handle)).map {
                string(from: le_model_language_at(handle, $0))
            }
        }
    }

    public var requiredFeatures: [UInt32] {
        get throws {
            let handle = try openHandle()
            let count = le_model_required_feature_count(handle)
            guard count > 0, let data = le_model_required_feature_data(handle) else {
                return []
            }
            return (0..<count).map { data[$0] }
        }
    }

    public func supports(language: String) throws -> Bool {
        let handle = try openHandle()
        return language.utf8CString.withUnsafeBufferPointer { bytes in
            let view = le_string_view_t(data: bytes.baseAddress, size: language.utf8.count)
            return le_model_supports_language(handle, view) != 0
        }
    }

    public func close() {
        if let handle {
            le_model_destroy(handle)
            self.handle = nil
        }
    }
}

public final class Runtime: @unchecked Sendable {
    private var handle: OpaquePointer?

    public init() throws {
        var created: OpaquePointer?
        let status = le_runtime_create(nil, &created)
        guard status == LE_OK, let created else {
            throw error(status, runtime: nil)
        }
        handle = created
    }

    deinit {
        close()
    }

    private func openHandle() throws -> OpaquePointer {
        guard let handle else {
            throw LAEError(status: LE_ERROR_INVALID_ARGUMENT, name: "LE_ERROR_INVALID_ARGUMENT",
                           detail: "runtime is closed")
        }
        return handle
    }

    public func loadModel(_ bytes: [UInt8]) throws -> Model {
        let runtime = try openHandle()
        var loaded: OpaquePointer?
        let status = bytes.withUnsafeBytes { buffer in
            le_model_load(runtime, buffer.baseAddress, buffer.count, &loaded)
        }
        guard status == LE_OK, let loaded else {
            throw error(status, runtime: runtime)
        }
        return Model(handle: loaded)
    }

    public func loadModel(contentsOf url: URL) throws -> Model {
        try loadModel(Array(Data(contentsOf: url)))
    }

    public func process(
        _ text: String,
        options: ProcessOptions = ProcessOptions(),
        model: Model? = nil
    ) throws -> [Emphasis] {
        let runtime = try openHandle()
        var nativeOptions = le_process_options_t()
        le_process_options_init(&nativeOptions)
        nativeOptions.prefix_strategy = options.prefixStrategy.rawValue
        nativeOptions.fixed_graphemes = options.fixedGraphemes
        nativeOptions.prefix_proportion = options.prefixProportion
        nativeOptions.emphasis_strength = options.emphasisStrength
        nativeOptions.presentation_policy = options.presentationPolicy.rawValue
        nativeOptions.minimum_emphasis_strength = options.minimumEmphasisStrength
        nativeOptions.salience_threshold = options.salienceThreshold
        nativeOptions.reading_model = options.readingModel.rawValue
        let modelHandle = try model?.openHandle()

        var result: OpaquePointer?
        let status = text.utf8CString.withUnsafeBufferPointer { textBytes in
            options.language.utf8CString.withUnsafeBufferPointer { languageBytes in
                let textView = le_string_view_t(
                    data: textBytes.baseAddress,
                    size: text.utf8.count
                )
                nativeOptions.language = le_string_view_t(
                    data: languageBytes.baseAddress,
                    size: options.language.utf8.count
                )
                if let modelHandle {
                    return le_process_with_model(
                        runtime, modelHandle, textView, &nativeOptions, &result
                    )
                }
                return le_process(runtime, textView, &nativeOptions, &result)
            }
        }
        guard status == LE_OK, let result else {
            throw error(status, runtime: runtime)
        }
        defer { le_result_destroy(result) }

        let count = le_result_emphasis_count(result)
        guard count > 0, let data = le_result_emphasis_data(result) else {
            return []
        }
        return (0..<count).map { index in
            let value = data[index]
            return Emphasis(
                span: TextSpan(begin: value.span.begin, end: value.span.end),
                strength: value.strength,
                styleClass: value.style_class
            )
        }
    }

    public func close() {
        if let handle {
            le_runtime_destroy(handle)
            self.handle = nil
        }
    }
}
