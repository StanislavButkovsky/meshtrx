import UIKit
import ImageIO

/// Обработка фото для передачи через LoRa.
/// Resize до 320×426, JPEG 70%, без метаданных.
enum ImageProcessor {

    static let maxWidth = 320
    static let maxHeight = 426
    static let defaultQuality: CGFloat = 0.70
    static let maxFileSize = 100_000 // 100 KB

    /// Prepare photo for transmission: resize + compress
    static func preparePhoto(_ image: UIImage, quality: CGFloat = defaultQuality) -> Data? {
        // Fix EXIF orientation
        let oriented = fixOrientation(image)

        // Resize preserving aspect ratio
        let scale = min(
            CGFloat(maxWidth) / oriented.size.width,
            CGFloat(maxHeight) / oriented.size.height,
            1.0
        )
        let newSize = CGSize(
            width: oriented.size.width * scale,
            height: oriented.size.height * scale
        )

        UIGraphicsBeginImageContextWithOptions(newSize, true, 1.0)
        oriented.draw(in: CGRect(origin: .zero, size: newSize))
        let resized = UIGraphicsGetImageFromCurrentImageContext()
        UIGraphicsEndImageContext()

        guard let resized = resized else { return nil }

        // JPEG compress
        return resized.jpegData(compressionQuality: quality)
    }

    /// Fix UIImage orientation to .up
    private static func fixOrientation(_ image: UIImage) -> UIImage {
        guard image.imageOrientation != .up else { return image }

        UIGraphicsBeginImageContextWithOptions(image.size, false, image.scale)
        image.draw(in: CGRect(origin: .zero, size: image.size))
        let normalized = UIGraphicsGetImageFromCurrentImageContext()
        UIGraphicsEndImageContext()

        return normalized ?? image
    }
}
