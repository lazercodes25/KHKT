package com.example.connectesp

import android.animation.ObjectAnimator
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.card.MaterialCardView
import com.google.android.material.snackbar.Snackbar
import okhttp3.*
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import androidx.appcompat.app.AlertDialog
import android.os.CountDownTimer

class RoadListActivity : AppCompatActivity() {

    private val client = OkHttpClient()
    private val espBaseUrl = "http://192.168.4.1" // IP ESP32 mặc định
    private lateinit var layoutRoads: LinearLayout
    private lateinit var rootLayout: LinearLayout

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_road_list)

        layoutRoads = findViewById(R.id.layoutRoads)
        rootLayout = findViewById(R.id.rootLayout)
        loadRoads()
    }

    // 🛰️ Lấy danh sách đường từ ESP32
    private fun loadRoads() {
        EspApiClient.get("/roads") { response ->
            try {
                val json = JSONObject(response)
                val roads = json.getJSONArray("roads")
                showRoadCards(roads)
            } catch (e: Exception) {
                Snackbar.make(rootLayout, "⚠️ Parse lỗi dữ liệu!", Snackbar.LENGTH_SHORT).show()
            }
        }
    }

    // 🧱 Hiển thị các thẻ đường (card đẹp)
    private fun showRoadCards(roads: JSONArray) {
        layoutRoads.removeAllViews()

        for (i in 0 until roads.length()) {
            val road = roads.getJSONObject(i)
            val id = road.getInt("id")
            val name = road.getString("name")
            val status = road.getInt("status")

            // Card container
            val card = MaterialCardView(this).apply {
                radius = 28f
                cardElevation = 6f
                setContentPadding(24, 32, 24, 32)
                useCompatPadding = true
                strokeWidth = 3
                strokeColor = Color.parseColor("#DADCE0")
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    setMargins(0, 16, 0, 0)
                }
                setCardBackgroundColor(
                    if (status == 1) Color.parseColor("#A5D6A7") else Color.parseColor("#FFF59D")
                )
            }

            // Text label
            val tv = TextView(this).apply {
                text = if (status == 1) "✅ $name (Đang mở)" else "🚧 $name (Đang đóng)"
                textSize = 18f
                setTextColor(Color.BLACK)
                gravity = Gravity.CENTER
            }

            card.addView(tv)
            layoutRoads.addView(card)

            // Animation khi nhấn
            card.setOnClickListener {
                ObjectAnimator.ofFloat(card, "alpha", 0.7f, 1f).apply {
                    duration = 200
                    start()
                }
                sendSetRoad(id)
            }
        }
    }
    private fun sendSetRoad(roadId: Int) {
        val json = """{"roadId":$roadId}"""
        EspApiClient.postJson("/set_road", json) { response ->
            try {
                if (response.contains("Another road")) {
                    val obj = JSONObject(response)
                    val timeLeft = obj.optLong("timeLeftMs", 0L)
                    val activeRoadId = obj.optInt("activeRoadId", -1)
                    showWaitingDialog(timeLeft, roadId, activeRoadId)
                } else if (response.contains("\"status\":\"ok\"")) {
                    Snackbar.make(rootLayout, "🚦 Đã chọn đường ID $roadId", Snackbar.LENGTH_SHORT).show()
                    loadRoads()
                } else if (response.startsWith("❌")) {
                    Snackbar.make(rootLayout, response, Snackbar.LENGTH_SHORT).show()
                } else {
                    Snackbar.make(rootLayout, "⚠️ Không xác định: $response", Snackbar.LENGTH_SHORT).show()
                }
            } catch (e: Exception) {
                Snackbar.make(rootLayout, "⚠️ Lỗi xử lý phản hồi!", Snackbar.LENGTH_SHORT).show()
            }
        }
    }
    // 🕒 Hộp thoại chờ đếm ngược khi đang có đường khác mở
    private fun showWaitingDialog(timeLeftMs: Long, retryRoadId: Int, activeRoadId: Int) {
        val seconds = (timeLeftMs / 1000).coerceAtLeast(1)
        var remaining = seconds

        val messageView = TextView(this).apply {
            text = "Đường $activeRoadId đang mở.\nChờ ${remaining}s để thử lại..."
            textSize = 18f
            setPadding(32, 24, 32, 24)
            gravity = Gravity.CENTER
        }

        val dialog = AlertDialog.Builder(this)
            .setTitle("⏳ Vui lòng chờ")
            .setView(messageView)
            .setCancelable(false)
            .create()

        dialog.show()

        // Bộ đếm thời gian
        object : CountDownTimer(timeLeftMs, 1000) {
            override fun onTick(millisUntilFinished: Long) {
                remaining = (millisUntilFinished / 1000)
                messageView.text =
                    "Đường $activeRoadId đang mở.\nChờ ${remaining}s để thử lại..."
            }

            override fun onFinish() {
                dialog.dismiss()
                sendSetRoad(retryRoadId)
            }
        }.start()
    }
}
