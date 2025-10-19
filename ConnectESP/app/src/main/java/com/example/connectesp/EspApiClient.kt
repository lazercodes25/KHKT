package com.example.connectesp


import android.os.Handler
import android.os.Looper
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.IOException

object EspApiClient {
    private val client = OkHttpClient()
    private const val BASE_URL = "http://192.168.4.1"

    // NetworkHolder.activeNetwork phải được gán trong lúc kết nối Wi-Fi
    var activeNetwork: android.net.Network? = null

    /**
     * Gửi lệnh POST JSON đến ESP32
     * @param endpoint ví dụ "/api" hoặc "/set_road"
     * @param jsonBody chuỗi JSON
     * @param onResult callback kết quả (String hoặc lỗi)
     */
    fun postJson(endpoint: String, jsonBody: String, onResult: (String) -> Unit) {
        val body = jsonBody.toRequestBody("application/json".toMediaType())
        val request = Request.Builder()
            .url(BASE_URL + endpoint)
            .post(body)
            .build()

        val call = if (activeNetwork != null) {
            client.newBuilder()
                .socketFactory(activeNetwork!!.socketFactory)
                .build()
                .newCall(request)
        } else client.newCall(request)

        call.enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                Handler(Looper.getMainLooper()).post {
                    onResult("❌ Lỗi kết nối: ${e.message}")
                }
            }

            override fun onResponse(call: Call, response: Response) {
                val res = response.body?.string() ?: "(không có phản hồi)"
                Handler(Looper.getMainLooper()).post {
                    onResult(res)
                }
            }
        })
    }

    /**
     * Gửi GET request tới ESP32
     */
    fun get(endpoint: String, onResult: (String) -> Unit) {
        val request = Request.Builder()
            .url(BASE_URL + endpoint)
            .get()
            .build()

        val call = if (activeNetwork != null) {
            client.newBuilder()
                .socketFactory(activeNetwork!!.socketFactory)
                .build()
                .newCall(request)
        } else client.newCall(request)

        call.enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                Handler(Looper.getMainLooper()).post {
                    onResult("❌ Lỗi: ${e.message}")
                }
            }

            override fun onResponse(call: Call, response: Response) {
                val res = response.body?.string() ?: "(no response)"
                Handler(Looper.getMainLooper()).post {
                    onResult(res)
                }
            }
        })
    }
}