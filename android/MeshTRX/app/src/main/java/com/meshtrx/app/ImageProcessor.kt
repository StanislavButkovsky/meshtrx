package com.meshtrx.app

import android.content.Context
import android.graphics.*
import android.net.Uri
import androidx.exifinterface.media.ExifInterface
import java.io.ByteArrayOutputStream

/**
 * Обработка фото для передачи через LoRa.
 * Resize до 300x400, JPEG 60%, без метаданных.
 */
object ImageProcessor {

    fun preparePhoto(context: Context, uri: Uri, quality: Int = 70): ByteArray? {
        return try {
            val input = context.contentResolver.openInputStream(uri) ?: return null
            val original = BitmapFactory.decodeStream(input)
            input.close()
            if (original == null) return null

            // EXIF ориентация
            val exifInput = context.contentResolver.openInputStream(uri)
            val exif = ExifInterface(exifInput!!)
            val orientation = exif.getAttributeInt(ExifInterface.TAG_ORIENTATION, ExifInterface.ORIENTATION_NORMAL)
            exifInput.close()
            val rotated = rotateBitmap(original, orientation)

            // Resize с сохранением пропорций, макс 320x426
            val maxW = 320
            val maxH = 426
            val scale = minOf(maxW.toFloat() / rotated.width, maxH.toFloat() / rotated.height, 1f)
            val w = (rotated.width * scale).toInt()
            val h = (rotated.height * scale).toInt()
            val resized = Bitmap.createScaledBitmap(rotated, w, h, true)

            // JPEG compress — чистый bitmap без EXIF/метаданных
            val out = ByteArrayOutputStream()
            resized.compress(Bitmap.CompressFormat.JPEG, quality, out)

            if (rotated != original) rotated.recycle()
            if (resized != rotated) resized.recycle()
            original.recycle()

            out.toByteArray()
        } catch (e: Exception) {
            null
        }
    }

    private fun rotateBitmap(bitmap: Bitmap, orientation: Int): Bitmap {
        val matrix = Matrix()
        when (orientation) {
            ExifInterface.ORIENTATION_ROTATE_90 -> matrix.postRotate(90f)
            ExifInterface.ORIENTATION_ROTATE_180 -> matrix.postRotate(180f)
            ExifInterface.ORIENTATION_ROTATE_270 -> matrix.postRotate(270f)
            ExifInterface.ORIENTATION_FLIP_HORIZONTAL -> matrix.preScale(-1f, 1f)
            ExifInterface.ORIENTATION_FLIP_VERTICAL -> matrix.preScale(1f, -1f)
            else -> return bitmap
        }
        return Bitmap.createBitmap(bitmap, 0, 0, bitmap.width, bitmap.height, matrix, true)
    }
}
