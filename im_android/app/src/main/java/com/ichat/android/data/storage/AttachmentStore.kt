package com.ichat.android.data.storage

import android.content.Context
import android.os.Environment
import java.io.File

/**
 * Android 端不再弹出“另存为”，所有下载文件默认进入应用专属下载目录。
 * 该目录不需要额外存储权限，用户卸载应用时系统会自动清理。
 */
class AttachmentStore(private val context: Context) {
    fun downloadRoot(): File {
        val external = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS)
        return File(external ?: context.filesDir, "IChat").apply { mkdirs() }
    }

    fun reserveDownloadFile(fileId: String, fileName: String): File {
        val cleanName = fileName.replace(Regex("[\\\\/:*?\"<>|]"), "_").ifBlank { "file.bin" }
        val root = downloadRoot()
        var candidate = File(root, "${fileId}_$cleanName")
        var index = 1
        while (candidate.exists()) {
            candidate = File(root, "${fileId}_${index}_$cleanName")
            index += 1
        }
        return candidate
    }
}
