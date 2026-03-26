import SwiftUI
import UniformTypeIdentifiers

struct FilesView: View {
    @EnvironmentObject var appState: AppState
    @EnvironmentObject var controller: MeshTRXController

    @State private var pendingPhotoData: Data? = nil
    @State private var pendingPhotoName: String = "photo.jpg"
    @State private var previewImage: UIImage? = nil
    @State private var showPreview: Bool = false
    @State private var showFilePicker: Bool = false
    @State private var showImagePicker: Bool = false

    private var isConnected: Bool { appState.bleState == .connected }

    var body: some View {
        ZStack {
            AppColors.bgPrimary.ignoresSafeArea()

            VStack(spacing: 0) {
                buttonsRow

                if showPreview, let img = previewImage {
                    previewCard(img)
                }

                if appState.fileTransfers.isEmpty {
                    Spacer()
                    Text("Нет передач")
                        .foregroundColor(AppColors.textDim)
                    Spacer()
                } else {
                    transferList
                }
            }
        }
        .sheet(isPresented: $showImagePicker) {
            ImagePickerView { image in
                if let processed = ImageProcessor.preparePhoto(image) {
                    pendingPhotoData = processed
                    previewImage = UIImage(data: processed)
                    showPreview = true
                }
            }
        }
    }

    // MARK: - Buttons

