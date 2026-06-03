package com.ichat.android.data.storage

import android.content.Context
import android.os.Environment
import android.util.Base64
import java.io.File

/**
 * Android 端不再弹出“另存为”，所有下载文件默认进入应用专属下载目录。
 * 该目录不需要额外存储权限，用户卸载应用时系统会自动清理。
 */
class AttachmentStore(private val context: Context) {
    // Media thumbnails live in cache because they are app-owned previews, not user exports.
    fun cacheRoot(): File {
        return File(context.cacheDir, "IChat").apply { mkdirs() }
    }

    fun downloadRoot(): File {
        val external = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS)
        return File(external ?: context.filesDir, "IChat").apply { mkdirs() }
    }

    fun reserveDownloadFile(fileId: String, fileName: String): File {
        val cleanId = cleanFileName(fileId).ifBlank { "file" }
        val cleanName = cleanFileName(fileName).ifBlank { "file.bin" }
        val root = downloadRoot()
        var candidate = File(root, "${cleanId}_$cleanName")
        var index = 1
        while (candidate.exists()) {
            candidate = File(root, "${cleanId}_${index}_$cleanName")
            index += 1
        }
        return candidate
    }

    fun findExistingDownloadFile(fileId: String, fileName: String): File? {
        val cleanId = cleanFileName(fileId).ifBlank { return null }
        val cleanName = cleanFileName(fileName).ifBlank { "file.bin" }
        val root = downloadRoot()
        val preferred = File(root, "${cleanId}_$cleanName")
        if (preferred.exists()) return preferred

        return root.listFiles()
            ?.filter { it.isFile && it.name.startsWith("${cleanId}_") && it.name.endsWith("_$cleanName") }
            ?.maxByOrNull { it.lastModified() }
    }

    // The chat protocol carries image/video preview frames as data URLs; persist them for fast redraw.
    fun saveDataUrlToCache(fileId: String, fileName: String, dataUrl: String): File? {
        val commaIndex = dataUrl.indexOf(',')
        if (commaIndex <= 0) return null

        val cleanId = cleanFileName(fileId).ifBlank { "image" }
        val cleanName = cleanFileName(fileName).ifBlank { "image.jpg" }
        val bytes = Base64.decode(dataUrl.substring(commaIndex + 1), Base64.DEFAULT)
        val target = File(cacheRoot(), "${cleanId}_$cleanName")
        target.writeBytes(bytes)
        return target
    }

    private fun cleanFileName(value: String): String {
        return value.replace(Regex("[\\\\/:*?\"<>|]"), "_")
    }
}
