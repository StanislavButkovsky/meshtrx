package com.meshtrx.app.ui

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Bundle
import android.util.Log
import android.view.*
import android.widget.*
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.meshtrx.app.*
import com.meshtrx.app.model.*

class FilesFragment : Fragment() {

    private val service: MeshTRXService? get() = (activity as? MainActivity)?.service
    private var pendingPhotoData: ByteArray? = null
    private var pendingPhotoName: String = "photo.jpg"

    // Photo picker
    private val photoPicker = registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        uri ?: return@registerForActivityResult
        val ctx = requireContext()
        // Получить имя файла из URI
        var name = "photo.jpg"
        val cursor = ctx.contentResolver.query(uri, null, null, null, null)
        cursor?.use {
            if (it.moveToFirst()) {
                val idx = it.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME)
                if (idx >= 0) name = it.getString(idx) ?: "photo.jpg"
            }
        }
        val data = ImageProcessor.preparePhoto(ctx, uri)
        if (data != null) {
            pendingPhotoData = data
            pendingPhotoName = name
            showPreview(data)
        } else {
            Toast.makeText(ctx, getString(R.string.photo_failed), Toast.LENGTH_SHORT).show()
        }
    }

    // File picker
    private val filePicker = registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        uri ?: return@registerForActivityResult
        try {
            val ctx = requireContext()
            val input = ctx.contentResolver.openInputStream(uri) ?: return@registerForActivityResult
            val bytes = input.readBytes()
            input.close()

            // Получить имя файла из URI
            var name = "file"
            val cursor = ctx.contentResolver.query(uri, null, null, null, null)
            cursor?.use {
                if (it.moveToFirst()) {
                    val idx = it.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME)
                    if (idx >= 0) name = it.getString(idx) ?: "file"
                }
            }
            if (bytes.size > 100_000) {
                Toast.makeText(ctx, getString(R.string.file_too_big, bytes.size / 1024), Toast.LENGTH_LONG).show()
                return@registerForActivityResult
            }

            val fileType = if (name.endsWith(".txt")) 0x02 else 0x03
            showDestPicker(name, fileType, bytes)
        } catch (e: Exception) {
            Toast.makeText(requireContext(), getString(R.string.error_msg, e.message ?: ""), Toast.LENGTH_SHORT).show()
        }
    }

    private lateinit var layoutPreview: View
    private lateinit var ivPreview: ImageView
    private lateinit var tvPreviewInfo: TextView

    override fun onCreateView(inflater: LayoutInflater, c: ViewGroup?, s: Bundle?): View {
        val v = inflater.inflate(R.layout.fragment_files, c, false)

        val btnSendPhoto = v.findViewById<Button>(R.id.btnSendPhoto)
        val btnSendFile = v.findViewById<Button>(R.id.btnSendFile)
        layoutPreview = v.findViewById(R.id.layoutPreview)
        ivPreview = v.findViewById(R.id.ivPreview)
        tvPreviewInfo = v.findViewById(R.id.tvPreviewInfo)
        val btnSendPreview = v.findViewById<Button>(R.id.btnSendPreview)
        val btnCancelPreview = v.findViewById<Button>(R.id.btnCancelPreview)
        val rvFiles = v.findViewById<RecyclerView>(R.id.rvFiles)
        rvFiles.layoutManager = LinearLayoutManager(requireContext())

        btnSendPhoto.setOnClickListener {
            photoPicker.launch("image/*")
        }

        btnSendFile.setOnClickListener {
            filePicker.launch("*/*")
        }

        btnSendPreview.setOnClickListener {
            val data = pendingPhotoData ?: return@setOnClickListener
            if (data.size > 100_000) {
                Toast.makeText(requireContext(), getString(R.string.photo_too_big, data.size / 1024), Toast.LENGTH_LONG).show()
                return@setOnClickListener
            }
            val name = pendingPhotoName
            pendingPhotoData = null
            pendingPhotoName = "photo.jpg"
            layoutPreview.visibility = View.GONE
            showDestPicker(name, 0x01, data)
        }

        btnCancelPreview.setOnClickListener {
            pendingPhotoData = null
            layoutPreview.visibility = View.GONE
        }

        // Observers
        ServiceState.connectionState.observe(viewLifecycleOwner) { state ->
            val connected = state == BleState.CONNECTED
            btnSendPhoto.isEnabled = connected
            btnSendFile.isEnabled = connected
        }

        ServiceState.fileTransfers.observe(viewLifecycleOwner) { transfers ->
            // Не показывать голосовые (0x04=voice msg, 0x05=PTT voice)
            val filtered = transfers.filter { it.fileType != 0x04 && it.fileType != 0x05 }
            rvFiles.adapter = FileAdapter(filtered)
        }

        return v
    }

    override fun onResume() {
        super.onResume()
        refreshList()
    }

    override fun onHiddenChanged(hidden: Boolean) {
        super.onHiddenChanged(hidden)
        if (!hidden) refreshList()
    }

    private fun refreshList() {
        val transfers = ServiceState.fileTransfers.value ?: emptyList()
        val filtered = transfers.filter { it.fileType != 0x04 && it.fileType != 0x05 }
        view?.findViewById<RecyclerView>(R.id.rvFiles)?.adapter = FileAdapter(filtered)
    }

    private fun showPreview(data: ByteArray) {
        val bitmap = BitmapFactory.decodeByteArray(data, 0, data.size)
        ivPreview.setImageBitmap(bitmap)
        tvPreviewInfo.text = "${data.size / 1024} КБ · ${bitmap?.width ?: 0}×${bitmap?.height ?: 0}"
        layoutPreview.visibility = View.VISIBLE
    }

    private fun showFileDetails(transfer: FileTransfer, position: Int) {
        val ctx = requireContext()
        val builder = androidx.appcompat.app.AlertDialog.Builder(ctx)
        builder.setTitle(transfer.fileName)

        val fileData = transfer.loadData()

        // Custom view: превью + инфо + кнопки
        val layout = LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(48, 24, 48, 16)
        }

        // Превью фото (по типу или расширению)
        val isImage = transfer.fileType == 0x01 ||
            transfer.fileName.lowercase().let { it.endsWith(".jpg") || it.endsWith(".jpeg") || it.endsWith(".png") || it.endsWith(".webp") || it.endsWith(".bmp") }
        if (isImage && fileData != null) {
            val bmp = BitmapFactory.decodeByteArray(fileData, 0, fileData.size)
            if (bmp != null) {
                val displayMetrics = ctx.resources.displayMetrics
                val maxW = (displayMetrics.widthPixels * 0.7).toInt()
                val maxH = (displayMetrics.heightPixels * 0.4).toInt()
                val scale = minOf(maxW.toFloat() / bmp.width, maxH.toFloat() / bmp.height, 1f)
                val w = (bmp.width * scale).toInt()
                val h = (bmp.height * scale).toInt()
                val iv = ImageView(ctx).apply {
                    setImageBitmap(Bitmap.createScaledBitmap(bmp, w, h, true))
                }
                layout.addView(iv, LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { bottomMargin = 24; gravity = android.view.Gravity.CENTER_HORIZONTAL })
            }
        }

        // Инфо
        val typeStr = when (transfer.fileType) {
            0x01 -> getString(R.string.photo); 0x02 -> getString(R.string.text); else -> getString(R.string.file)
        }
        val dir = if (transfer.isOutgoing) getString(R.string.outgoing) else getString(R.string.incoming)
        val time = java.text.SimpleDateFormat("dd.MM.yyyy HH:mm", java.util.Locale.getDefault())
            .format(java.util.Date(transfer.timeMs))
        val hasData = if (fileData != null) "" else "\n⚠ Данные отсутствуют"
        val destInfo = if (transfer.isOutgoing) {
            "\n" + getString(R.string.to, transfer.destName ?: getString(R.string.all))
        } else {
            val fromIdx = transfer.fileName.indexOf(" от ")
            if (fromIdx >= 0) "\nОт: ${transfer.fileName.substring(fromIdx + 4)}" else ""
        }
        val tvInfo = TextView(ctx).apply {
            text = "$dir · $typeStr · ${transfer.totalSize / 1024} КБ\n$time$destInfo$hasData"
            setTextColor(0xFFAAAAAA.toInt())
            textSize = 13f
        }
        layout.addView(tvInfo, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = 24 })

        // Кнопка "Повторить" внутри layout (только исходящие)
        var dialog: androidx.appcompat.app.AlertDialog? = null
        if (transfer.isOutgoing) {
            val btnRetry = Button(ctx).apply {
                text = getString(R.string.retry_send)
                setOnClickListener {
                    if (fileData != null) {
                        retryTransfer(transfer)
                        dialog?.dismiss()
                    } else {
                        Toast.makeText(ctx, getString(R.string.no_data_retry), Toast.LENGTH_SHORT).show()
                    }
                }
            }
            layout.addView(btnRetry, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
            ))
        }

        builder.setView(layout)

        builder.setPositiveButton(getString(R.string.share)) { _, _ ->
            if (fileData != null) shareFile(transfer)
            else Toast.makeText(ctx, getString(R.string.no_data), Toast.LENGTH_SHORT).show()
        }
        builder.setNegativeButton(getString(R.string.delete)) { _, _ ->
            deleteTransfer(position)
        }
        builder.setNeutralButton(getString(R.string.close), null)

        dialog = builder.show()
    }

    private fun shareFile(transfer: FileTransfer) {
        val data = transfer.loadData() ?: return
        try {
            val ctx = requireContext()
            val file = java.io.File(ctx.cacheDir, transfer.fileName)
            file.writeBytes(data)
            val uri = androidx.core.content.FileProvider.getUriForFile(
                ctx, "${ctx.packageName}.fileprovider", file)
            val mime = when (transfer.fileType) {
                0x01 -> "image/jpeg"; 0x02 -> "text/plain"; else -> "application/octet-stream"
            }
            val intent = android.content.Intent(android.content.Intent.ACTION_SEND).apply {
                type = mime
                putExtra(android.content.Intent.EXTRA_STREAM, uri)
                addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            startActivity(android.content.Intent.createChooser(intent, getString(R.string.share)))
        } catch (e: Exception) {
            Toast.makeText(requireContext(), getString(R.string.error_msg, e.message ?: ""), Toast.LENGTH_SHORT).show()
        }
    }

    /** Показать выбор получателя, затем отправить */
    private fun showDestPicker(fileName: String, fileType: Int, data: ByteArray) {
        val sheet = FileDestPickerSheet()
        sheet.showBroadcast = false // файлы — только адресная передача
        sheet.onSelected = { destMac, destName ->
            service?.sendFile(fileName, fileType, data, destMac, destName)
            Toast.makeText(requireContext(), getString(R.string.sending, fileName, destName), Toast.LENGTH_SHORT).show()
        }
        sheet.show(parentFragmentManager, "file_dest")
    }

    // === Adapter ===

    private fun deleteTransfer(position: Int) {
        val list = ServiceState.fileTransfers.value?.toMutableList() ?: return
        if (position in list.indices) {
            val removed = list.removeAt(position)
            service?.deleteFileData(removed.localPath)
            ServiceState.fileTransfers.value = list
            service?.saveFileTransfers()
        }
    }

    private fun retryTransfer(transfer: FileTransfer) {
        val data = transfer.loadData() ?: return
        service?.sendFile(transfer.fileName, transfer.fileType, data, transfer.destMac, transfer.destName)
    }

    inner class FileAdapter(
        private val transfers: List<FileTransfer>
    ) : RecyclerView.Adapter<FileAdapter.VH>() {

        inner class VH(view: View) : RecyclerView.ViewHolder(view) {
            val tvDirection: TextView = view.findViewById(R.id.tvDirection)
            val tvFileName: TextView = view.findViewById(R.id.tvFileName)
            val tvFileInfo: TextView = view.findViewById(R.id.tvFileInfo)
            val progressFile: ProgressBar = view.findViewById(R.id.progressFile)
            val tvFileStatus: TextView = view.findViewById(R.id.tvFileStatus)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            return VH(LayoutInflater.from(parent.context).inflate(R.layout.item_file_transfer, parent, false))
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            val t = transfers[position]
            holder.tvDirection.text = if (t.isOutgoing) "→" else "←"
            holder.tvDirection.setTextColor(if (t.isOutgoing) Colors.blueAccent else Colors.greenAccent)
            holder.tvFileName.text = t.fileName

            val typeStr = when (t.fileType) {
                0x01 -> getString(R.string.photo); 0x02 -> getString(R.string.text); else -> getString(R.string.file)
            }
            val time = java.text.SimpleDateFormat("dd.MM HH:mm", java.util.Locale.getDefault())
                .format(java.util.Date(t.timeMs))
            holder.tvFileInfo.text = "${t.totalSize / 1024} КБ · $typeStr · $time"

            when (t.status) {
                FileStatus.PENDING, FileStatus.TRANSFERRING -> {
                    holder.progressFile.visibility = View.VISIBLE
                    holder.tvFileStatus.visibility = View.GONE
                    val pct = if (t.chunksTotal > 0) t.chunksDone * 100 / t.chunksTotal else 0
                    holder.progressFile.progress = pct
                }
                FileStatus.DONE -> {
                    holder.progressFile.visibility = View.GONE
                    holder.tvFileStatus.visibility = View.VISIBLE
                    holder.tvFileStatus.text = "✓"
                    holder.tvFileStatus.setTextColor(Colors.greenAccent)
                }
                FileStatus.FAILED -> {
                    holder.progressFile.visibility = View.GONE
                    holder.tvFileStatus.visibility = View.VISIBLE
                    holder.tvFileStatus.text = "✗"
                    holder.tvFileStatus.setTextColor(Colors.redAccent)
                }
            }

            // Нажатие на строку — детали
            holder.itemView.setOnClickListener {
                showFileDetails(t, position)
            }
        }

        override fun getItemCount() = transfers.size
    }
}