    private var buttonsRow: some View {
        HStack(spacing: 12) {
            Button {
                showImagePicker = true
            } label: {
                Label("Фото", systemImage: "photo")
                    .font(.system(size: 13))
                    .foregroundColor(AppColors.greenAccent)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
                    .background(AppColors.greenBg)
                    .cornerRadius(8)
            }
            .disabled(!isConnected)

            Button {
                showFilePicker = true
            } label: {
                Label("Файл", systemImage: "doc")
                    .font(.system(size: 13))
                    .foregroundColor(AppColors.blueAccent)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
                    .background(AppColors.blueBg)
                    .cornerRadius(8)
            }
            .disabled(!isConnected)
            .fileImporter(isPresented: $showFilePicker, allowedContentTypes: [.data]) { result in
                handleFileImport(result)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
    }

    // MARK: - Preview

    private func previewCard(_ image: UIImage) -> some View {
        VStack(spacing: 8) {
            Image(uiImage: image)
                .resizable()
                .scaledToFit()
                .frame(maxHeight: 200)
                .cornerRadius(8)

            if let data = pendingPhotoData {
                Text("\(data.count / 1024) КБ · \(Int(image.size.width))×\(Int(image.size.height))")
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundColor(AppColors.textMuted)

                if data.count > ImageProcessor.maxFileSize {
                    Text("Слишком большой!")
                        .font(.system(size: 12, weight: .bold))
                        .foregroundColor(AppColors.redAccent)
                }
            }

            HStack(spacing: 12) {
                Button("Отправить") { sendPendingPhoto() }
                    .foregroundColor(AppColors.greenAccent)
                    .disabled(pendingPhotoData == nil || (pendingPhotoData?.count ?? 0) > ImageProcessor.maxFileSize)

                Button("Отмена") { cancelPreview() }
                    .foregroundColor(AppColors.redAccent)
            }
        }
        .padding(12)
        .background(AppColors.bgSurface)
        .cornerRadius(10)
        .padding(.horizontal, 12)
    }

    // MARK: - Transfer list

    private var transferList: some View {
        ScrollView {
            LazyVStack(spacing: 2) {
                ForEach(appState.fileTransfers) { transfer in
                    FileTransferRow(transfer: transfer)
                }
            }
            .padding(.horizontal, 12)
            .padding(.top, 8)
        }
    }

    // MARK: - Actions

    private func handleFileImport(_ result: Result<URL, Error>) {
        guard case .success(let url) = result else { return }
        guard url.startAccessingSecurityScopedResource() else { return }
        defer { url.stopAccessingSecurityScopedResource() }

        guard let data = try? Data(contentsOf: url) else { return }
        guard data.count <= ImageProcessor.maxFileSize else { return }

        let name = url.lastPathComponent
        let fileType: Int = name.hasSuffix(".txt") ? 0x02 : 0x03
        controller.sendFile(fileName: name, fileType: fileType, data: data)
    }

    private func sendPendingPhoto() {
        guard let data = pendingPhotoData else { return }
        controller.sendFile(fileName: pendingPhotoName, fileType: 0x01, data: data)
        cancelPreview()
    }

    private func cancelPreview() {
        pendingPhotoData = nil
        previewImage = nil
        showPreview = false
    }
}

// MARK: - UIImagePickerController wrapper (iOS 15 compatible)

struct ImagePickerView: UIViewControllerRepresentable {
    var onImagePicked: (UIImage) -> Void

    func makeUIViewController(context: Context) -> UIImagePickerController {
        let picker = UIImagePickerController()
        picker.sourceType = .photoLibrary
        picker.delegate = context.coordinator
        return picker
    }

    func updateUIViewController(_ uiViewController: UIImagePickerController, context: Context) {}

    func makeCoordinator() -> Coordinator { Coordinator(onImagePicked: onImagePicked) }

    class Coordinator: NSObject, UIImagePickerControllerDelegate, UINavigationControllerDelegate {
        let onImagePicked: (UIImage) -> Void
        init(onImagePicked: @escaping (UIImage) -> Void) { self.onImagePicked = onImagePicked }

        func imagePickerController(_ picker: UIImagePickerController, didFinishPickingMediaWithInfo info: [UIImagePickerController.InfoKey: Any]) {
            if let image = info[.originalImage] as? UIImage {
                onImagePicked(image)
            }
            picker.dismiss(animated: true)
        }

        func imagePickerControllerDidCancel(_ picker: UIImagePickerController) {
            picker.dismiss(animated: true)
        }
    }
}

// MARK: - File Transfer Row

struct FileTransferRow: View {
    let transfer: FileTransfer

    var body: some View {
        HStack(spacing: 8) {
            Text(transfer.isOutgoing ? "→" : "←")
                .font(.system(size: 14, design: .monospaced))
                .foregroundColor(transfer.isOutgoing ? AppColors.blueAccent : AppColors.greenAccent)

            VStack(alignment: .leading, spacing: 2) {
                Text(transfer.fileName)
                    .font(.system(size: 13))
                    .foregroundColor(AppColors.textPrimary)
                    .lineLimit(1)

                HStack(spacing: 4) {
                    Text("\(transfer.totalSize / 1024) КБ")
                    Text("·")
                    Text(fileTypeLabel)
                    Text("·")
                    Text(formatTime)
                }
                .font(.system(size: 11, design: .monospaced))
                .foregroundColor(AppColors.textDim)
            }

            Spacer()

            statusView
        }
        .padding(.vertical, 8)
        .padding(.horizontal, 10)
        .background(AppColors.bgSurface)
        .cornerRadius(8)
    }

    @ViewBuilder
    private var statusView: some View {
        switch transfer.status {
        case .pending, .transferring:
            let pct = transfer.chunksTotal > 0
                ? Double(transfer.chunksDone) / Double(transfer.chunksTotal)
                : 0
            ProgressView(value: pct)
                .progressViewStyle(.circular)
                .frame(width: 24, height: 24)
        case .done:
            Image(systemName: "checkmark.circle.fill")
                .foregroundColor(AppColors.greenAccent)
        case .failed:
            Image(systemName: "xmark.circle.fill")
                .foregroundColor(AppColors.redAccent)
        }
    }

    private var fileTypeLabel: String {
        switch transfer.fileType {
        case 0x01: return "Фото"
        case 0x02: return "Текст"
        default: return "Файл"
        }
    }

    private var formatTime: String {
        let date = Date(timeIntervalSince1970: Double(transfer.timeMs) / 1000)
        let fmt = DateFormatter()
        fmt.dateFormat = "dd.MM HH:mm"
        return fmt.string(from: date)
    }
}
